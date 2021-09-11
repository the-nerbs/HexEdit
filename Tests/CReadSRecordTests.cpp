#include "Stdafx.h"
#include "utils/AssertHelpers.h"
#include "utils/File.h"
#include "utils/Garbage.h"
#include "utils/TestFiles.h"

#include "SRecordImporter.h"

#include "catch.hpp"


class CErrorFile : public CMemFile
{
public:
    using CMemFile::CMemFile;

    ~CErrorFile() = default;

    UINT Read(void* lpBuf, UINT nCount) override
    {
        AfxThrowFileException(CFileException::genericException);
    }
};


TEST_CASE("SRecordImporter constructor")
{
    CString testFilePath = TestFiles::GetSRecordsFilePath();
    bool allowDiscontiguous = GENERATE(false, true);

    std::unique_ptr<hex::SRecordImporter> reader = garbage_fill_and_construct_ptr<hex::SRecordImporter>(
        std::make_unique<CFile>(testFilePath, CFile::modeRead),
        allowDiscontiguous
    );

    CHECK(reader->Error() == "");
    CHECK(reader->RecordsRead() == 0);
}


TEST_CASE("SRecordImporter::Get")
{
    std::unique_ptr<hex::SRecordImporter> reader;

    SECTION("From file stream")
    {
        reader = std::make_unique<hex::SRecordImporter>(
            std::make_unique<CFile>(TestFiles::GetSRecordsFilePath(), CFile::modeRead),
            true
        );
    }

    SECTION("From memory stream")
    {
        std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();

        // read the test file into the memory stream
        CStdioFile source{
            static_cast<const char*>(TestFiles::GetSRecordsFilePath()),
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

        reader = std::make_unique<hex::SRecordImporter>(std::move(memstream), true);
    }

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;


    // first record
    std::size_t sz = reader->Get(buffer, buffer_sz, address);
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
    CHECK(reader->RecordsRead() == 1);


    // second record
    sz = reader->Get(buffer, buffer_sz, address);
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
    CHECK(reader->RecordsRead() == 2);


    // third record
    sz = reader->Get(buffer, buffer_sz, address);
    REQUIRE(sz == 14);
    const std::uint8_t expectedRecord3[14] =
    {
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77,
        0x6F, 0x72, 0x6C, 0x64, 0x2E, 0x0A, 0x00
    };
    CHECK(test::areEqual(expectedRecord3, buffer));
    CHECK(address == 0x0038);
    CHECK(reader->RecordsRead() == 3);


    // record count (S5) record
    sz = reader->Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(address == 0x0003);
    CHECK(reader->RecordsRead() == 3);


    // S9 (skipped) and EOF
    sz = reader->Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(reader->Error() == "WARNING: No S5 record found");
}

TEST_CASE("SRecordImporter::Get - read failure")
{
    hex::SRecordImporter reader{ std::make_unique<CErrorFile>(), true };

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

TEST_CASE("SRecordImporter::Get - output buffer tests")
{
    const char content[] =
        "S00F000068656C6C6F202020202000003C\n"
        "S11F00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026\n"
        "S5030001FB\n"
        "S9030000FC\n";

    std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();
    memstream->Write(content, sizeof(content));
    memstream->SeekToBegin();

    hex::SRecordImporter reader{ std::move(memstream), true };
    REQUIRE(reader.Error() == "");


    SECTION("buffer too short")
    {
        constexpr std::size_t buffer_sz = 0x1B; // 1 less than the necessary length
        unsigned char buffer[buffer_sz];
        unsigned long address = ~0;

        std::size_t sz = reader.Get(buffer, buffer_sz, address);

        CHECK(sz == 0);
        CHECK(reader.Error() == "ERROR: Record too long at line 2");
    }

    SECTION("buffer exact size")
    {
        constexpr std::size_t buffer_sz = 0x1C;
        unsigned char buffer[buffer_sz];
        unsigned long address = ~0;

        std::size_t sz = reader.Get(buffer, buffer_sz, address);

        CHECK(sz == 0x1C);
        CHECK(reader.Error() == "");
    }

    SECTION("buffer longer than needed")
    {
        constexpr std::size_t buffer_sz = 0x1D; // 1 more than the necessary length
        unsigned char buffer[buffer_sz];
        unsigned long address = ~0;

        std::size_t sz = reader.Get(buffer, buffer_sz, address);

        CHECK(sz == 0x1C);
        CHECK(reader.Error() == "");
    }
}

TEST_CASE("SRecordImporter::Get - bad records")
{
    CMemFile* memstream = new CMemFile();

    constexpr char S0[] = "S00F000068656C6C6F202020202000003C\n";
    memstream->Write(S0, sizeof(S0)-1);

    hex::SRecordImporter reader{ std::unique_ptr<CFile>(memstream), false };
    CString expectedError;

    SECTION("record length shorter than declared")
    {
        // declared as 32 bytes, only 31
        constexpr char S1[] = "S12000007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026\n";
        memstream->Write(S1, sizeof(S1) - 1);

        expectedError = "ERROR: Short S record at line 2";
    }

    // note: record length *longer* than the declared length does not signal an error.
    //SECTION("record length longer than declared")
    //{
    //    // declared as 32 bytes, only 31
    //    constexpr char S1[] = "S11E00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026\n";
    //    memstream->Write(S1, sizeof(S1) - 1);
    //
    //    expectedError = "ERROR: Long S record at line 2";
    //}

    SECTION("bad hex digit (record length)")
    {
        constexpr char S1[] = "S10G0000FFFC\n";
        memstream->Write(S1, sizeof(S1) - 1);

        expectedError = "ERROR: Invalid hexadecimal at line 2";
    }

    SECTION("bad hex digit (address)")
    {
        constexpr char S1[] = "S104000GFFFC\n";
        memstream->Write(S1, sizeof(S1) - 1);

        expectedError = "ERROR: Invalid hexadecimal at line 2";
    }

    SECTION("bad hex digit (data)")
    {
        constexpr char S1[] = "S1040000FGFC\n";
        memstream->Write(S1, sizeof(S1) - 1);

        expectedError = "ERROR: Invalid hexadecimal at line 2";
    }

    SECTION("bad hex digit (checksum)")
    {
        // data: FF -> FG
        constexpr char S1[] = "S1040000FFFG\n";
        memstream->Write(S1, sizeof(S1) - 1);

        expectedError = "ERROR: Invalid hexadecimal at line 2";
    }

    SECTION("bad checksum")
    {
        constexpr char S1[] = "S11F00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000027\n";
        memstream->Write(S1, sizeof(S1) - 1);

        expectedError = "ERROR: Checksum mismatch at line 2";
    }


    constexpr char S5[] = "S5030003F9\n";
    memstream->Write(S5, sizeof(S5) - 1);

    constexpr char S9[] = "S9030000FC\n";
    memstream->Write(S9, sizeof(S9) - 1);

    memstream->SeekToBegin();

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;

    std::size_t sz = reader.Get(buffer, buffer_sz, address);

    CHECK(sz == 0);
    CHECK(reader.Error() == expectedError);
}

TEST_CASE("SRecordImporter::Get - skipped records")
{
    CMemFile* memstream = new CMemFile();

    constexpr char S0[] = "S00F000068656C6C6F202020202000003C\n";
    memstream->Write(S0, sizeof(S0) - 1);

    constexpr char S1[] = "S11F00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026\n";
    memstream->Write(S1, sizeof(S1) - 1);


    hex::SRecordImporter reader{ std::unique_ptr<CFile>(memstream), false };

    SECTION("record does not start with S")
    {
        constexpr char line[] = "T11F00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("S4 (reserved) not supported")
    {
        constexpr char line[] = "S4030000FB\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("S6 (24-bit address; not official) not supported")
    {
        constexpr char line[] = "S605000000FFFB\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("Type number ASCII less than number")
    {
        constexpr char line[] = "S/05000000FFFB\n";
        memstream->Write(line, sizeof(line) - 1);
    }

    SECTION("Type number ASCII greater than number")
    {
        constexpr char line[] = "S:05000000FFFB\n";
        memstream->Write(line, sizeof(line) - 1);
    }


    constexpr char S5[] = "S5030001FB\n";
    memstream->Write(S5, sizeof(S5) - 1);

    constexpr char S9[] = "S9030000FC\n";
    memstream->Write(S9, sizeof(S9) - 1);

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

TEST_CASE("SRecordImporter::Get - non-contiguous records")
{
    // 1 byte gap between first and second S1 records
    const char content[] =
        "S00F000068656C6C6F202020202000003C\n"
        "S11F00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026\n"
        "S11F001D4BFFFFE5398000007D83637880010014382100107C0803A64E800020E8\n"
        "S5030002FA\n"
        "S9030000FC\n";

    std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();
    memstream->Write(content, sizeof(content));
    memstream->SeekToBegin();

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;

    SECTION("Non-contiguous records allowed")
    {
        hex::SRecordImporter reader{ std::move(memstream), true };
        REQUIRE(reader.Error() == "");

        // both records should read OK
        std::size_t sz = reader.Get(buffer, buffer_sz, address);
        CHECK(sz == 0x1C);
        CHECK(address == 0x0000);
        REQUIRE(reader.Error() == "");

        sz = reader.Get(buffer, buffer_sz, address);
        CHECK(sz == 0x1C);
        CHECK(address == 0x001D);
        REQUIRE(reader.Error() == "");
    }

    SECTION("Non-contiguous records disallowed")
    {
        hex::SRecordImporter reader{ std::move(memstream), false };
        REQUIRE(reader.Error() == "");

        // first record should read OK
        std::size_t sz = reader.Get(buffer, buffer_sz, address);
        CHECK(sz == 0x1C);
        CHECK(address == 0x0000);
        REQUIRE(reader.Error() == "");

        // second record raises an error
        sz = reader.Get(buffer, buffer_sz, address);
        CHECK(sz == 0);
        CHECK(address == 0x001D);
        REQUIRE(reader.Error() == "ERROR: Non-adjoining address at line 3");
    }
}

TEST_CASE("SRecordImporter::Get - no data records")
{
    const char content[] =
        "S00F000068656C6C6F202020202000003C\n"
        "S5030000FC\n"
        "S9030000FC\n";

    std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();
    memstream->Write(content, sizeof(content));
    memstream->SeekToBegin();

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;

    hex::SRecordImporter reader{ std::move(memstream), true };

    std::size_t sz = reader.Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(reader.Error() == "No S1/S2/S3 records found");
}

TEST_CASE("SRecordImporter::Get - S2/S8 (24 bit) records")
{
    const char content[] =
        "S00F000068656C6C6F202020202000003C\n"
        "S206FFFFFF1122C9\n"
        "S5030001FB\n"
        "S804000000FB\n";

    std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();
    memstream->Write(content, sizeof(content));
    memstream->SeekToBegin();

    hex::SRecordImporter reader{ std::move(memstream), true };
    REQUIRE(reader.Error() == "");

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = 0;

    // S2 record
    std::size_t sz = reader.Get(buffer, buffer_sz, address);
    REQUIRE(sz == 2);
    CHECK(address == 0xFFFFFF);
    CHECK(reader.Error() == "");

    const std::uint8_t expectedRecord[2] = { 0x11, 0x22 };
    CHECK(test::areEqual(expectedRecord, buffer));

    // S5, S8, to EOF
    sz = reader.Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(reader.Error() == "");
}

TEST_CASE("SRecordImporter::Get - S3/S7 (32 bit) records")
{
    const char content[] =
        "S00F000068656C6C6F202020202000003C\n"
        "S307FFFFFFFF1122C9\n"
        "S5030001FB\n"
        "S70500000000FA\n";

    std::unique_ptr<CMemFile> memstream = std::make_unique<CMemFile>();
    memstream->Write(content, sizeof(content));
    memstream->SeekToBegin();

    hex::SRecordImporter reader{ std::move(memstream), true };
    REQUIRE(reader.Error() == "");

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = 0;

    // S3 record
    std::size_t sz = reader.Get(buffer, buffer_sz, address);
    REQUIRE(sz == 2);
    CHECK(address == 0xFFFFFFFF);
    CHECK(reader.Error() == "");

    const std::uint8_t expectedRecord[2] = { 0x11, 0x22 };
    CHECK(test::areEqual(expectedRecord, buffer));

    // S5, S7, to EOF
    sz = reader.Get(buffer, buffer_sz, address);
    CHECK(sz == 0);
    CHECK(reader.Error() == "");
}

TEST_CASE("SRecordImporter::Get - S5 data record count")
{
    CMemFile* memstream = new CMemFile();

    constexpr char S0[] = "S00F000068656C6C6F202020202000003C\n";
    memstream->Write(S0, sizeof(S0) - 1);

    hex::SRecordImporter reader{ std::unique_ptr<CFile>(memstream), false };
    CString expectedError;

    SECTION("Too few data records")
    {
        // 1 data record
        constexpr char S1[] = "S11F00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026\n";
        memstream->Write(S1, sizeof(S1) - 1);

        expectedError = "ERROR: Mismatch in number of records (S5 record) at line 3";
    }

    SECTION("Too many data records")
    {
        // 3 data records
        constexpr char S1a[] = "S11F00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026\n";
        memstream->Write(S1a, sizeof(S1a) - 1);

        constexpr char S1b[] = "S11F001C4BFFFFE5398000007D83637880010014382100107C0803A64E800020E9\n";
        memstream->Write(S1b, sizeof(S1b) - 1);

        constexpr char S1c[] = "S111003848656C6C6F20776F726C642E0A0042\n";
        memstream->Write(S1c, sizeof(S1c) - 1);

        expectedError = "ERROR: Mismatch in number of records (S5 record) at line 5";
    }

    // declare 2 data records
    constexpr char S5[] = "S5030002FA\n";
    memstream->Write(S5, sizeof(S5) - 1);

    constexpr char S9[] = "S9030000FC\n";
    memstream->Write(S9, sizeof(S9) - 1);

    memstream->SeekToBegin();

    constexpr std::size_t buffer_sz = 1024;
    unsigned char buffer[buffer_sz];
    unsigned long address = ~0;

    // process the whole file
    std::size_t sz;
    do
    {
        sz = reader.Get(buffer, buffer_sz, address);
    }  while (sz > 0);

    CHECK(reader.Error() == expectedError);
}
