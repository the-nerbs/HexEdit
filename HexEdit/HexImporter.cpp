#include "Stdafx.h"
#include "HexImporter.h"

#include "Misc.h"

namespace hex
{
    HexImporter::HexImporter(std::unique_ptr<CFile> stream, bool allowNoncontiguous) :
        _stream{ std::move(stream) },
        _nextAddress{ ~0UL },
        _lineNumber{ 0 },
        _error{},
        _recordsRead{ 0 },
        _allowNoncontiguous{ allowNoncontiguous }
    { }


    std::size_t HexImporter::Get(
        std::uint8_t* buffer,
        std::size_t bufferLen,
        unsigned long& address)
    {
        try
        {
            RecordType type;
            std::size_t len;

            do
            {
                type = ReadRecord(buffer, bufferLen, address, len);

            } while (type == RecordType::Skip);

            switch (type)
            {
            case RecordType::Data:
                if (!_allowNoncontiguous)
                {
                    if (_recordsRead != 0
                        && address != _nextAddress)
                    {
                        _error.Format("ERROR: Non-adjoining address at line %d", _lineNumber);
                        return 0;
                    }

                    _nextAddress = address + len;
                }
                _recordsRead++;
                return len;

            case RecordType::Termination:
                if (RecordsRead() == 0)
                {
                    _error.Format("No %s records found", static_cast<const char*>(DataRecordName()));
                }
                return 0;

            default:
            case RecordType::Error:
                if (_error.IsEmpty())
                {
                    _error.Format("WARNING: No %s record found", static_cast<const char*>(EofRecordName()));
                }
                return 0;
            }
        }
        catch (const std::exception& ex)
        {
            _error = CString{ "ERROR: " } + ex.what();
            return 0;
        }
        catch (CFileException* ex)
        {
            _error = ::FileErrorMessage(ex, CFile::modeRead);
            ex->Delete();
            return 0;
        }
        catch (CException* ex)
        {
            char buffer[512];
            ex->GetErrorMessage(buffer, sizeof(buffer));
            ex->Delete();

            _error = CString{ "ERROR: An unexpected error occurred. " } + buffer;
            return 0;
        }
    }

    bool HexImporter::ReadLine(char* buffer, std::size_t bufferLen)
    {
        char* psz = buffer;
        char* const end = buffer + bufferLen;
        UINT readCount = 0;

        while (psz < end)
        {
            readCount = _stream->Read(psz, 1);

            if (readCount == 0)
            {
                // hit EOF
                return false;
            }
            else if (*psz == '\n')
            {
                // hit new line
                // note: psz not incremented here as we don't want the new line char.
                break;
            }

            // if the source stream was opened in binary mode, then discard the CR from CRLF sequences.
            if (*psz != '\r')
            {
                psz++;
            }
        }
        _lineNumber++;

        if (readCount > 0 && psz < end)
        {
            *psz = '\0';
        }

        if (psz >= end)
        {
            throw std::exception{ "Record too long at line %ld" };
        }

        return true;
    }

    std::uint64_t HexImporter::ParseHex(const char* pHex, int numBytes, int* checksum)
    {
        std::uint64_t retval;

        if (!TryParseHex(pHex, numBytes, checksum, retval))
        {
            // something did not parse successfully
            CString msg;
            msg.Format("Invalid hexadecimal at line %d", _lineNumber);
            throw std::exception{ msg };
        }

        return retval;
    }

    bool HexImporter::TryParseHex(const char* pHex, int numBytes, int* checksum, std::uint64_t& parsed)
    {
        ASSERT(numBytes <= 8);

        char buf[32];
        const int numDigits = numBytes * 2;

        memcpy(buf, pHex, numDigits);
        buf[numDigits] = '\0';

        char* endptr = nullptr;
        parsed = strtoull(buf, &endptr, 16);

        if (endptr != &buf[numDigits]
            || (parsed == 0 && strspn(buf, "0") != numDigits))
        {
            parsed = 0;
            return false;
        }

        // Add bytes to checksum
        if (checksum)
        {
            AppendToChecksum(parsed, numBytes, *checksum);
        }

        return true;
    }
}
