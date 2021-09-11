#include "Stdafx.h"
#include "IntelHexImporter.h"

#include <cstring>

namespace hex
{
    IntelHexImporter::IntelHexImporter(std::unique_ptr<CFile> stream, bool allowNoncontiguous)
        : HexImporter{ std::move(stream), allowNoncontiguous }
    { }

    HexImporter::RecordType IntelHexImporter::ReadRecord(
        std::uint8_t* buffer,
        std::size_t bufferLen,
        unsigned long& address,
        std::size_t& recordLen)
    {
        static constexpr int CharsPerByte = 2;
        static constexpr int LengthBytes = 1;
        static constexpr int AddressBytes = 2;
        static constexpr int TypeBytes = 1;
        static constexpr int ChecksumBytes = 1;

        // record component max lengths:
        //  - colon:        1 char
        //  - byte count:   2 chars (1 byte)
        //  - address:      4 chars (2 bytes)
        //  - type:         2 chars (1 byte)
        //  - data:         512 chars (256 bytes)
        //  - checksum:     2 chars (1 byte)
        static constexpr std::size_t MaxLineLen = 1 + CharsPerByte * (LengthBytes + AddressBytes + TypeBytes + 256 + ChecksumBytes);

        // and minimum length:
        //  - colon:        1 char
        //  - byte count:   2 chars (1 byte)
        //  - address:      4 chars (2 bytes)
        //  - type:         2 chars (1 byte)
        //  - data:         0 chars (0 bytes)
        //  - checksum:     2 chars (1 byte)
        static constexpr std::size_t MinLineLen = 1 + CharsPerByte * (LengthBytes + AddressBytes + TypeBytes + 0 + ChecksumBytes);


        char line[MaxLineLen + 1];

        if (!ReadLine(line, sizeof(line)))
        {
            return RecordType::Error;
        }

        int lineLen = std::strlen(line);

        if (lineLen < MinLineLen)
        {
            // if it starts with a ':', then we expect it to be 
            if (lineLen > 0 && line[0] == ':')
            {
                // line is too long for the provided output buffer.
                CString msg;
                msg.Format("Short record at line %d", LineNumber());
                throw std::exception{ msg };
            }

            return RecordType::Skip;
        }

        if (line[0] != ':')
        {
            return RecordType::Skip;
        }

        const char* current = &line[1];
        int checksum = 0;

        // read the data length
        recordLen = static_cast<std::size_t>(ParseHex(current, 1, &checksum));
        current += CharsPerByte;

        // validate the data length
        if (recordLen > bufferLen)
        {
            // line is too long for the provided output buffer.
            CString msg;
            msg.Format("Record too long at line %d", LineNumber());
            throw std::exception{ msg };
        }

        //TODO: should this also signal an error if the record is too long? (probably yes)
        if (lineLen < (1 + CharsPerByte * (LengthBytes + AddressBytes + TypeBytes + recordLen + ChecksumBytes)))
        {
            // line is shorter than the declared length.
            CString msg;
            msg.Format("Short record at line %d", LineNumber());
            throw std::exception{ msg };
        }

        // read the address
        address = static_cast<unsigned long>(ParseHex(current, 2, &checksum));
        current += 2 * CharsPerByte;

        // read the record type
        std::uint64_t parsed;
        int type;
        if (TryParseHex(current, 1, &checksum, parsed))
        {
            type = static_cast<int>(parsed);
        }
        else
        {
            return RecordType::Skip;
        }
        current += CharsPerByte;

        // TODO: type 4 could be useful to support as well. Types 2, 3, and 5 maybe not though.
        if (type != 0 && type != 1)
        {
            return RecordType::Skip;
        }

        // read the data.
        for (std::size_t i = 0; i < recordLen; i++)
        {
            buffer[i] = static_cast<std::uint8_t>(ParseHex(current, 1, &checksum));
            current += CharsPerByte;
        }

        // read the checksum and validate.
        int readChecksum = static_cast<int>(ParseHex(current, 1, nullptr));
        if (readChecksum != ((-checksum) & 0xFF))
        {
            CString msg;
            msg.Format("Checksum mismatch at line %d", LineNumber());
            throw std::exception{ msg };
        }

        if (type == 0)
        {
            return RecordType::Data;
        }

        ASSERT(type == 1);
        return RecordType::Termination;
    }

    void IntelHexImporter::AppendToChecksum(std::uint64_t bytes, int numBytes, int& checksum)
    {
        for (std::uint64_t ul = bytes; numBytes > 0; numBytes--)
        {
            checksum += (ul & 0xff);
            ul >>= 8;
        }
    }
}
