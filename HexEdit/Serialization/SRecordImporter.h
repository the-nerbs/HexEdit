#pragma once
#include "HexImporter.h"

namespace hex
{
    /// \brief  Motorola SRecord hex file importer
    class SRecordImporter final : public HexImporter
    {
    public:
        SRecordImporter(std::unique_ptr<CFile> stream, bool allowNoncontiguous);


    protected:
        CString DataRecordName() const override { return "S1/S2/S3"; }
        CString EofRecordName() const override { return "S5"; }

        RecordType ReadRecord(
            std::uint8_t* buffer,
            std::size_t bufferLen,
            unsigned long& address,
            std::size_t& recordLength) override;

        void AppendToChecksum(std::uint64_t bytes, int numBytes, int& checksum) override;
    };
}
