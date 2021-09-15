#include "Stdafx.h"
#include "utils/AssertHelpers.h"
#include "utils/ErrorFile.h"
#include "utils/File.h"
#include "utils/Garbage.h"
#include "utils/TestFiles.h"

#include "SRecord.h"

#include "catch.hpp"


TEST_CASE("CWriteSRecord constructors")
{
    std::unique_ptr<CWriteSRecord> writer;

    SECTION("from file path")
    {
        CString path = TestFiles::GetMutableFilePath();
        writer = garbage_fill_and_construct_ptr<CWriteSRecord>(static_cast<const char*>(path));
    }

    SECTION("from memory stream")
    {
        writer = std::make_unique<CWriteSRecord>(std::make_unique<CMemFile>());
    }

    CHECK(writer->Error() == "");
}

TEST_CASE("CWriteSRecord constructor errors")
{
    SECTION("Cannot open file")
    {
        CString path = TestFiles::GetMutableFilePath();
        CStdioFile denyWrite{ path, CFile::modeCreate | CFile::shareDenyWrite };

        CWriteSRecord reader{ path, TRUE };

        // actual error message doesn't really matter here, just that we start with the error.
        CHECK(reader.Error() != "");
    }

    SECTION("Error writing S0")
    {
        auto stream = std::make_unique<CErrorFile>(CErrorFile::writeError);

        // need to play some games with the stream references to be
        // able to change things after the unique_ptr is moved from.
        CErrorFile* pStream = stream.get();

        const std::uint8_t data[16]{};

        CWriteSRecord writer{ std::move(stream), 0x0000, 1, 16 };

        // actual error message doesn't really matter here, just that we start with the error.
        CHECK(writer.Error() != "");

        // make sure the dtor doesn't throw
        pStream->writeThrows = false;
    }
}

TEST_CASE("CWriteSRecord destructor CFileException does not propagate")
{
    auto stream = std::make_unique<CErrorFile>(CErrorFile::closeError);

    {
        CWriteSRecord reader{ std::move(stream), TRUE };
    }

    // no exception thrown by destructor.
}


TEST_CASE("CWriteSRecord::Put - no data")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[1] = { 0x00 };

    {
        CWriteSRecord writer{ path, 0x0000, 1, 16 };

        writer.Put(data, 0);
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        "S00600004844521B\n"
        "S5030000FC\n";

    CHECK(written == expected);
}

TEST_CASE("CWriteSRecord::Put - 1 full record")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[16] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    {
        CWriteSRecord writer{ path, 0x0000, 1, 16 };

        writer.Put(data, sizeof(data));
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        "S00600004844521B\n"
        "S113000000112233445566778899AABBCCDDEEFFF4\n"
        "S5030001FB\n";

    CHECK(written == expected);
}

TEST_CASE("CWriteSRecord::Put - multiple full records")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[16] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    {
        CWriteSRecord writer{ path, 0x0000, 1, 4 };

        writer.Put(data, sizeof(data));
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        "S00600004844521B\n"
        "S10700000011223392\n"
        "S1070004445566777E\n"
        "S10700088899AABB6A\n"
        "S107000CCCDDEEFF56\n"
        "S5030004F8\n";

    CHECK(written == expected);
}


TEST_CASE("CWriteSRecord::Put - 1 partial record")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[15] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE
    };

    {
        CWriteSRecord writer{ path, 0x0000, 1, 16 };

        writer.Put(data, sizeof(data));
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        "S00600004844521B\n"
        "S112000000112233445566778899AABBCCDDEEF4\n"
        "S5030001FB\n";

    CHECK(written == expected);
}

TEST_CASE("CWriteSRecord::Put - partial record after full record")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[9] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88,
    };

    {
        CWriteSRecord writer{ path, 0x0000, 1, 8 };

        writer.Put(data, sizeof(data));
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        "S00600004844521B\n"
        "S10B0000001122334455667718\n"
        "S1040008886B\n"
        "S5030002FA\n";

    CHECK(written == expected);
}


TEST_CASE("CWriteSRecord::Put - write error")
{
    auto stream = std::make_unique<CErrorFile>(CErrorFile::noError);

    // need to play some games with the stream references
    // here since the constructor writes some data as well.
    CErrorFile* pStream = stream.get();

    const std::uint8_t data[16]{};

    CWriteSRecord writer{ std::move(stream), 0x0000, 1, 16 };

    pStream->writeThrows = true;
    writer.Put(data, sizeof(data));

    // actual error message doesn't really matter here, just that we start with the error.
    CHECK(writer.Error() != "");

    // make sure the dtor doesn't throw
    pStream->writeThrows = false;
}


TEST_CASE("CWriteSRecord::Put - data record types")
{
    int stype = GENERATE(1, 2, 3);

    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    CWriteSRecord writer{ std::move(stream), 0x0000, stype, 8 };

    const std::uint8_t data[8] =
    {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
    };

    writer.Put(data, sizeof(data));

    CString expected = "S00600004844521B\n";

    switch (stype)
    {
    case 1:
        // S1, address is 2 bytes
        expected += "S10B0000112233445566778890\n";
        break;

    case 2:
        // S2, address is 3 bytes
        expected += "S20C00000011223344556677888F\n";
        break;

    case 3:
        // S3, address is 3 bytes
        expected += "S30D0000000011223344556677888E\n";
        break;

    default:
        FAIL("unexpected stype!");
    }

    // note: we can't expect the S5 record here since that's written
    // by the destructor, and letting that run would close the stream.

    pStream->SeekToBegin();
    CString actual = File::ReadAllText(*pStream);

    REQUIRE(expected == actual);
}


TEST_CASE("CWriteSRecord::Put - addresses")
{
    const bool overridePutAddress = GENERATE(false, true);

    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    CWriteSRecord writer{ std::move(stream), 0x8000, 1, 8 };

    const std::uint8_t data[8] =
    {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
    };

    writer.Put(data, sizeof(data));

    CString expectedSecondRecord;

    if (overridePutAddress)
    {
        writer.Put(data, sizeof(data), 0xFFFF);

        // we overrode the address, so use that as the record address.
        expectedSecondRecord =
            "S10BFFFF112233445566778892\n";
    }
    else
    {
        writer.Put(data, sizeof(data));

        // did not override the address, so the second record
        // is immediately after the first
        expectedSecondRecord =
            "S10B8008112233445566778808\n";
    }

    // note: we can't expect the S5 record here since that's written
    // by the destructor, and letting that run would close the stream.
    CString expected =
        "S00600004844521B\n"
        "S10B8000112233445566778810\n"
        + expectedSecondRecord;

    pStream->SeekToBegin();
    CString actual = File::ReadAllText(*pStream);

    REQUIRE(expected == actual);
}
