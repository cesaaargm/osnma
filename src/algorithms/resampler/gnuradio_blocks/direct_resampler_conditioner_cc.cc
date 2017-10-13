/*!
 * \file direct_resampler_conditioner_cc.cc
 * \brief Nearest neighborhood resampler with
 *        gr_complex input and gr_complex output
 * \author Luis Esteve, 2011. luis(at)epsilon-formacion.com
 *
 * Detailed description of the file here if needed.
 *
 * -------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2015  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------
 */


#include "direct_resampler_conditioner_cc.h"
#include <algorithm>
#include <iostream>
#include <gnuradio/io_signature.h>
#include <glog/logging.h>

using google::LogMessage;

direct_resampler_conditioner_cc_sptr direct_resampler_make_conditioner_cc(
        double sample_freq_in, double sample_freq_out)
{
    return direct_resampler_conditioner_cc_sptr(
            new direct_resampler_conditioner_cc(sample_freq_in,
             sample_freq_out));
}



direct_resampler_conditioner_cc::direct_resampler_conditioner_cc(
        double sample_freq_in, double sample_freq_out) :
            gr::block("direct_resampler_conditioner_cc", gr::io_signature::make(1, 1,
                    sizeof(gr_complex)), gr::io_signature::make(1, 1,
                            sizeof(gr_complex))), d_sample_freq_in(sample_freq_in),
                            d_sample_freq_out(sample_freq_out), d_phase(0), d_lphase(0),
                            d_history(1)
{
    // Computes the phase step multiplying the resampling ratio by 2^32 = 4294967296
    const double two_32 = 4294967296.0;
    if (d_sample_freq_in >= d_sample_freq_out)
        {
            d_phase_step = static_cast<uint32_t>(floor(two_32 * sample_freq_out / sample_freq_in));
        }
    else
        {
            d_phase_step =  static_cast<uint32_t>(floor(two_32 * sample_freq_in / sample_freq_out));
        }
    set_relative_rate(1.0 * sample_freq_out / sample_freq_in);
    set_output_multiple(1);
}




direct_resampler_conditioner_cc::~direct_resampler_conditioner_cc()
{

}



void direct_resampler_conditioner_cc::forecast(int noutput_items,
        gr_vector_int &ninput_items_required)
{
    int nreqd = std::max(static_cast<unsigned>(1), static_cast<int>(static_cast<double>(noutput_items + 1)
            * sample_freq_in() / sample_freq_out()) + history() - 1);
    unsigned ninputs = ninput_items_required.size();
    for (unsigned i = 0; i < ninputs; i++)
        {
            ninput_items_required[i] = nreqd;
        }
}



int direct_resampler_conditioner_cc::general_work(int noutput_items,
        gr_vector_int &ninput_items, gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
{
    const gr_complex *in = reinterpret_cast<const gr_complex *>(input_items[0]);
    gr_complex *out = reinterpret_cast<gr_complex *>(output_items[0]);

    int lcv = 0;
    int count = 0;

    if (d_sample_freq_in >= d_sample_freq_out)
        {
            while ((lcv < noutput_items))
                {
                    if (d_phase <= d_lphase)
                        {
                            out[lcv] = *in;
                            lcv++;
                        }
                    d_lphase = d_phase;
                    d_phase += d_phase_step;
                    in++;
                    count++;
                }
        }
    else
        {
            while ((lcv < noutput_items))
                {
                    d_lphase = d_phase;
                    d_phase += d_phase_step;
                    if (d_phase <= d_lphase)
                        {
                            in++;
                            count++;
                        }
                    out[lcv] = *in;
                    lcv++;
                }
        }

    consume_each(std::min(count, ninput_items[0]));
    return lcv;
}
