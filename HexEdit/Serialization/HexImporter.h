#pragma once
#include <afx.h>
#include <afxstr.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace hex
{
    /// \brief  Base class for line-based hex file importers.
    class HexImporter
    {
    protected:
        HexImporter(std::unique_ptr<CFile> stream, bool allowNoncontiguous);


    public:
        virtual ~HexImporter();

        /// \brief  Reads a hex record.
        /// 
        /// \param       buffer     A pointer to the buffer to fill with the data that is read.
        /// \param       bufferLen  The length of \p buffer.
        /// \param[out]  address    Returns the address of the read record.
        /// 
        /// \returns  The size of the read record.
        std::size_t Get(
            std::uint8_t* buffer,
            std::size_t bufferLen,
            unsigned long& address);

        /// \brief  Gets the last error message.
        CString Error() const { return _error; }

        /// \brief  Gets the number of records read.
        int RecordsRead() const { return _recordsRead; }


    protected:
        enum class RecordType
        {
            Data,           //!< Line is a data record.
            Skip,           //!< Line can be skipped.
            Termination,    //!< Line is a termination record.
            Error,          //!< An error occurred while processing the line.
        };

        /// \brief  Gets the current line number.
        int LineNumber() const { return _lineNumber; }

        /// \brief  Reads a line from the backing stream.
        ///
        /// \param  buffer     A pointer to the buffer to receive the read line.
        /// \param  bufferLen  The length of the buffer.
        /// 
        /// \returns  true if a line was read, or false if EOF.
        /// 
        /// \exception  std::exception  Thrown if buffer is not long enough to hold the full line.
        bool ReadLine(char* buffer, std::size_t bufferLen);

        /// \brief  Gets the display name for data records.
        ///
        /// \remarks
        /// This is formatted into a message similar to "No (data) records found".
        virtual CString DataRecordName() const { return "data"; }

        /// \brief  Gets the display name for the EOF record.
        ///
        /// \remarks
        /// This is formatted into a message similar to "No (EOF) record found".
        virtual CString EofRecordName() const { return "EOF"; }

        /// \brief  Reads the next record.
        ///
        /// \param       buffer     A pointer to the buffer to receive the read data.
        /// \param       bufferLen  The length of \p buffer.
        /// \param[out]  address    Returns the record's address field.
        /// \param[out]  recordLen  Returns the length of the read data.
        /// 
        /// \returns  The kind of record that was read.
        ///
        /// \exception  std::exception  This or derived may be thrown to signal an error.
        ///                             The exception message will be used as the reader's error.
        virtual RecordType ReadRecord(
            std::uint8_t* buffer,
            std::size_t bufferLen,
            unsigned long& address,
            std::size_t& recordLen) = 0;

        /// \brief  Appends one or more bytes to a checksum.
        /// 
        /// \param  bytes     The bytes to append.
        /// \param  numBytes  The number of bytes to append, 0 to 4.
        /// \param  checksum  A reference to the checksum to update.
        virtual void AppendToChecksum(std::uint64_t bytes, int numBytes, int& checksum) = 0;

        /// \brief  Parses a hex string.
        /// 
        /// \param  pHex      A pointer to the string to parse.
        /// \param  numBytes  The number of bytes to parse, 1 to 4.
        /// \param  checksum  If not null, the checksum to update with the parsed value.
        /// 
        /// \returns  The parsed value.
        ///
        /// \exception  std::exception  This or derived may be thrown to signal an error.
        ///                             The exception message will be used as the reader's error.
        std::uint64_t ParseHex(const char* pHex, int numBytes, int* checksum);

        /// \brief  Tries to parse a hex string.
        /// 
        /// \param       pHex      A pointer to the string to parse.
        /// \param       numBytes  The number of bytes to parse, 1 to 4.
        /// \param       checksum  If not null, the checksum to update with the parsed value.
        /// \param[out]  parsed    The parsed value.
        /// 
        /// \returns  True if successful, or false if not.
        bool TryParseHex(const char* pHex, int numBytes, int* checksum, std::uint64_t& parsed);

    private:
        std::unique_ptr<CFile> _stream;
        std::uint64_t _nextAddress;
        int _lineNumber;
        CString _error;
        int _recordsRead;
        bool _allowNoncontiguous;
    };
}
