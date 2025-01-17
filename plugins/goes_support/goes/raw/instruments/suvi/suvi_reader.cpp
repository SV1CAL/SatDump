#include "suvi_reader.h"
#include "common/image/image.h"

namespace goes
{
    namespace instruments
    {
        namespace suvi
        {
            SUVIReader::SUVIReader()
            {
                current_frame = new uint16_t[4074 * 423];
            }

            SUVIReader::~SUVIReader()
            {
                delete[] current_frame;
            }

            void SUVIReader::work(ccsds::CCSDSPacket &pkt)
            {
                if (pkt.payload.size() < 8186)
                    return;

                int pkt_counter = pkt.payload[22] << 8 | pkt.payload[23];

                // printf("cnt1 %d\n", pkt_counter);

                if (pkt_counter < 423)
                {
                    for (int i = 0; i < 4074; i++)
                        current_frame[pkt_counter * 4074 + i] =
                            pkt.payload[38 + i * 2 + 1] << 8 | pkt.payload[38 + i * 2 + 0];
                }

                if (pkt_counter == 422) // End of frame
                {
                    image::Image<uint16_t> suvi_img(current_frame + 105, 1330, 1295, 1);
                    suvi_img.crop(0, 3, 0 + 1280, 3 + 1284);
                    for (int i = 0; i < suvi_img.size(); i++)
                        suvi_img[i] = suvi_img.clamp(int(suvi_img[i]) << 5);
                    suvi_img.save_img(directory + "/SUVI_" + std::to_string(img_cnt++));
                }
            }
        }
    }
}