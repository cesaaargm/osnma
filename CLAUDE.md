# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Repository Is

This is **GNSS-SDR**, a software-defined GNSS receiver built on GNU Radio. It processes signals from GPS, Galileo, GLONASS, and BeiDou to compute position fixes. This fork/branch focuses on **Galileo OSNMA** (Open Service Navigation Message Authentication), a cryptographic authentication mechanism for Galileo E1B navigation data.

## Build Commands

```bash
# Standard build (Release by default)
cd build
cmake ..
make -j$(nproc)

# Debug build (enables more logging)
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Build with specific options (e.g., enable OSMOSDR front-end)
cmake -DENABLE_OSMOSDR=ON ..
make -j$(nproc)
```

Build outputs land in `install/`: `gnss-sdr` (receiver), `run_tests` (test suite), `front-end-cal`, `volk_gnsssdr_profile`.

## Running Tests

```bash
# Run all tests
./install/run_tests

# Run only OSNMA-related tests
./install/run_tests --gtest_filter="OsnmaMsgReceiverTest*"
./install/run_tests --gtest_filter="OsnmaTestVectors*"

# Run a single named test
./install/run_tests --gtest_filter="OsnmaMsgReceiverTest.TeslaKeyVerification"
```

OSNMA test files: `src/tests/unit-tests/signal-processing-blocks/osnma/`

## Code Style

The project uses `clang-format` with a Google-based style (config in `.clang-format`) and `clang-tidy` (config in `.clang-tidy`). Enable clang-tidy during build with `-DENABLE_CLANG_TIDY=ON`.

## Architecture Overview

The receiver is structured as a **GNU Radio flowgraph** controlled by a **control plane**:

### Control Plane (`src/core/receiver/`)
- `ControlThread` — main application thread; reads control messages, applies actions to the flowgraph.
- `GNSSFlowgraph` — owns and connects all GNU Radio blocks: Signal Source → Signal Conditioner → Channels → Observables → PVT. Also hosts the OSNMA receiver block.
- `GnssBlockFactory` — instantiates all processing blocks from configuration.
- `Concurrent_Queue<T>` — thread-safe queue used to pass control events between blocks and the control thread.
- Configuration is read from `.conf` files (examples in `conf/`) via `ConfigurationInterface`.

### Signal Processing Plane (`src/algorithms/`)
Each algorithm has three sub-layers mirroring GNU Radio conventions:
- `adapters/` — glue between the GNSS-SDR interface and the GNU Radio block.
- `gnuradio_blocks/` — the actual GNU Radio block implementation.
- `libs/` — reusable logic (e.g., correlators, navigation message parsers).

Processing stages per channel: **Acquisition → Tracking → Telemetry Decoder**. All channels feed into the **Observables** block, which feeds **PVT**.

### OSNMA Subsystem (`src/core/libs/` and `src/core/system_parameters/`)

`osnma_msg_receiver` is a GNU Radio block with no stream I/O — it operates purely via asynchronous message ports:
- **Input port** `OSNMA_from_TLM`: receives `std::shared_ptr<OSNMA_msg>` from every Galileo E1B telemetry decoder channel.
- **Output port** `OSNMA_to_PVT`: sends verified `OSNMA_NavData` to the PVT block.

OSNMA is enabled by having at least one `Channels_1B` channel and setting `GNSS-SDR.osnma_enable=true` in the conf file. Key conf properties:
- `GNSS-SDR.osnma_public_key` — path to `.crt` public key file (default: `./OSNMA_PublicKey_*.crt`)
- `GNSS-SDR.osnma_merkletree` — path to Merkle tree `.xml` file (default: `./OSNMA_MerkleTree_*.xml`)
- `GNSS-SDR.osnma_mode` — set to `"strict"` to enforce wall-clock time checks

#### Key Data Structures in `osnma_msg_receiver`

