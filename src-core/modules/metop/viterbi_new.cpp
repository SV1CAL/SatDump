#include "viterbi_new.h"
#include <cstring>
#include "logger.h"

#define ST_IDLE 0
#define ST_SYNCED 1

namespace metop
{
    MetopViterbi2::MetopViterbi2(float ber_threshold, int outsync_after, int buffer_size) : d_ber_thresold(ber_threshold),
                                                                                            d_outsync_after(outsync_after),
                                                                                            d_state(ST_IDLE),
                                                                                            d_outsinc(0),
                                                                                            d_buffer_size(buffer_size),
                                                                                            d_first(true),
                                                                                            cc_decoder_in((d_buffer_size / 2.0f) * 1.5f, 7, 2, {79, 109}, 0, -1, CC_STREAMING, false),
                                                                                            cc_decoder_in_ber((TEST_BITS_LENGTH * 1.5f) / 2.0f, 7, 2, {79, 109}, 0, -1, CC_STREAMING, false),
                                                                                            cc_encoder_in_ber((TEST_BITS_LENGTH * 1.5f) / 2.0f, 7, 2, {79, 109}, 0, CC_STREAMING, false),
                                                                                            //depunc_ber(3, 110),
                                                                                            depunc(3, 110)
    {
        fixed_soft_packet = new uint8_t[buffer_size];
        converted_buffer = new uint8_t[buffer_size];
        reorg_buffer = new uint8_t[buffer_size];
        depunc_buffer = new uint8_t[buffer_size * 2];
        output_buffer = new uint8_t[buffer_size * 2];
    }

    MetopViterbi2::~MetopViterbi2()
    {
        delete[] fixed_soft_packet;
        delete[] converted_buffer;
        delete[] reorg_buffer;
        delete[] depunc_buffer;
        delete[] output_buffer;
    }

    float MetopViterbi2::getBER(uint8_t *input)
    {
        char_array_to_uchar((char *)input, d_ber_input_buffer, TEST_BITS_LENGTH);

        for (int i = 0; i < TEST_BITS_LENGTH / 4; i++)
        {
            d_ber_input_reorg_buffer[i * 4 + 0] = d_ber_input_buffer[i * 4 + 0];
            d_ber_input_reorg_buffer[i * 4 + 1] = d_ber_input_buffer[i * 4 + 1];
            d_ber_input_reorg_buffer[i * 4 + 2] = d_ber_input_buffer[i * 4 + 3];
            d_ber_input_reorg_buffer[i * 4 + 3] = d_ber_input_buffer[i * 4 + 2];
        }

        gr::fec::depuncture_bb_impl depunc_ber(3, 110);
        depunc_ber.general_work(TEST_BITS_LENGTH, d_ber_input_reorg_buffer, d_ber_input_buffer_depunc);

        cc_decoder_in_ber.generic_work(d_ber_input_buffer_depunc, d_ber_decoded_buffer);
        cc_encoder_in_ber.generic_work(d_ber_decoded_buffer, d_ber_encoded_buffer);

        float errors = 0;
        for (int i = 0; i < TEST_BITS_LENGTH * 1.5; i++)
            errors += (d_ber_input_buffer_depunc[i] > 0) != (d_ber_encoded_buffer[i] > 0);

        return (errors / ((float)TEST_BITS_LENGTH * 1.5f * 2.0f)) * 2.0f;
    }

    int MetopViterbi2::work(uint8_t *input, size_t size, uint8_t *output)
    {
        int data_size_out = 0;

        switch (d_state)
        {
        case ST_IDLE:
        {
            // Test without IQ Inversion
            for (int ph = 0; ph < 2; ph++)
            {
                std::memcpy(d_ber_test_buffer, input, TEST_BITS_LENGTH);
                phaseShifter.fixPacket(d_ber_test_buffer, TEST_BITS_LENGTH, (sathelper::PhaseShift)ph, false);
                d_bers[0][ph] = getBER(d_ber_test_buffer);
            }

            // Test with IQ Inversion
            for (int ph = 0; ph < 2; ph++)
            {
                std::memcpy(d_ber_test_buffer, input, TEST_BITS_LENGTH);
                phaseShifter.fixPacket(d_ber_test_buffer, TEST_BITS_LENGTH, (sathelper::PhaseShift)ph, true);
                d_bers[1][ph] = getBER(d_ber_test_buffer);
            }

            for (int s = 0; s < 2; s++)
            {
                for (int p = 0; p < 2; p++)
                {
                    if (d_ber_thresold > d_bers[s][p])
                    {
                        d_ber = d_bers[s][p];
                        d_iq_inv = s;
                        d_phase_shift = (sathelper::PhaseShift)p;
                        d_state = ST_SYNCED;
                    }
                }
            }
        }
        break;

        case ST_SYNCED:
        {
            // Decode
            std::memcpy(fixed_soft_packet, input, size);
            phaseShifter.fixPacket(fixed_soft_packet, size, d_phase_shift, d_iq_inv);

            char_array_to_uchar((char *)fixed_soft_packet, converted_buffer, size);

            for (int i = 0; i < size / 4; i++)
            {
                reorg_buffer[i * 4 + 0] = converted_buffer[i * 4 + 0];
                reorg_buffer[i * 4 + 1] = converted_buffer[i * 4 + 1];
                reorg_buffer[i * 4 + 2] = converted_buffer[i * 4 + 3];
                reorg_buffer[i * 4 + 3] = converted_buffer[i * 4 + 2];
            }

            depunc.general_work(size, reorg_buffer, depunc_buffer);

            cc_decoder_in.generic_work(depunc_buffer, output);

            data_size_out = repacker.work(output, size * 1.5 / 2, output);

            // Check BER
            d_ber = getBER(fixed_soft_packet);

            // Check we're still in sync!
            if (d_ber_thresold < d_ber)
            {
                d_outsinc++;
                if (d_outsinc == d_outsync_after)
                    d_state = ST_IDLE;
            }
            else
            {
                d_outsinc = 0;
            }
        }
        break;

        default:
            break;
        }

        return data_size_out;
    }

    float MetopViterbi2::ber()
    {
        if (d_state == ST_SYNCED)
            return d_ber;
        else
        {
            float ber = 10;
            for (int s = 0; s < 2; s++)
            {
                for (int p = 0; p < 2; p++)
                {
                    if (ber > d_bers[s][p])
                    {
                        ber = d_bers[s][p];
                    }
                }
            }
            return ber;
        }
    }

    int MetopViterbi2::getState()
    {
        return d_state;
    }
} // namespace npp