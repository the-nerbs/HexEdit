#include "Stdafx.h"
#include "SRecordExporter.h"

namespace hex
{
    // address bytes for each record type.
    // -1 indicates an invalid/unsupported record type.
    static constexpr const int addressBytes[10] = { 2, 2, 3, 4, -1, 2, -1, 4, 3, 2 };


    SRecordExporter::SRecordExporter(
        std::unique_ptr<CFile> stream, SType stype,
        unsigned long baseAddress, int recordLength)
        : HexExporter{ std::move(stream), baseAddress, recordLength },
          _stype{ stype }
    { }


    void SRecordExporter::WritePrologue()
    {
        WriteSRecord(0, "HDR", 3, 0x0000);
    }

    void SRecordExporter::WriteEpilogue()
    {
        WriteSRecord(5, "", 0, RecordsWritten());
    }

    void SRecordExporter::WriteRecord(const std::uint8_t* data, std::size_t count, unsigned long address)
    {
        WriteSRecord(static_cast<int>(_stype), data, count, address);
    }


    void SRecordExporter::WriteSRecord(int type, const void* data, std::size_t count, unsigned long address)
    {
        ASSERT(0 <= type && type <= 9);
        ASSERT(addressBytes[type] != -1);

        // total byte length = SType + byte count + address + data byte count + checksum
        int fullRecLen = 1 + 1 + addressBytes[type] + static_cast<int>(count) + 1;

        // and 2 chars per byte
        fullRecLen *= 2;

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

        // SType
        *pstr = 'S';
        pstr++;

        *pstr = hexDigits[type];
        pstr++;


        // byte count includes: address + data + checksum
        int byteCount = addressBytes[type] + count + 1;
        ASSERT(byteCount <= 0xFF);
        writeByte(byteCount);

        // address
        for (int b = addressBytes[type] - 1; b >= 0; b--)
        {
            std::uint8_t bval = (address >> (8 * b)) & 0xFF;
            writeByte(bval);
        }

        // data
        const std::uint8_t* pData = static_cast<const std::uint8_t*>(data);
        for (int i = 0; i < count; i++)
        {
            writeByte(pData[i]);
        }

        // checksum
        writeByte(~checksum & 0xFF);

        record.ReleaseBuffer();
        WriteLine(record);
    }
}
