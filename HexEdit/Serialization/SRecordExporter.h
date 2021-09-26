#pragma once
#include "HexExporter.h"


namespace hex
{
    enum class SType
    {
        S1 = 1,     //!< 16-bit address field
        S2 = 2,     //!< 24-bit address field
        S3 = 3,     //!< 32-bit address field
    };

    class SRecordExporter : public HexExporter
    {
    public:
        /// \brief  Constructor.
        ///
        /// \param  stream        The stream to write to.
        /// \param  stype         The data record type.
        /// \param  baseAddress   The address 
        /// \param  recordLength  The preferred number of bytes to include in each record.
        SRecordExporter(
            std::unique_ptr<CFile> stream,
            SType stype,
            unsigned long baseAddress = 0,
            int recordLength = 32);


        /// \brief  Gets the maximum exportable address.
        unsigned long MaxAddress() const override;

        /// \brief  Writes the S0 record.
        void WritePrologue() override;

        /// \brief  Writes the S5 record.
        void WriteEpilogue() override;

    protected:
        void WriteRecord(const std::uint8_t* data, std::size_t count, unsigned long address) override;

    private:
        void WriteSRecord(int type, const void* data, std::size_t count, unsigned long address);

    private:
        SType _stype;
    };
}