| Member | Type | Purpose |
|---|---|---|
| `d_satellite_nav_data` | `std::map<uint32_t, std::map<uint32_t, OSNMA_NavData>>` | Navigation data keyed by `[SVID][TOW]` |
| `d_tesla_keys` | `std::map<uint32_t, std::vector<uint8_t>>` | TESLA keys keyed by TOW |
| `d_tags_awaiting_verify` | `std::multimap<uint32_t, Tag>` | Pending authentication tags keyed by TOW |
| `d_dsm_message` | `std::array<std::array<uint8_t, 256>, 16>` | Buffer for incoming DSM blocks (16 DSM IDs × 256 bytes) |
| `d_mack_message` | `std::array<uint8_t, 60>` | Current MACK message buffer (480 bits) |

#### Supporting Classes

- **`OSNMA_data`** (`osnma_data.h`) — aggregate holding the parsed NMA header, DSM header, DSM-PKR message, DSM-KROOT message, MACK message, and one `OSNMA_NavData` instance.
- **`OSNMA_NavDataManager`** (`osnma_nav_data_manager.h`) — manages `std::map<uint32_t, std::map<uint32_t, OSNMA_NavData>>` sorted by `[PRNd][TOW_start]`; handles adding, querying, and marking nav data as verified.
- **`OSNMA_DSM_Reader`** (`osnma_dsm_reader.h`) — stateless bit-mask parser for DSM-KROOT and DSM-PKR fields from raw byte vectors.
- **`Gnss_Crypto`** (`gnss_crypto.h`) — wraps OpenSSL/GnuTLS; handles ECDSA signature verification, HMAC-SHA256/512 for TESLA key chain, and Merkle tree verification.
- **`Osnma_Helper`** (`osnma_helper.h`) — stateless utilities: GST ↔ WN/TOW conversion, byte/hex/binary string conversions.
- **`Tag`** (`osnma_data.h`) — represents one authentication tag with its `PRNa`, `PRN_d`, `ADKD`, `TOW`, TESLA key slot, received and computed tag values, and a verification status enum (`SUCCESS`/`FAIL`/`UNVERIFIED`).

#### OSNMA Processing Flow

1. Telemetry decoder extracts OSNMA bits from Galileo E1B word 6 and sends `OSNMA_msg` via `OSNMA_from_TLM`.
2. `msg_handler_osnma` dispatches to `process_osnma_message`.
3. NMA header and DSM header are parsed; DSM blocks are buffered into `d_dsm_message` until complete.
4. A complete DSM triggers `process_dsm_message` → authenticates DSM-KROOT against the Merkle root or verifies DSM-PKR public key.
5. MACK blocks are buffered in `d_mack_message`; once complete, `process_mack_message` extracts Tag0 and tag-and-info fields, adds them to `d_tags_awaiting_verify`, and verifies MACSEQ.
6. TESLA key chain: each subframe's key is verified by hashing forward to the previously verified key. Verified keys stored in `d_tesla_keys`.
7. Tags in `d_tags_awaiting_verify` are verified against the matching TESLA key and the corresponding nav data in `d_satellite_nav_data`/`OSNMA_NavDataManager`.
8. Verified nav data is sent to PVT via `OSNMA_to_PVT`.

Hot-start is supported: if a public key (`.crt`) and a saved KROOT binary (`OSNMA_DSM_KROOT_NMAHeader.bin`) are present at startup, the chain verification begins immediately without waiting for a full DSM-KROOT reception.

#### Constants (`src/core/system_parameters/Galileo_OSNMA.h`)

ICD tables (hash functions, key sizes, MAC lookup tables, ADKD definitions) are stored as `const std::unordered_map` globals. Default file paths:
```
CRTFILE_DEFAULT   = "./OSNMA_PublicKey_<date>_newPKID_1.crt"
MERKLEFILE_DEFAULT = "./OSNMA_MerkleTree_<date>_newPKID_1.xml"
KROOTFILE_DEFAULT  = "./OSNMA_DSM_KROOT_NMAHeader.bin"
```
