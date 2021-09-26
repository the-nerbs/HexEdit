#include "Stdafx.h"
#include "../utils/AssertHelpers.h"
#include "../utils/ErrorFile.h"
#include "../utils/File.h"
#include "../utils/Garbage.h"
#include "../utils/TestFiles.h"

#include "Serialization/IntelHexImporter.h"

#include "catch.hpp"


TEST_CASE("IntelHexImporter constructor")
{
    CString testFilePath = TestFiles::GetIntelHexFilePath();
    bool allowDiscontiguous = GENERATE(false, true);

    std::unique_ptr<hex::IntelHexImporter> reader = garbage_fill_and_construct_ptr<hex::IntelHexImporter>(
        std::make_unique<CFile>(testFilePath, CFile::modeRead),
        allowDiscontiguous
    );

    CHECK(reader->Error() == "");
}

TEST_CASE("IntelHexImporter destructor CFileException does not propagate")
{
    auto stream = std::make_unique<CErrorFile>(CErrorFile::closeError);

    {
        hex::IntelHexImporter reader{ std::move(stream), TRUE };
    }

    // no exception thrown by destructor.
}


TEST_CASE("IntelHexImporter::Get")
{
    std::unique_ptr<hex::IntelHexImporter> reader;

    SECTION("From file stream")
    {
        reader = std::make_unique<hex::IntelHexImporter>(
            std::make_unique<CStdioFile>(TestFiles::GetIntelHexFilePath(), CFile::modeRead | CFile::typeText),
            true
        );
    }

    SECTION("From memory stream")
    {
        std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();

        // read the test file into the memory stream
        CStdioFile source{
            static_cast<const char*>(TestFiles::GetIntelHexFilePath()),
            CFile::modeRead | CFile::typeText
        };

        char sourceBuf[1024];
        UINT blockSize;
        do
        {
            blockSize = source.Read(sourceBuf, sizeof(sourceBuf));
            memstream->Write(sourceBuf, blockSize);
        } while (blockSize == sizeof(sourceBuf));

        memstream->SeekToBegin();

        reader = std::make_unique<hex::IntelHexImporter>(std::move(memstream), true);
    }

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;


    // first record
    std::size_t sz = reader->Get(buffer, buffer_sz, address);
    REQUIRE(sz == 16);
    const std::uint8_t expectedRecord1[16] =
    {
        0x21, 0x46, 0x01, 0x36, 0x01, 0x21, 0x47, 0x01,
        0x36, 0x00, 0x7E, 0xFE, 0x09, 0xD2, 0x19, 0x01
    };
    CHECK(test::areEqual(expectedRecord1, buffer));
    CHECK(address == 0x0100);


    // second record
    sz = reader->Get(buffer, buffer_sz, address);
    REQUIRE(sz == 16);
    const std::uint8_t expectedRecord2[16] =
    {
        0x21, 0x46, 0x01, 0x7E, 0x17, 0xC2, 0x00, 0x01,
        0xFF, 0x5F, 0x16, 0x00, 0x21, 0x48, 0x01, 0x19
    };
    CHECK(test::areEqual(expectedRecord2, buffer));
    CHECK(address == 0x0110);


    // third record
    sz = reader->Get(buffer, buffer_sz, address);
    REQUIRE(sz == 16);
    const std::uint8_t expectedRecord3[16] =
    {
        0x19, 0x4E, 0x79, 0x23, 0x46, 0x23, 0x96, 0x57,
        0x78, 0x23, 0x9E, 0xDA, 0x3F, 0x01, 0xB2, 0xCA
    };
    CHECK(test::areEqual(expectedRecord3, buffer));
    CHECK(address == 0x0120);


    // fourth record
    sz = reader->Get(buffer, buffer_sz, address);
    REQUIRE(sz == 16);
    const std::uint8_t expectedRecord4[16] =
    {
        0x3F, 0x01, 0x56, 0x70, 0x2B, 0x5E, 0x71, 0x2B,
        0x72, 0x2B, 0x73, 0x21, 0x46, 0x01, 0x34, 0x21
    };
    CHECK(test::areEqual(expectedRecord4, buffer));
    CHECK(address == 0x0130);


    // EOF record
    sz = reader->Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(address == 0x0000);


    // past EOF
    sz = reader->Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(reader->Error() == "WARNING: No Intel hex EOF record found");
}

