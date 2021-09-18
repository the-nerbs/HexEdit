#include "Stdafx.h"
#include "HexExporter.h"

#include "Misc.h"


namespace hex
{
    HexExporter::HexExporter(std::unique_ptr<CFile> stream, unsigned long baseAddress, int recordLength) :
        _stream{ std::move(stream) },
        _nextAddress{ baseAddress },
        _recordLength{ recordLength },
        _error{},
        _recordsWritten{ 0 }
    { }

    HexExporter::~HexExporter()
    {
        try
        {
            if (_stream)
            {
                _stream->Close();
            }
        }
        catch (CFileException* ex)
        {
            ex->Delete();
        }
    }


    void HexExporter::WriteData(const std::uint8_t* data, std::size_t count, unsigned long address)
    {
        if (address != UINT_MAX)
        {
            _nextAddress = address;
        }

        while (count > 0)
        {
            std::size_t thisRecordLength = std::min<std::size_t>(count, _recordLength);

            WriteRecord(data, thisRecordLength, _nextAddress);

            count -= thisRecordLength;
            data += thisRecordLength;
            _nextAddress += thisRecordLength;

            _recordsWritten++;
        }
    }


    void HexExporter::WriteLine(const CString& line)
    {
        const char* psz = line.GetString();
        UINT len = static_cast<UINT>(line.GetLength());
        const char eol = '\n';

        try
        {
            _stream->Write(line, len);
            _stream->Write(&eol, 1);
        }
        catch (CFileException* ex)
        {
            _error = ::FileErrorMessage(ex, CFile::modeWrite);
            ex->Delete();
        }
    }
}
