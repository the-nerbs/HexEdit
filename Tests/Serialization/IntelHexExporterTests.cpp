#include "Stdafx.h"
#include "../utils/AssertHelpers.h"
#include "../utils/ErrorFile.h"
#include "../utils/File.h"
#include "../utils/Garbage.h"
#include "../utils/TestFiles.h"

#include "Serialization/IntelHexExporter.h"

#include "catch.hpp"


TEST_CASE("IntelHexExporter constructors")
{
    std::unique_ptr<hex::IntelHexExporter> writer;

    SECTION("from file path")
    {
        CString path = TestFiles::GetMutableFilePath();
        writer = garbage_fill_and_construct_ptr<hex::IntelHexExporter>(
            std::make_unique<CStdioFile>(path, CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive | CFile::typeText)
        );
    }

    SECTION("from memory stream")
    {
        writer = std::make_unique<hex::IntelHexExporter>(std::make_unique<CMemFile>());
    }

    CHECK(writer->Error() == "");
}

TEST_CASE("IntelHexExporter destructor CFileException does not propagate")
{
    auto stream = std::make_unique<CErrorFile>(CErrorFile::closeError);

    {
        hex::IntelHexExporter writer{ std::move(stream) };
    }

    // no exception thrown by destructor.
}


TEST_CASE("IntelHexExporter::WritePrologue")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    hex::IntelHexExporter exporter{ std::move(stream) };

    exporter.WritePrologue();

    CString written = File::ReadAllText(*pStream);

    // Intel Hex format has no prologue record
    CString expected = "";

    CHECK(written == expected);
}


TEST_CASE("IntelHexExporter::WriteEpilogue")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    hex::IntelHexExporter exporter{ std::move(stream) };

    CString expected;

    SECTION("No data records")
    {
        expected = ":00000001FF\n";
    }

    SECTION("Yes data records")
    {
        // write a few records out so we have a 
        const std::uint8_t data[1]{ 0xCC };
        exporter.WriteData(data, sizeof(data));
        exporter.WriteData(data, sizeof(data));
        exporter.WriteData(data, sizeof(data));

        expected =
            ":01000000CC33\n"
            ":01000100CC32\n"
            ":01000200CC31\n"
            ":00000001FF\n";
    }

    exporter.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CHECK(written == expected);
}


TEST_CASE("IntelHexExporter::Put - no data")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[1] = { 0x00 };

    hex::IntelHexExporter writer{ std::move(stream), 0x0000, 16 };

    writer.WritePrologue();
    writer.WriteData(data, 0);
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        ":00000001FF\n";

    CHECK(written == expected);
}

TEST_CASE("IntelHexExporter::Put - 1 full record")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[16] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    hex::IntelHexExporter writer{ std::move(stream), 0x0000, 16 };

    writer.WritePrologue();
    writer.WriteData(data, sizeof(data));
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        ":1000000000112233445566778899AABBCCDDEEFFF8\n"
        ":00000001FF\n";

    CHECK(written == expected);
}

TEST_CASE("IntelHexExporter::Put - multiple full records")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[16] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    hex::IntelHexExporter writer{ std::move(stream), 0x0000, 4};

    writer.WritePrologue();
    writer.WriteData(data, sizeof(data));
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        ":040000000011223396\n"
        ":040004004455667782\n"
        ":040008008899AABB6E\n"
        ":04000C00CCDDEEFF5A\n"
        ":00000001FF\n";

    CHECK(written == expected);
}


TEST_CASE("IntelHexExporter::Put - 1 partial record")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[15] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE
    };

    hex::IntelHexExporter writer{ std::move(stream), 0x0000, 16 };
    writer.WritePrologue();
    writer.WriteData(data, sizeof(data));
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        ":0F00000000112233445566778899AABBCCDDEEF8\n"
        ":00000001FF\n";

    CHECK(written == expected);
}

TEST_CASE("IntelHexExporter::Put - partial record after full record")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[9] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88,
    };

    hex::IntelHexExporter writer{ std::move(stream), 0x0000, 8 };

    writer.WritePrologue();
    writer.WriteData(data, sizeof(data));
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        ":0800000000112233445566771C\n"
        ":01000800886F\n"
        ":00000001FF\n";

    CHECK(written == expected);
}


TEST_CASE("IntelHexExporter::Put - write error")
{
    auto stream = std::make_unique<CErrorFile>(CErrorFile::noError);
    CErrorFile* pStream = stream.get();

    const std::uint8_t data[16]{};

    hex::IntelHexExporter writer{ std::move(stream), 0x0000, 16 };

    pStream->writeThrows = true;
    writer.WriteData(data, sizeof(data));

    // actual error message doesn't really matter here, just that we start with the error.
    CHECK(writer.Error() != "");

    // make sure the dtor doesn't throw
    pStream->writeThrows = false;
}


TEST_CASE("IntelHexExporter::Put - addresses")
{
    const bool overridePutAddress = GENERATE(false, true);

    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    hex::IntelHexExporter writer{ std::move(stream), 0x8000, 8 };

    const std::uint8_t data[8] =
    {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
    };

    writer.WriteData(data, sizeof(data));

    CString expectedSecondRecord;

    if (overridePutAddress)
    {
        writer.WriteData(data, sizeof(data), 0xFFFF);

        // we overrode the address, so use that as the record address.
        expectedSecondRecord =
            ":08FFFF00112233445566778896\n";
    }
    else
    {
        writer.WriteData(data, sizeof(data));

        // did not override the address, so the second record
        // is immediately after the first
        expectedSecondRecord =
            ":0880080011223344556677880C\n";
    }

    // note: we can't expect the S5 record here since that's written
    // by the destructor, and letting that run would close the stream.
    CString expected =
        ":08800000112233445566778814\n"
        + expectedSecondRecord;

    pStream->SeekToBegin();
    CString actual = File::ReadAllText(*pStream);

    REQUIRE(expected == actual);
}
