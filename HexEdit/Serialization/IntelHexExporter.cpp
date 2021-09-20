#include "Stdafx.h"
#include "IntelHexExporter.h"

namespace hex
{
    IntelHexExporter::IntelHexExporter(std::unique_ptr<CFile> stream, unsigned long baseAddress, int recordLength)
        : HexExporter{ std::move(stream), baseAddress, recordLength }
    { }


    unsigned long IntelHexExporter::MaxAddress() const
    {
        // record type 4 (extended linear address) not supported.
        return 0xFFFFu;
    }

    void IntelHexExporter::WriteEpilogue()
    {
        WriteHexRecord(1, "", 0, 0x0000);
    }

    void IntelHexExporter::WriteRecord(const std::uint8_t* data, std::size_t count, unsigned long address)
    {
        WriteHexRecord(0, data, count, address);
    }


    void IntelHexExporter::WriteHexRecord(int type, const void* data, std::size_t count, unsigned long address)
    {
        ASSERT(0 <= type && type <= 9);

        // total byte length = count + address + type + data + checksum
        int fullRecLen = 1 + 2 + 1 + static_cast<int>(count) + 1;

        // and 2 chars per byte
        fullRecLen *= 2;

        // +1 for the colon
        fullRecLen += 1;

        CString record;
        char* pstr = record.GetBuffer(fullRecLen);
        int checksum = 0;

        const char hexDigits[] = "0123456789ABCDEF";

        auto writeByte = [&](std::uint8_t b)
        {
            *pstr = hexDigits[((b >> 4) & 0xF)];
            pstr++;

            *pstr = hexDigits[(b & 0xF)];
            pstr++;

            checksum += b;
        };

        // colon
        *pstr = ':';
        pstr++;

        // data byte count
        writeByte(count & 0xFF);

        // address
        writeByte((address >> 8) & 0xFF);
        writeByte(address & 0xFF);

        // type 
        writeByte(type);

        // data
        const std::uint8_t* pData = static_cast<const std::uint8_t*>(data);
        for (int i = 0; i < count; i++)
        {
            writeByte(pData[i]);
        }

        // checksum
        writeByte((-checksum) & 0xFF);

        record.ReleaseBuffer();
        WriteLine(record);
    }
}
