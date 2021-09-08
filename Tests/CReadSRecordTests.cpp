#include "Stdafx.h"
#include "utils/AssertHelpers.h"
#include "utils/File.h"
#include "utils/Garbage.h"
#include "utils/TestFiles.h"

#include "SRecord.h"

#include "catch.hpp"


TEST_CASE("CReadSRecord constructor")
{
    std::unique_ptr<CReadSRecord> reader = garbage_fill_and_construct_ptr<CReadSRecord>(
        static_cast<const char*>(TestFiles::GetSRecordsFilePath()),
        TRUE
    );

    CHECK(reader->Error() == "");
}


TEST_CASE("CReadSRecord Get")
{
    CReadSRecord reader{
        static_cast<const char*>(TestFiles::GetSRecordsFilePath()),
        TRUE
    };

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;


    // first record
    std::size_t sz = reader.Get(buffer, buffer_sz, address);
    REQUIRE(sz == 28);
    const std::uint8_t expectedRecord1[28] =
    {
        0x7C, 0x08, 0x02, 0xA6, 0x90, 0x01, 0x00,
        0x04, 0x94, 0x21, 0xFF, 0xF0, 0x7C, 0x6C,
        0x1B, 0x78, 0x7C, 0x8C, 0x23, 0x78, 0x3C,
        0x60, 0x00, 0x00, 0x38, 0x63, 0x00, 0x00
    };
    CHECK(test::areEqual(expectedRecord1, buffer));
    CHECK(address == 0x0000);


    // second record
    sz = reader.Get(buffer, buffer_sz, address);
    REQUIRE(sz == 28);
    const std::uint8_t expectedRecord2[28] =
    {
        0x4B, 0xFF, 0xFF, 0xE5, 0x39, 0x80, 0x00,
        0x00, 0x7D, 0x83, 0x63, 0x78, 0x80, 0x01,
        0x00, 0x14, 0x38, 0x21, 0x00, 0x10, 0x7C,
        0x08, 0x03, 0xA6, 0x4E, 0x80, 0x00, 0x20,
    };
    CHECK(test::areEqual(expectedRecord2, buffer));
    CHECK(address == 0x001C);


    // third record
    sz = reader.Get(buffer, buffer_sz, address);
    REQUIRE(sz == 14);
    const std::uint8_t expectedRecord3[14] =
    {
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77,
        0x6F, 0x72, 0x6C, 0x64, 0x2E, 0x0A, 0x00
    };
    CHECK(test::areEqual(expectedRecord3, buffer));
    CHECK(address == 0x0038);


    // record count (S5) record
    sz = reader.Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(address == 0x0003);


    // S9 (skipped) and EOF
    sz = reader.Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(reader.Error() == "WARNING: No S5 record found");
}

// TODO tests:
//  - CFileException thrown from CStdioFile::Open (in ctor)
//  - CFileException thrown from CStdioFile::ReadStream (in get_rec)
//  - record too long for output buffer
//  - record length does not match declared length
//  - output buffer can be null, but no nulls are passed in intentionally - remove or support?
//  - bad record checksum
//  - pass/fail with non-contiguous records disabled.
//  - No S1/S2/S3 records
//  - S5 record count does not match.
//
// It would be nice to refactor CReadSRecord's ctor to take the CFile& instead of the file
// path, so I can just pass in a CMemFile or something. The only thing that's kept me from
// doing this so far is that CStdioFile declares the ReadString method used to read lines,
// so I'd need to modify the code in a significant way...
