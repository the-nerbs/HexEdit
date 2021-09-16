#include "Stdafx.h"
#include "utils/AssertHelpers.h"
#include "utils/ErrorFile.h"
#include "utils/File.h"
#include "utils/Garbage.h"
#include "utils/TestFiles.h"

#include "IntelHex.h"

#include "catch.hpp"


TEST_CASE("CWriteIntelHex constructors")
{
    std::unique_ptr<CWriteIntelHex> writer;

    SECTION("from file path")
    {
        CString path = TestFiles::GetMutableFilePath();
        writer = garbage_fill_and_construct_ptr<CWriteIntelHex>(static_cast<const char*>(path));
    }

    SECTION("from memory stream")
    {
        writer = std::make_unique<CWriteIntelHex>(std::make_unique<CMemFile>());
    }

    CHECK(writer->Error() == "");
}

TEST_CASE("CWriteIntelHex constructor errors")
{
    SECTION("Cannot open file")
    {
        CString path = TestFiles::GetMutableFilePath();
        CStdioFile denyWrite{ path, CFile::modeCreate | CFile::shareDenyWrite };

        CWriteIntelHex reader{ path, TRUE };

        // actual error message doesn't really matter here, just that we start with the error.
        CHECK(reader.Error() != "");
    }
}

TEST_CASE("CWriteIntelHex destructor CFileException does not propagate")
{
    auto stream = std::make_unique<CErrorFile>(CErrorFile::closeError);

    {
        CWriteIntelHex reader{ std::move(stream), TRUE };
    }

    // no exception thrown by destructor.
}


TEST_CASE("CWriteIntelHex::Put - no data")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[1] = { 0x00 };

    {
        CWriteIntelHex writer{ path, 0x0000, 16 };

        writer.Put(data, 0);
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        ":00000001FF\n";

    CHECK(written == expected);
}

TEST_CASE("CWriteIntelHex::Put - 1 full record")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[16] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    {
        CWriteIntelHex writer{ path, 0x0000, 16 };

        writer.Put(data, sizeof(data));
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        ":1000000000112233445566778899AABBCCDDEEFFF8\n"
        ":00000001FF\n";

    CHECK(written == expected);
}

TEST_CASE("CWriteIntelHex::Put - multiple full records")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[16] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    {
        CWriteIntelHex writer{ path, 0x0000, 4 };

        writer.Put(data, sizeof(data));
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        ":040000000011223396\n"
        ":040004004455667782\n"
        ":040008008899AABB6E\n"
        ":04000C00CCDDEEFF5A\n"
        ":00000001FF\n";

    CHECK(written == expected);
}


TEST_CASE("CWriteIntelHex::Put - 1 partial record")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[15] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE
    };

    {
        CWriteIntelHex writer{ path, 0x0000, 16 };

        writer.Put(data, sizeof(data));
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        ":0F00000000112233445566778899AABBCCDDEEF8\n"
        ":00000001FF\n";

    CHECK(written == expected);
}

TEST_CASE("CWriteIntelHex::Put - partial record after full record")
{
    CString path = TestFiles::GetMutableFilePath();

    const std::uint8_t data[9] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88,
    };

    {
        CWriteIntelHex writer{ path, 0x0000, 8 };

        writer.Put(data, sizeof(data));
    }

    CString written = File::ReadAllText(path, true);

    CString expected =
        ":0800000000112233445566771C\n"
        ":01000800886F\n"
        ":00000001FF\n";

    CHECK(written == expected);
}


TEST_CASE("CWriteIntelHex::Put - write error")
{
    auto stream = std::make_unique<CErrorFile>(CErrorFile::noError);

    // need to play some games with the stream references
    // here since the constructor writes some data as well.
    CErrorFile* pStream = stream.get();

    const std::uint8_t data[16]{};

    CWriteIntelHex writer{ std::move(stream), 0x0000, 16 };

    pStream->writeThrows = true;
    writer.Put(data, sizeof(data));

    // actual error message doesn't really matter here, just that we start with the error.
    CHECK(writer.Error() != "");

    // make sure the dtor doesn't throw
    pStream->writeThrows = false;
}


//TODO: intel export does not support the export addresses?
//TEST_CASE("CWriteIntelHex::Put - addresses")
//{
//    const bool overridePutAddress = GENERATE(false, true);
//
//    auto stream = std::make_unique<CMemFile>();
//    CMemFile* pStream = stream.get();
//
//    CWriteIntelHex writer{ std::move(stream), 0x8000, 1, 8 };
//
//    const std::uint8_t data[8] =
//    {
//        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
//    };
//
//    writer.Put(data, sizeof(data));
//
//    CString expectedSecondRecord;
//
//    if (overridePutAddress)
//    {
//        writer.Put(data, sizeof(data), 0xFFFF);
//
//        // we overrode the address, so use that as the record address.
//        expectedSecondRecord =
//            "S10BFFFF112233445566778892\n";
//    }
//    else
//    {
//        writer.Put(data, sizeof(data));
//
//        // did not override the address, so the second record
//        // is immediately after the first
//        expectedSecondRecord =
//            "S10B8008112233445566778808\n";
//    }
//
//    // note: we can't expect the S5 record here since that's written
//    // by the destructor, and letting that run would close the stream.
//    CString expected =
//        "S00600004844521B\n"
//        "S10B8000112233445566778810\n"
//        + expectedSecondRecord;
//
//    pStream->SeekToBegin();
//    CString actual = File::ReadAllText(*pStream);
//
//    REQUIRE(expected == actual);
//}
