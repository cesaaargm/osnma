# Improving Galileo OSNMA Time to First Authenticated Fix

**Authors:**
- **Aleix Galan-Figueras** — Graduate Student Member, IEEE; Katholieke Universiteit Leuven, Leuven, Belgium
- **Ignacio Fernandez-Hernandez** — Katholieke Universiteit Leuven, Leuven, Belgium; European Commission, Brussels, Belgium
- **Wim De Wilde** — Septentrio NV, Leuven, Belgium
- **Sofie Pollin** — Senior Member, IEEE; Katholieke Universiteit Leuven, Leuven, Belgium
- **Gonzalo Seco-Granados** — Fellow, IEEE; Universitat Autònoma de Barcelona, Barcelona, Spain; Institute of Space Studies of Catalonia (IEEC), Castelldefels, Spain

**Published:** IEEE Transactions on Aerospace and Electronic Systems, Vol. 61, No. 5, October 2025
**DOI:** 10.1109/TAES.2025.3570273
**Received:** 18 March 2024 | **Accepted:** 6 May 2025

---

## Abstract

Galileo is the first global navigation satellite system to authenticate its civilian signals through the open service navigation message authentication (OSNMA) protocol. However, OSNMA adds a delay in the time to obtain a first position and time fix, the so-called **time to first authentication fix (TTFAF)**. Reducing the TTFAF as much as possible is crucial to integrate the technology seamlessly into existing products.

In cases where the receiver already has cryptographic data available (the **hot start mode**, the focus of this article), currently available implementations achieve an average TTFAF of around **100 s** in ideal environments.

This work explores TTFAF optimizations applicable to:
- General OSNMA-capable receivers
- Receivers with tighter time synchronization than required by the OSNMA receiver guidelines

Two blocks of optimizations are proposed and benchmarked in three scenarios (open-sky, soft urban, and hard urban) using recorded real data, and also evaluated against official OSNMA test vectors:

1. **Page-level processing** — extracts as much information as possible from broken subframes and combines redundant data from multiple satellites.
2. **COP-IOD optimization** — reconstructs missing navigation data through the intelligent use of COP fields in authentication tags belonging to the same subframe as the authentication key.

**Key results:**
- Average TTFAF of **60.9 s** (test vectors) and **68.8 s** (open-sky)
- Lowest TTFAF of **44.0 s** in both cases
- Urban scenarios show drastic TTFAF reductions compared to the non-optimized case

Optimizations are available as part of the open-source **OSNMAlib** library on GitHub.

---

## I. Introduction

GNSS signals are vulnerable to spoofing (transmission of false GNSS-like signals). Navigation message authentication (NMA) exploits the spoofer's ignorance of cryptographic material. Galileo's NMA protocol, **OSNMA**, is the first deployed civil GNSS authentication system.

**Key context:**
- OSNMA is based on **TESLA** (Timed Efficient Stream Loss-Tolerant Authentication), a delayed disclosure protocol adapted for GNSS.
- TESLA requires an external loose time reference; symmetric encryption tags/keys are shorter than asymmetric signatures but their transmission delays the TTFAF vs. the unauthenticated TTFF.
- Galileo TTFF is typically 30–60 s; I/NAV improvements may reduce it further.

**Prior TTFAF results in the literature:**
| Reference | Average/Lowest TTFAF |
|---|---|
| [14] (with I/NAV improvements) | ~150 s |
| [14] (without I/NAV improvements) | ~170 s |
| [10] | 127 s (lowest comparable case) |
| [9] | 120 s (lowest case) |
| [15] | 90 s |
| **This work** | **44 s (lowest), 60.9 s (avg, test vectors), 68.8 s (avg, open-sky)** |

**Main contributions:**
1. Two TTFAF improvement methods: **page-level processing** (after [19]) and **COP-IOD optimization** (novel).
2. Validation in three real-data scenarios (open-sky, soft urban, hard urban) and official test vectors.
3. Analysis of the OSNMA cross-authentication algorithm and its implications for the COP-IOD optimization.
4. Open-source implementation in OSNMAlib.

---

## II. Galileo I/NAV, OSNMA, and OSNMAlib

### A. Galileo I/NAV and OSNMA