TEST_CASE("IntelHexImporter::Get - read failure")
{
    hex::IntelHexImporter reader{ std::make_unique<CErrorFile>(CErrorFile::readError), true };

    REQUIRE(reader.Error() == "");


    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;

    std::size_t sz = reader.Get(buffer, buffer_sz, address);

    // actual error message doesn't really matter here, just that we start with the error.
    REQUIRE(reader.Error() != "");

    CHECK(sz == 0);

    // address may or may not be written to, depending on whether a non-data
    // record was fully processed prior to the error.
}

TEST_CASE("IntelHexImporter::Get - output buffer tests")
{
    const char content[] =
        ":10010000214601360121470136007EFE09D2190140\n"
        ":00000001FF\n";

    std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();
    memstream->Write(content, sizeof(content));
    memstream->SeekToBegin();

    hex::IntelHexImporter reader{ std::move(memstream), true };
    REQUIRE(reader.Error() == "");


    SECTION("buffer too short")
    {
        constexpr std::size_t buffer_sz = 0x0F; // 1 less than the necessary length
        unsigned char buffer[buffer_sz];
        unsigned long address = ~0;

        std::size_t sz = reader.Get(buffer, buffer_sz, address);

        CHECK(sz == 0);
        CHECK(reader.Error() == "ERROR: Record too long at line 1");
    }

    SECTION("buffer exact size")
    {
        constexpr std::size_t buffer_sz = 0x10;
        unsigned char buffer[buffer_sz];
        unsigned long address = ~0;

        std::size_t sz = reader.Get(buffer, buffer_sz, address);

        CHECK(sz == 0x10);
        CHECK(reader.Error() == "");
    }

    SECTION("buffer longer than needed")
    {
        constexpr std::size_t buffer_sz = 0x11; // 1 more than the necessary length
        unsigned char buffer[buffer_sz];
        unsigned long address = ~0;

        std::size_t sz = reader.Get(buffer, buffer_sz, address);

        CHECK(sz == 0x10);
        CHECK(reader.Error() == "");
    }
}

TEST_CASE("IntelHexImporter::Get - bad records")
{
    CMemFile* memstream = new CMemFile();

    hex::IntelHexImporter reader{ std::unique_ptr<CFile>(memstream), false };
    CString expectedError;

    SECTION("record shorter than possible minimum length with start char")
    {
        // min is 11 chars. Error because it starts with a colon
        constexpr char line[] = ":000000000\n";
        memstream->Write(line, sizeof(line) - 1);

        expectedError = "ERROR: Short record at line 1";
    }

    SECTION("record length shorter than declared")
    {
        // declared as 2 bytes, only 1
        constexpr char record[] = ":0201000011EC\n";
        memstream->Write(record, sizeof(record) - 1);

        expectedError = "ERROR: Short record at line 1";
    }

    // note: record length *longer* than the declared length does not signal an error.
    //SECTION("record length longer than declared")
    //{
    //    // declared as 2 bytes, but is actually 3
    //    constexpr char S1[] = ":02010000112233EC\n";
    //    memstream->Write(S1, sizeof(S1) - 1);
    //
    //    expectedError = "ERROR: Long S record at line 1";
    //}

    SECTION("bad hex digit (record length)")
    {
        constexpr char record[] = ":0G0100001122CA\n";
        memstream->Write(record, sizeof(record) - 1);

        expectedError = "ERROR: Invalid hexadecimal at line 1";
    }

    SECTION("bad hex digit (address)")
    {
        constexpr char record[] = ":02010G001122CA\n";
        memstream->Write(record, sizeof(record) - 1);

        expectedError = "ERROR: Invalid hexadecimal at line 1";
    }

    // note: invalid hex in the type field just causes the record to be skipped.

    SECTION("bad hex digit (data)")
    {
        constexpr char record[] = ":02010000112GCA\n";
        memstream->Write(record, sizeof(record) - 1);

        expectedError = "ERROR: Invalid hexadecimal at line 1";
    }

    SECTION("bad hex digit (checksum)")
    {
        // data: FF -> FG
        constexpr char record[] = ":020100001122CG\n";
        memstream->Write(record, sizeof(record) - 1);

        expectedError = "ERROR: Invalid hexadecimal at line 1";
    }

    SECTION("bad checksum")
    {
        constexpr char record[] = ":020100001122CB\n";
        memstream->Write(record, sizeof(record) - 1);

        expectedError = "ERROR: Checksum mismatch at line 1";
    }


    constexpr char EofRecord[] = ":00000001FF\n";
    memstream->Write(EofRecord, sizeof(EofRecord) - 1);

    memstream->SeekToBegin();

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;

    std::size_t sz = reader.Get(buffer, buffer_sz, address);

    CHECK(sz == 0);
    CHECK(reader.Error() == expectedError);
}

