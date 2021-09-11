#include "Stdafx.h"
#include "SRecordImporter.h"

#include <cstring>

namespace hex
{
    // address bytes for each record type.
    // -1 indicates an invalid/unsupported record type.
    static constexpr const int addressBytes[10] = { 2, 2, 3, 4, -1, 2, -1, 4, 3, 2 };


    SRecordImporter::SRecordImporter(std::unique_ptr<CFile> stream, bool allowNoncontiguous)
        : HexImporter{ std::move(stream), allowNoncontiguous }
    { }

    HexImporter::RecordType SRecordImporter::ReadRecord(
        std::uint8_t* buffer,
        std::size_t bufferLen,
        unsigned long& address,
        std::size_t& recordLen)
    {
        static constexpr int CharsPerByte = 2;

        // record component max lengths:
        //  - record type:                    2 chars (1 byte equivalent)
        //  - byte count:                     1 byte
        //  - (address, data, checksum):    256 bytes
        static constexpr std::size_t MaxLineLen = CharsPerByte * (1 + 1 + 256);

        // and minimum length:
        //  - record type:  2 chars (1 byte equivalent)
        //  - byte count:   1 byte
        //  - address:      2 bytes
        //  - data:         0 bytes
        //  - checksum:     1 byte
        static constexpr std::size_t MinLineLen = CharsPerByte * (1 + 1 + 2 + 0 + 1);


        char line[MaxLineLen + 1];

        if (!ReadLine(line, sizeof(line)))
        {
            return RecordType::Error;
        }

        int lineLen = std::strlen(line);

        if (lineLen < MinLineLen)
        {
            return RecordType::Error;
        }

        if (line[0] != 'S')
        {
            return RecordType::Skip;
        }

        int type = (line[1] - '0');
        if (type < 0 || 9 < type)
        {
            // not S follow by a digit
            return RecordType::Skip;
        }
        else if (addressBytes[type] < 0)
        {
            // invalid or unsupported record type
            return RecordType::Skip;
        }

        const char* current = &line[2];
        int checksum = 0;

        // read the byte count
        int numBytes = static_cast<int>(ParseHex(current, 1, &checksum));
        current += CharsPerByte;

        recordLen = numBytes - addressBytes[type] - 1;

        // validate the record length
        if (recordLen > bufferLen)
        {
            // line is too long for the provided output buffer.
            CString msg;
            msg.Format("Record too long at line %d", LineNumber());
            throw std::exception{ msg };
        }

        //TODO: should this also signal an error if the record is too long? (probably yes)
        if (lineLen < (CharsPerByte * (2 + numBytes)))
        {
            // line is too long for the provided output buffer.
            CString msg;
            msg.Format("Short S record at line %d", LineNumber());
            throw std::exception{ msg };
        }

        // read the address
        address = static_cast<unsigned long>(ParseHex(current, addressBytes[type], &checksum));
        current += CharsPerByte * addressBytes[type];

        // read the data
        for (int i = 0; i < recordLen; i++)
        {
            buffer[i] = static_cast<std::uint8_t>(ParseHex(current, 1, &checksum));
            current += CharsPerByte;
        }

        // read the checksum and validate.
        int readChecksum = static_cast<int>(ParseHex(current, 1, nullptr));
        if (readChecksum != (~checksum & 0xFF))
        {
            CString msg;
            msg.Format("Checksum mismatch at line %d", LineNumber());
            throw std::exception{ msg };
        }

        if (type == 0)
        {
            return RecordType::Skip;
        }
        else if (1 <= type && type <= 3)
        {
            return RecordType::Data;
        }
        else if (type == 5)
        {
            if (address != RecordsRead())
            {
                CString msg;
                msg.Format("Mismatch in number of records (S5 record) at line %d", LineNumber());
                throw std::exception{ msg };
            }

            // there may be S7/S8/S9 record(s) after the S5, but
            // we don't do anything with them, so terminate now.
            return RecordType::Termination;
        }

        // guaranteed by addressBytes check disallowing types 4 and 6.
        ASSERT(7 <= type && type <= 9);

        return RecordType::Skip;
    }

    void SRecordImporter::AppendToChecksum(std::uint64_t bytes, int numBytes, int& checksum)
    {
        for (std::uint64_t ul = bytes; numBytes > 0; numBytes--)
        {
            checksum += (ul & 0xff);
            ul >>= 8;
        }
    }
}
