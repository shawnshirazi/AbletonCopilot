#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal, dependency-free gzip (RFC 1952) encoder, used to write Ableton
// Live Set (.als) files, which are gzip-compressed XML. Uses DEFLATE
// "stored" blocks (RFC 1951 BTYPE=00): perfectly valid gzip that any
// inflater (zlib, Live, gunzip) reads, just not size-reduced - a Live Set
// of a few MB is fine uncompressed, and this keeps Engine/ free of a zlib
// dependency (same zero-dependency convention as the rest of Engine/).
namespace Engine
{
    uint32_t crc32(const uint8_t* data, size_t size);

    std::vector<uint8_t> gzipStored(const std::string& data);
}
