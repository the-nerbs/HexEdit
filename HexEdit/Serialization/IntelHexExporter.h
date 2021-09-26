#pragma once
#include "HexExporter.h"


namespace hex
{
    class IntelHexExporter : public HexExporter
    {
    public:
        /// \brief  Constructor.
        ///
        /// \param  stream        The stream to write to.
        /// \param  baseAddress   The address 
        /// \param  recordLength  The preferred number of bytes to include in each record.
        IntelHexExporter(
            std::unique_ptr<CFile> stream,
            unsigned long baseAddress = 0,
            int recordLength = 32);


        /// \brief  Gets the maximum exportable address.
        unsigned long MaxAddress() const override;

        /// \brief  Writes the EOF record.
        void WriteEpilogue() override;

    protected:
        void WriteRecord(const std::uint8_t* data, std::size_t count, unsigned long address) override;

    private:
        void WriteHexRecord(int type, const void* data, std::size_t count, unsigned long address);
    };
}