- OSNMA is transmitted in the **I/NAV message, E1-B signal**.
- I/NAV is composed of **30-s subframes** of **15 pages × 2 s each**, each page carrying a word type (WT).
  - WTs 1–5: satellite ephemerides, ionosphere model, health flags
  - WTs 6–10: time parameters (some shared with almanacs)
  - Other WTs: almanacs (WTs 7–9), spare (WT 0)
  - New WTs (recently added): WT 16 (reduced ephemerides), WTs 17–20 (Reed–Solomon page recovery)
- OSNMA is inserted in E1-B as a **40-bit field every 2 s**, split into:
  - **HKROOT** (Header and Root Key): 8 bits
  - **MACK** (Message Authentication Codes and Key): 32 bits

**MACK structure per subframe:**
- 6 truncated MAC tags (40 bits each), each with a 16-bit tag-info encoding satellite number and authentication type
- 1 TESLA key (authenticates the previous subframe's tags)

**ADKD (Authentication Data and Key Delay) types:**
| ADKD | Authenticates | Key delay |
|---|---|---|
| ADKD0 | WTs 1–5 (ephemerides) | Standard (TL = 30 s) |
| ADKD12 | WTs 1–5 | 5 min (relaxed sync requirement) |
| ADKD4 | WTs 6 and 10 (time) | Standard |

**Connected vs. disconnected satellites:**
- Not all satellites transmit OSNMA simultaneously. **Connected** = transmitting OSNMA; **disconnected** = not.
- OSNMA transmits cross-authentication tags for disconnected satellites in positions called **00E** and **FLX** (flex positions, currently used only for ADKD0 cross-authentication).
- There are **3 ADKD0 cross-authentication tag positions** per subframe.

### B. OSNMA Time Synchronization

- OSNMA requires the receiver to know its synchronization accuracy with respect to **Galileo System Time (GST)**.
- The synchronization parameter **TL = 30 s**: time between the last bit of a tag and the first bit of the authenticating TESLA key.
- A receiver unable to guarantee TL cannot use ADKD0 or ADKD4 tags, but may use ADKD12 if sync < TL + 300 s.
- This work also uses tighter synchronizations of **TS = 25 s** and **TS = 17 s** for specific optimizations.

### C. OSNMAlib

**OSNMAlib** ([GitHub](https://github.com/Algafix/OSNMA)) is an open-source Python library implementing the OSNMA protocol. It:
- Reads Galileo I/NAV pages, stores navigation and authentication data, performs verification, reports status.
- Supports cold start, warm start, and hot start.
- Required input: navigation data bits from E1-B I/NAV, GST of page transmission, SVID.

**Supported input modules:**
1. Septentrio SBF (postprocess or live, via `GALRawINAV` block)
2. u-blox UBX (postprocess or live, via `UBX-RXM-SFRBX`)
3. GNSS-SDR (via UDP socket)
4. Galmon network (aggregated multi-receiver data)
5. Android GnssLogger App (postprocess log files)

**Outputs:** OSNMA data received, verification events, authenticated navigation data in chronological order; JSON status logs per subframe (used in the OSNMAlib web monitoring page).

---

## III. Proposed TTFAF Optimizations

**Standard TTFF requires:** acquisition, tracking, ephemeris decoding (WTs 1–5) from ≥4 satellites, time (WTs 6 and 10) from ≥1 satellite.
**TTFAF additionally requires:** authentication tags for the above + a TESLA key in the next subframe → introduces a delay.

**Hot start TTFAF** (focus of this paper): cryptographic bootstrap data (root key) already available. The root key lasts several months.
- **Warm start:** public key available but root key must be retrieved from navigation data.
- **Cold start:** only Merkle Tree root hash available; public key transmitted every 6 h.

### A. Page-Level Tag and Key Processing

OSNMA can be optimized below the subframe level since subframes = 15 × 2 s pages. Discarding an entire subframe because one page was missed is suboptimal.

**Two ideas:**

1. **Extract tags from partially corrupted subframes.** Tags in correctly received pages of a broken subframe are still valid, provided their order within the subframe can still be verified via the MAC lookup table or MACSEQ value. Caveat: if any flex tag or its MACSEQ is missing, all flex tags in that subframe are lost.

2. **Reconstruct the TESLA key from multiple satellites.** All Galileo satellites transmit the same TESLA key per subframe. If no single satellite provided a complete key, it can be reconstructed by combining non-overlapping correct pages from different satellites.

**Example:** Satellite 04 loses last pages → first 4 tags still usable. Satellite 10 loses a flex-tag page → 4 non-flex tags still usable. Satellites 10 and 27 each miss a different key page → key reconstructed from both.

This optimization is most impactful in urban/fading environments.

### B. Issue of Data (IOD) Navigation Data Link

**Standard ADKD0 authentication flow:**
1. Receive navigation data in subframe SF_j
2. Receive corresponding tags in SF_{j+1}
3. Receive authenticating TESLA key in SF_{j+2}

→ Minimum TTFAF: **90 s**; maximum (if receiver misses start): **119 s**

**IOD optimization:** Ephemerides change slowly and may be identical across multiple subframes. If the IOD (Issue of Data) value in a subframe's I/NAV words matches that of a later subframe, data from the later subframe can fill in missing words from the earlier one.

- WT 5 has no IOD and is assigned based on other words' IOD.
- Lowest case: receiver starts just before WT 3 (last word with subframe IOD, transmitted 8 s before subframe end) → lowest TTFAF: **60 s**
- Worst case (receiver starts just after WT 3): **97 s**

### C. Cutoff Point (COP) Tag-Data Link

The **4-bit COP field** (replacing the former truncated-IOD field) indicates for how many subframes the authenticated navigation data have not changed:
- COP = 1: data valid only from the immediately previous subframe.
- COP = 15: data unchanged for the 15 previous subframes.

**Novel use of COP:** Instead of using COP only to link the tag's own subframe to prior data, use the COP value of tags in the *key subframe* (SF_{j+2}) to verify that the data from SF_j (the receiver's first subframe) matches the data authenticated by the tags in SF_{j+1}.

**Operating procedure (COP + IOD combined):**
1. Receiver powers up mid-SF_j, receives WTs 1, 3, 5 and a few cross-authentication tags.
2. At end of SF_j, cross-auth tags correspond to SF_{j-1} (missed by receiver).
3. During SF_{j+1}, receiver gets all WTs. If IOD matches SF_j's IOD, partial data from SF_j is reconstructed.
4. Receiver checks COP of tags in SF_{j+1}: if COP > 1, data from SF_j is the same as SF_{j-1}, linking the SF_j tags to the reconstructed data.
5. Receiver now has: navigation data (SF_{j-1} = SF_j), tags (SF_j), TESLA key (SF_{j+1}) → can authenticate.

**TTFAF results with COP + IOD:**
- Lowest case: **44 s** (even subframes) or **46 s** (odd subframes)
- Worst case (receiver starts just after last cross-auth tag): **73 s**

---

## IV. Further Considerations

### A. Tighter Time Synchronization Requirements

| Optimization | Required TS |
|---|---|
| None (standard) | TL = 30 s |
| IOD optimization (full potential) | 25 s |
| COP + IOD optimization | 17 s |

- TS = 25 s corresponds to the time between the last bit of WT 5 (in the tag subframe) and the first bit of the TESLA key.
- TS = 17 s corresponds to the time between the last bit of WT 5 (in the *key* subframe) and the first bit of the TESLA key.
- WT 1 transmitted simultaneously with the key → discarded (TS would need to be ~1 s).

The receiver must specify TS before execution; OSNMAlib does not support changing TS mid-run.

### B. Optimization Theoretical Improvement

**IOD optimization success rate** (from 24 h of open-sky data):
- Navigation data unchanged for ≥600 s (20 subframes) the **majority of the time**

| Metric | Value |
|---|---|
| Probability of IOD optimization working for a given satellite on any given subframe | 96.28% |
| Probability of OSNMA receiver being able to use optimization on any given subframe | 97.78% |

### C. Acceptable Forgeries (Security Consideration)

The COP-IOD optimization uses unauthenticated COP values. A potential attack:
- Adversary modifies the COP value of the current subframe tag and replays previous subframe navigation data.
- Authentication passes (tag is genuine, navigation data is valid — just 30 s older).

**Why this is acceptable:**
- Navigation data has a validity of **4 h** per Galileo System Definition Document — 30 s mismatch is negligible.
- The adversary cannot forge the tag itself; the data content is correct.
- The forgery is detected **30 s later** when the COP-modified tag is itself authenticated.
- The receiver could use the data safely for the full 4 h validity period even if the adversary jams further reception.
- Mitigation: time-stamp data relative to the first subframe in which it was actually received.
- Attack requires real-time signal replay + modification, detectable by partial-correlation anti-replay techniques.
- The attack is only possible at protocol startup or after long interruptions, not during continuous authentication.

---

## V. Test Scenarios

**Equipment:** Septentrio mosaic-X5 (firmware 4.14.0) for dynamic scenarios; Septentrio PolaRx5TR (firmware 5.5.0) for static. Data saved in SBF format (`GalRawINAV` block).

### A. Hard Urban — Brussels, European District
- **Date/time:** 3 December 2023, 09:50:00–10:22:30 UTC (GST 1267 35400–37350)
- **Trajectory:** Parc de Bruxelles → Rue Belliart → Rue de Trèves → Rue de la Loi → back to park
- **Satellites:** 8 tracked; SVID 5 not transmitting OSNMA; SVID 31 initially disconnected
- **Characteristics:** Highly volatile tracking; several entirely lost subframes; urban canyon environment

### B. Soft Urban — Brussels, Atomium and Laeken Parks
- **Date/time:** 3 December 2023, 11:03:24–11:43:53 UTC (GST 1267 39804–42233)
- **Trajectory:** Around Atomium, Osseghem Park, Laeken Park
- **Satellites:** 9 tracked; balanced connected/disconnected; high variability in which satellites transmit OSNMA

### C. Open-Sky — Leuven, Septentrio Offices
- **Date/time:** 20 December 2023, 15:00:00–16:00:00 UTC (GST 1269 313200–316800)
- **Satellites:** 11 tracked (SVID 31 briefly below horizon); all satellites cycle between connected and disconnected; always ≥4 disconnected

### D. Test Vectors — Configuration 2
- **Simulation period:** 26 July 00:29:43 to 27 July 00:29:43 UTC (GST 1248 345601–347401), first 30 min processed
- **Characteristics:** 25 synthetic Galileo satellites (impossible in live recording); perfect reception with no pages lost; same tag sequence as operational live data
- **Note:** Must be chronologically sorted before OSNMAlib can process them

---

## VI. Test Results

**Methodology:** Recordings were replayed in OSNMAlib, starting 1 s later each iteration to emulate a receiver powering up at any point in time. Number of TTFAF data points = number of seconds in each scenario.

**Three accumulative optimization groups:**
1. **Standard OSNMA:** IOD optimization, TS = TL = 30 s (used as baseline)
2. **Page-level + tighter sync:** IOD optimization, TS = 25 s, page-level processing
3. **COP-IOD + page-level + tighter sync:** COP-IOD optimization, TS = 17 s, page-level processing

### TTFAF Results Summary

**Table II — IOD Data Link Optimization, TS = 30 s**

| Scenario | Lowest (s) | Average (s) | P95 (s) |
|---|---|---|---|
| Test Vectors | 68.0 | 82.5 | 97.0 |
| Open-Sky | 68.0 | 82.5 | 97.0 |
| Soft Urban | — | 127.5 | — |
| Hard Urban | — | 266.1 | — |

**Table III — IOD Optimization, TS = 25 s + Page-Level Processing**

| Scenario | Lowest (s) | Average (s) | P95 (s) |
|---|---|---|---|
| Test Vectors | 66.0 | 80.5 | — |
| Open-Sky | 66.0 | 80.5 | — |
| Soft Urban | — | 94.1 | — |
| Hard Urban | — | 151.1 | — |

**Table IV — COP-IOD Optimization, TS = 17 s + Page-Level Processing**

| Scenario | Lowest (s) | Average (s) | P95 (s) |
|---|---|---|---|
| Test Vectors | **44.0** | **60.9** | — |
| Open-Sky | **44.0** | **68.8** | — |
| Soft Urban | — | 87.5 | — |
| Hard Urban | — | 146.1 | — |

### A. Page-Level Processing Results

- **Urban scenarios:** Clear improvement. Hard Urban: ~80% of TTFAF values < 200 s with page-level (vs. 360 s worst case without). More effective in Hard Urban than Soft Urban.
- **Open-Sky / Test Vectors:** The 2 s improvement observed is due to TS reduction to 25 s (allowing IOD link using WT 3 instead of WT 1), not from page-level processing itself.
- **Test Vectors:** No improvement expected (synthetic, no pages lost).

### B. COP-IOD Results

- **Urban scenarios:** Minimal improvement due to the requirement of two consecutive tags for the same satellite, rarely met under fading. Slightly more effective in Soft than Hard Urban.
- **Test Vectors:** Works as theorized. TTFAF alternates between 44 and 46 s per subframe (determined by even/odd subframe and the position of the last cross-auth tag 00E). Subframes with navigation data changes show 60 s minimum.
- **Open-Sky:** Works well but not as consistently as theorized. Discrete TTFAF values observed: **60, 54, 46, 44 s** — directly linked to positions of ADKD0 tags in the sequence.

### C. OSNMA Cross-Authentication Algorithm Impact

**Tag imbalance between connected and disconnected satellites:**
- A **connected** satellite receives only **1 ADKD0 tag per subframe** (its own self-authentication tag, position 00S — first in sequence).
- A **disconnected** satellite receives **up to 5 ADKD0 tags per subframe** from cross-authenticating connected satellites.

**Impact on COP-IOD optimization:**

| Constellation | Minimum TTFAF (COP-IOD) | Average TTFAF (COP-IOD) |
|---|---|---|
| 4 connected, 0 disconnected in view | 60 s | 74.5 s |
| 2 connected, 4 disconnected in view | 54 s (odd subframes only) | 71.5 s |
| 4 connected, 4 disconnected in view | **44 s** | **59.5 s** |

With only connected satellites, the single self-auth tag is always in position 00S (first in subframe). Missing just 2 s of the subframe means no tags for that satellite for the remaining 28 s. With 4+ disconnected satellites, cross-auth tags appear later in the sequence, allowing the receiver to start later and still collect tags — paradoxically, the receiver authenticates using satellites that are *not* transmitting OSNMA.

---

## VII. Conclusion

Two concrete ideas improve the TTFAF: **page-level processing** and **COP-IOD optimization**. They are complementary:

| Scenario | Page-Level Best For | COP-IOD Best For |
|---|---|---|
| Urban | ✓ Very effective (fading/lost pages) | ✗ Ineffective (few consecutive tags) |
| Open-Sky / Test Vectors | ✗ No effect (no pages lost) | ✓ Very effective (good visibility) |

**Quantitative summary:**
- Page-level processing: avg TTFAF from 127.5 → 94.1 s (Soft Urban); 266.1 → 151.1 s (Hard Urban)
- COP-IOD: avg TTFAF from 82.5 → **60.9 s** (test vectors); → **68.8 s** (open-sky)
- Lowest TTFAF: from 68.0 → **44.0 s** (both test vectors and open-sky)

**Limitation identified:** The OSNMA cross-authentication algorithm never sends cross-auth tags for connected satellites, creating a tag imbalance that constrains the COP-IOD optimization's effectiveness. Transmitting 00S in the last position of the sequence, or cross-authenticating connected satellites, could further improve performance.

**Future directions:**
- Multifrequency library using E5b-I I/NAV messages
- Integration of the 4 new Galileo WTs (Reed–Solomon clock and ephemeris recovery) for urban scenario improvements

---

## References

| # | Citation |
|---|---|
| [1] | L. Scott, "Anti-spoofing & authenticated signal architectures for civil navigation systems," ION GNSS, 2003 |
| [2] | K. D. Wesson et al., "GNSS signal authentication via power and distortion monitoring," IEEE TAES, vol. 54, no. 2, 2018 |
| [3] | Ç. Tanil et al., "An INS monitor to detect GNSS spoofers," IEEE TAES, vol. 54, no. 1, 2018 |
| [4] | J. M. Anderson et al., "CHIMERA for GPS civilian signals," ION GNSS, 2017 |
| [5] | I. Fernandez-Hernandez et al., "Semi-assisted signal authentication for Galileo," IEEE TAES, vol. 59, no. 4, 2023 |
| [6] | K. Zhang et al., "Protecting GNSS NMA against distance-decreasing attacks," IEEE TAES, vol. 58, no. 2, 2021 |
| [7] | T. E. Humphreys, "Detection strategy for cryptographic GNSS anti-spoofing," IEEE TAES, vol. 49, no. 2, 2013 |
| [8] | I. Fernandez-Hernandez et al., "A NMA proposal for Galileo open service," NAVIGATION, vol. 63, no. 1, 2016 |
| [9] | M. Götzelmann et al., "Galileo OSNMA: Preparation phase and future service provision," NAVIGATION, vol. 70, no. 3, 2023 |
| [10] | L. Musumeci et al., "OSNMA user performance assessment at ESA/ESTEC," ION GNSS, 2023 |
| [11] | A. Perrig et al., "TESLA broadcast authentication," Secure Broadcast Communication, Springer, 2003 |
| [12] | I. Fernandez-Hernandez et al., "Analysis and recommendations for MAC and key lengths in delayed disclosure GNSS," IEEE TAES, vol. 57, no. 3, 2021 |
| [13] | M. Paonni et al., "Improving Galileo E1-OS by optimizing the I/NAV navigation message," ION GNSS, 2019 |
| [14] | L. Cucchi et al., "Receiver testing for Galileo E1 OSNMA and I/NAV improvements," ION GNSS, 2022 |
| [15] | T. Hammarberg et al., "An experimental performance assessment of Galileo OSNMA," Sensors, vol. 24, no. 2, 2024 |
| [16] | A. Galan et al., "OSNMAlib. GitHub repository," 2024. https://github.com/Algafix/OSNMA |
| [17] | Septentrio N.V., "Mosaic-X5 GNSS receiver module," 2024 |
| [18] | Septentrio N.V., "PolaRx5TR GNSS receiver," 2024 |
| [19] | S. Damy et al., "Performance assessment of Galileo OSNMA data retrieval strategies," Satell. Navig. Technol., 2022 |
| [20] | Galileo OS SIS ICD, Issue 2.1, European Union, Nov. 2023 |
| [21] | I. Fernandez-Hernandez et al., "Galileo authentication and high accuracy: Getting to the truth," Inside GNSS, Feb. 2023 |
| [22] | Galileo OSNMA SIS ICD, Issue 1.1, European Union, Oct. 2023 |
| [23] | I. Fernandez-Hernandez et al., "Independent time synchronization for resilient GNSS receivers," ION ITM, 2020 |
| [24] | OSNMA Receiver Galileo Guidelines, Issue 1.3, European Union, Jan. 2024 |
| [25] | A. Galan et al., "OSNMAlib: An open Python library for Galileo OSNMA," Workshop Satell. Navig. Technol., 2022 |
| [26] | C. Fernández-Prades, "GNSS-SDR. CTTC. Open-source GNSS software-defined receiver," 2024. https://gnss-sdr.org |
| [27] | B. Hubert, "Galmon network. GitHub repository," 2024 |
| [28] | A. Galan-Figueras et al., "Improving OSNMAlib: New Formats, Features, and Monitoring Capabilities," IEEE J. Indoor Seamless Position. Navigation, vol. 3, 2025 |
| [29] | S. Damy et al., "Impact of OSNMA configurations, operations and user's strategies on receiver performances," ION GNSS, 2022 |
| [30] | I. Fernández et al., Galileo NMA Specification for SIS Testing v1.0, European Commission, 2016 |
| [31] | Galileo OS Service Definition Document, Issue 1.3, European Union, Nov. 2023 |
| [32] | G. Seco-Granados et al., "Detection of replay attacks to GNSS based on partial correlations," GPS Solutions, vol. 25, 2021 |
| [33] | A. Galan et al., "GNSS recordings for Galileo OSNMA evaluation," IEEE Dataport, 2024. doi:10.21227/a0nm-kn45 |
| [34] | A. Galan et al., "Sensitivity analysis of Galileo OSNMA cross-authentication sequences," Eng. Proc., vol. 88, no. 1, 2025 |
| [35] | S. Damy et al., "Increasing OSNMA performance with Galileo I/NAV improvements," ION ITM, 2024 |