TEST_CASE("IntelHexImporter::Get - skipped records")
{
    CMemFile* memstream = new CMemFile();

    constexpr char Record0[] = ":10010000214601360121470136007EFE09D2190140\n";
    memstream->Write(Record0, sizeof(Record0) - 1);

    constexpr char Record1[] = ":100110002146017E17C20001FF5F16002148011928\n";
    memstream->Write(Record1, sizeof(Record1) - 1);


    hex::IntelHexImporter reader{ std::unique_ptr<CFile>(memstream), false };

    SECTION("record shorter than possible minimum length without start char")
    {
        // min is 11 chars. skipped because it does not start with a colon.
        constexpr char line[] = "0000000000\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("record does not start with colon")
    {
        constexpr char line[] = ";020000011200EB\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("Type 2 (ext segment address) not supported")
    {
        constexpr char line[] = ":020000021200EA\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("Type 3 (start segment address) not supported")
    {
        constexpr char line[] = ":020000031200E9\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("Type 4 (ext linear address) not supported")
    {
        constexpr char line[] = ":020000041200E8\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("Type number ASCII less than number")
    {
        constexpr char line[] = ":0200000/1200EB\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("Type number ASCII greater than number")
    {
        constexpr char line[] = ":0200000:1200EB\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    constexpr char EofRecord[] = ":00000001FF\n";
    memstream->Write(EofRecord, sizeof(EofRecord) - 1);

    memstream->SeekToBegin();

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;

    // process the whole file until EOF or error
    std::size_t sz;
    do
    {
        sz = reader.Get(buffer, buffer_sz, address);
    } while (sz > 0 && reader.Error() == "");

    CHECK(sz == 0);
    CHECK(reader.Error() == "");
}

TEST_CASE("IntelHexImporter::Get - non-contiguous records")
{
    // 1 byte gap between first and second S1 records
    const char content[] =
        ":10010000214601360121470136007EFE09D2190140\n"
        ":100111002146017E17C20001FF5F16002148011927\n"
        ":00000001FF\n";

    std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();
    memstream->Write(content, sizeof(content));
    memstream->SeekToBegin();

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;

    SECTION("Non-contiguous records allowed")
    {
        hex::IntelHexImporter reader{ std::move(memstream), true };
        REQUIRE(reader.Error() == "");

        // both records should read OK
        std::size_t sz = reader.Get(buffer, buffer_sz, address);
        CHECK(sz == 0x10);
        CHECK(address == 0x0100);
        REQUIRE(reader.Error() == "");

        sz = reader.Get(buffer, buffer_sz, address);
        CHECK(sz == 0x10);
        CHECK(address == 0x0111);
        REQUIRE(reader.Error() == "");
    }

    SECTION("Non-contiguous records disallowed")
    {
        hex::IntelHexImporter reader{ std::move(memstream), false };
        REQUIRE(reader.Error() == "");

        // first record should read OK
        std::size_t sz = reader.Get(buffer, buffer_sz, address);
        CHECK(sz == 0x10);
        CHECK(address == 0x0100);
        REQUIRE(reader.Error() == "");

        // second record raises an error
        sz = reader.Get(buffer, buffer_sz, address);
        CHECK(sz == 0);
        CHECK(address == 0x0111);
        REQUIRE(reader.Error() == "ERROR: Non-adjoining address at line 2");
    }
}

TEST_CASE("IntelHexImporter::Get - no data records")
{
    const char content[] =
        ":00000001FF\n";

    std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();
    memstream->Write(content, sizeof(content));
    memstream->SeekToBegin();

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;

    hex::IntelHexImporter reader{ std::move(memstream), true };

    std::size_t sz = reader.Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(reader.Error() == "No data records found");
}
