#include "Gzip.h"
#include <algorithm>
#include <array>

namespace Engine
{
    uint32_t crc32(const uint8_t* data, size_t size)
    {
        static const std::array<uint32_t, 256> table = []
        {
            std::array<uint32_t, 256> t {};
            for (uint32_t i = 0; i < 256; ++i)
            {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k)
                    c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
                t[i] = c;
            }
            return t;
        }();

        uint32_t c = 0xffffffffu;
        for (size_t i = 0; i < size; ++i)
            c = table[(c ^ data[i]) & 0xff] ^ (c >> 8);
        return c ^ 0xffffffffu;
    }

    std::vector<uint8_t> gzipStored(const std::string& data)
    {
        const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
        const size_t size = data.size();

        std::vector<uint8_t> out = { 0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0xff }; // magic, deflate, no flags, unknown OS
        out.reserve(size + size / 65535 * 5 + 32);

        size_t pos = 0;
        do
        {
            const size_t len = std::min<size_t>(65535, size - pos);
            const bool final = pos + len == size;
            out.push_back(final ? 1 : 0); // BFINAL, BTYPE=00 (stored)
            out.push_back((uint8_t) len);
            out.push_back((uint8_t) (len >> 8));
            out.push_back((uint8_t) ~len);
            out.push_back((uint8_t) (~len >> 8));
            out.insert(out.end(), bytes + pos, bytes + pos + len);
            pos += len;
        } while (pos < size);

        const uint32_t crc = crc32(bytes, size);
        const uint32_t isize = (uint32_t) size;
        for (int s = 0; s < 32; s += 8) out.push_back((uint8_t) (crc >> s));
        for (int s = 0; s < 32; s += 8) out.push_back((uint8_t) (isize >> s));
        return out;
    }
}
