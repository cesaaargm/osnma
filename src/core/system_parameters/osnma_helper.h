/*!
* \file osnma_helper.h
* \brief Class for auxiliary osnma functions
* \author Carles Fernandez-Prades, 2024 cfernandez(at)cttc.es
*
* -----------------------------------------------------------------------------
*
* GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
* This file is part of GNSS-SDR.
*
* Copyright (C) 2010-2023  (see AUTHORS file for a list of contributors)
* SPDX-License-Identifier: GPL-3.0-or-later
*
* -----------------------------------------------------------------------------
*/

#ifndef GNSS_SDR_OSNMA_HELPER_H
#define GNSS_SDR_OSNMA_HELPER_H


#include <cstdint>
#include <string>
#include <vector>
class Osnma_Helper
{
public:
    Osnma_Helper() = default;
    ~Osnma_Helper() = default;
    uint32_t compute_gst(uint32_t WN, uint32_t TOW) const;
    std::vector<uint8_t> gst_to_uint8(uint32_t GST) const;
    std::vector<uint8_t> bytes(const std::string& binaryString) const;
    std::string verification_status_str(int status) const;
    std::string convert_to_hex_string(const std::vector<uint8_t>& vector) const ;
    std::vector<uint8_t> convert_from_hex_string(const std::string& hex_string) const; // TODO remove similar function in gnss_crypto
};

#endif  // GNSS_SDR_OSNMA_HELPER_H
