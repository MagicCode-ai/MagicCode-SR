// Minimal RGBA8 PNG writer for MagicSR smoke dumps (zlib).
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

namespace MagicSRSmokePng
{
inline void PutBe32(uint8_t* Dst, uint32_t Value)
{
    Dst[0] = static_cast<uint8_t>((Value >> 24) & 0xff);
    Dst[1] = static_cast<uint8_t>((Value >> 16) & 0xff);
    Dst[2] = static_cast<uint8_t>((Value >> 8) & 0xff);
    Dst[3] = static_cast<uint8_t>(Value & 0xff);
}

inline bool WriteChunk(FILE* File, const char* Type, const uint8_t* Payload, uint32_t PayloadLen)
{
    uint8_t LenBe[4];
    PutBe32(LenBe, PayloadLen);
    if (fwrite(LenBe, 1, 4, File) != 4)
    {
        return false;
    }
    if (fwrite(Type, 1, 4, File) != 4)
    {
        return false;
    }
    if (PayloadLen > 0 && fwrite(Payload, 1, PayloadLen, File) != PayloadLen)
    {
        return false;
    }

    uint32_t Crc = crc32(0L, Z_NULL, 0);
    Crc = crc32(Crc, reinterpret_cast<const Bytef*>(Type), 4);
    if (PayloadLen > 0)
    {
        Crc = crc32(Crc, Payload, PayloadLen);
    }
    uint8_t CrcBe[4];
    PutBe32(CrcBe, Crc);
    return fwrite(CrcBe, 1, 4, File) == 4;
}

/** Write tightly packed RGBA8 pixels as a color PNG. */
inline bool WriteRgbaPng(const char* Path, const uint8_t* Rgba, int Width, int Height)
{
    if (Path == nullptr || Rgba == nullptr || Width <= 0 || Height <= 0)
    {
        return false;
    }

    const size_t RowBytes = static_cast<size_t>(Width) * 4 + 1;
    const size_t RawSize = RowBytes * static_cast<size_t>(Height);
    uint8_t* Raw = static_cast<uint8_t*>(malloc(RawSize));
    if (Raw == nullptr)
    {
        return false;
    }

    for (int Y = 0; Y < Height; ++Y)
    {
        uint8_t* Row = Raw + static_cast<size_t>(Y) * RowBytes;
        Row[0] = 0;  // filter None
        memcpy(Row + 1, Rgba + static_cast<size_t>(Y) * Width * 4, static_cast<size_t>(Width) * 4);
    }

    uLongf CompressedCap = compressBound(static_cast<uLong>(RawSize));
    uint8_t* Compressed = static_cast<uint8_t*>(malloc(CompressedCap));
    if (Compressed == nullptr)
    {
        free(Raw);
        return false;
    }

    uLongf CompressedLen = CompressedCap;
    const int ZRet = compress2(Compressed, &CompressedLen, Raw, static_cast<uLong>(RawSize), Z_BEST_SPEED);
    free(Raw);
    if (ZRet != Z_OK)
    {
        free(Compressed);
        return false;
    }

    FILE* File = fopen(Path, "wb");
    if (File == nullptr)
    {
        free(Compressed);
        return false;
    }

    static const uint8_t Signature[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    bool Ok = fwrite(Signature, 1, 8, File) == 8;

    uint8_t Ihdr[13] = {};
    PutBe32(Ihdr + 0, static_cast<uint32_t>(Width));
    PutBe32(Ihdr + 4, static_cast<uint32_t>(Height));
    Ihdr[8] = 8;   // bit depth
    Ihdr[9] = 6;   // RGBA
    Ihdr[10] = 0;  // compression
    Ihdr[11] = 0;  // filter
    Ihdr[12] = 0;  // interlace
    Ok = Ok && WriteChunk(File, "IHDR", Ihdr, 13);
    Ok = Ok && WriteChunk(File, "IDAT", Compressed, static_cast<uint32_t>(CompressedLen));
    Ok = Ok && WriteChunk(File, "IEND", nullptr, 0);

    free(Compressed);
    fclose(File);
    return Ok;
}

/** Expand tightly packed R8 to RGBA8 (R=G=B, A=255) then write PNG. */
inline bool WriteR8AsRgbaPng(const char* Path, const uint8_t* Gray, int Width, int Height)
{
    if (Gray == nullptr || Width <= 0 || Height <= 0)
    {
        return false;
    }

    const size_t PixelCount = static_cast<size_t>(Width) * Height;
    uint8_t* Rgba = static_cast<uint8_t*>(malloc(PixelCount * 4));
    if (Rgba == nullptr)
    {
        return false;
    }
    for (size_t I = 0; I < PixelCount; ++I)
    {
        const uint8_t V = Gray[I];
        Rgba[I * 4 + 0] = V;
        Rgba[I * 4 + 1] = V;
        Rgba[I * 4 + 2] = V;
        Rgba[I * 4 + 3] = 255;
    }
    const bool Ok = WriteRgbaPng(Path, Rgba, Width, Height);
    free(Rgba);
    return Ok;
}
}  // namespace MagicSRSmokePng
