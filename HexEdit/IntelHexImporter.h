#pragma once
#include "HexImporter.h"

namespace hex
{
    /// \brief  Motorola SRecord hex file importer
    class IntelHexImporter final : public HexImporter
    {
    public:
        IntelHexImporter(std::unique_ptr<CFile> stream, bool allowNoncontiguous);


    protected:
        CString EofRecordName() const override { return "Intel hex EOF"; }

        RecordType ReadRecord(
            std::uint8_t* buffer,
            std::size_t bufferLen,
            unsigned long& address,
            std::size_t& recordLength) override;

        void AppendToChecksum(std::uint64_t bytes, int numBytes, int& checksum) override;
    };
}
