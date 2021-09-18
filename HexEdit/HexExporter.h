#pragma once
#include <afx.h>
#include <afxstr.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace hex
{
    /// \brief  Base class for line-based hex file exporters.
    class HexExporter
    {
    protected:
        /// \brief  Constructor.
        ///
        /// \param  stream        The stream to write to.
        /// \param  baseAddress   The address 
        /// \param  recordLength  The preferred number of bytes to include in each record.
        HexExporter(
            std::unique_ptr<CFile> stream,
            unsigned long baseAddress = 0,
            int recordLength = 32);

    public:
        virtual ~HexExporter() = default;


        /// \brief  Gets the last error message.
        CString Error() const { return _error; }

        /// \brief  Gets the number of records that have been written.
        int RecordsWritten() const { return _recordsWritten; }


        /// \brief  Writes any prologue needed by this exporter.
        virtual void WritePrologue() {}

        /// \brief  Writes any epilogue needed by this exporter.
        virtual void WriteEpilogue() {}


        /// \brief  Writes one or more records for the given data.
        ///
        /// \param  data     A pointer to the data to export.
        /// \param  count    The number of bytes from \p data to export.
        /// \param  address  If specified, overrides the address for the exported records.
        void WriteData(const std::uint8_t* data, std::size_t count, unsigned long address = UINT_MAX);


    protected:
        /// \brief  Writes one record for the given data.
        ///
        /// \param  data   A pointer to the data to export.
        /// \param  count  The number of bytes from \p data to export.
        /// 
        /// \returns  The length of the written record.
        virtual void WriteRecord(const std::uint8_t* data, std::size_t count, unsigned long address) = 0;

        /// \brief  Writes a line of text to the export.
        ///
        /// \param  line          The line of text to write.
        void WriteLine(const CString& line);


    private:
        std::unique_ptr<CFile> _stream;
        unsigned long _nextAddress;
        int _recordLength;
        CString _error;
        int _recordsWritten;
    };
}
