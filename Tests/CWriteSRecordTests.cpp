#include "Stdafx.h"
#include "utils/AssertHelpers.h"
#include "utils/ErrorFile.h"
#include "utils/File.h"
#include "utils/Garbage.h"
#include "utils/TestFiles.h"

#include "SRecordExporter.h"

#include "catch.hpp"


TEST_CASE("SRecordExporter constructors")
{
    std::unique_ptr<hex::SRecordExporter> writer;

    SECTION("from file stream")
    {
        CString path = TestFiles::GetMutableFilePath();
        writer = garbage_fill_and_construct_ptr<hex::SRecordExporter>(
            std::make_unique<CStdioFile>(path, CFile::modeCreate | CFile::modeWrite | CFile::shareExclusive | CFile::typeText),
            hex::SType::S1
        );
    }

    SECTION("from memory stream")
    {
        writer = std::make_unique<hex::SRecordExporter>(
            std::make_unique<CMemFile>(),
            hex::SType::S1
        );
    }

    CHECK(writer->RecordsWritten() == 0);
    CHECK(writer->Error() == "");
}

TEST_CASE("SRecordExporter destructor CFileException does not propagate")
{
    auto stream = std::make_unique<CErrorFile>(CErrorFile::closeError);

    {
        hex::SRecordExporter writer{ std::move(stream), hex::SType::S1 };
    }

    // no exception thrown by destructor.
}


TEST_CASE("SRecordExporter::WritePrologue")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    hex::SRecordExporter exporter{ std::move(stream), hex::SType::S1 };

    exporter.WritePrologue();

    CString written = File::ReadAllText(*pStream);
    CString expected = "S00600004844521B\n";

    CHECK(written == expected);
}


TEST_CASE("SRecordExporter::WriteEpilogue")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    hex::SRecordExporter exporter{ std::move(stream), hex::SType::S1 };

    CString expected;

    SECTION("No data records")
    {
        expected = "S5030000FC\n";
    }

    SECTION("Yes data records")
    {
        // write a few records out so we have a 
        const std::uint8_t data[1]{ 0xCC };
        exporter.WriteData(data, sizeof(data));
        exporter.WriteData(data, sizeof(data));
        exporter.WriteData(data, sizeof(data));

        expected =
            "S1040000CC2F\n"
            "S1040001CC2E\n"
            "S1040002CC2D\n"
            "S5030003F9\n";
    }

    exporter.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CHECK(written == expected);
}

TEST_CASE("SRecordExporter::WriteData - no data")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[1] = { 0x00 };

    hex::SRecordExporter writer{ std::move(stream), hex::SType::S1, 0x0000, 16 };

    writer.WritePrologue();
    writer.WriteData(data, 0);
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        "S00600004844521B\n"
        "S5030000FC\n";

    CHECK(written == expected);
}

TEST_CASE("SRecordExporter::WriteData - 1 full record")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[16] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    hex::SRecordExporter writer{ std::move(stream), hex::SType::S1, 0x0000, 16 };

    writer.WritePrologue();
    writer.WriteData(data, sizeof(data));
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        "S00600004844521B\n"
        "S113000000112233445566778899AABBCCDDEEFFF4\n"
        "S5030001FB\n";

    CHECK(written == expected);
}

TEST_CASE("SRecordExporter::WriteData - multiple full records")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[16] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    hex::SRecordExporter writer{ std::move(stream), hex::SType::S1, 0x0000, 4 };

    writer.WritePrologue();
    writer.WriteData(data, sizeof(data));
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        "S00600004844521B\n"
        "S10700000011223392\n"
        "S1070004445566777E\n"
        "S10700088899AABB6A\n"
        "S107000CCCDDEEFF56\n"
        "S5030004F8\n";

    CHECK(written == expected);
}


TEST_CASE("SRecordExporter::WriteData - 1 partial record")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[15] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE
    };

    hex::SRecordExporter writer{ std::move(stream), hex::SType::S1, 0x0000, 16 };

    writer.WritePrologue();
    writer.WriteData(data, sizeof(data));
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        "S00600004844521B\n"
        "S112000000112233445566778899AABBCCDDEEF4\n"
        "S5030001FB\n";

    CHECK(written == expected);
}

TEST_CASE("SRecordExporter::WriteData - partial record after full record")
{
    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    const std::uint8_t data[9] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88,
    };

    hex::SRecordExporter writer{ std::move(stream), hex::SType::S1, 0x0000, 8 };

    writer.WritePrologue();
    writer.WriteData(data, sizeof(data));
    writer.WriteEpilogue();

    CString written = File::ReadAllText(*pStream);

    CString expected =
        "S00600004844521B\n"
        "S10B0000001122334455667718\n"
        "S1040008886B\n"
        "S5030002FA\n";

    CHECK(written == expected);
}


TEST_CASE("SRecordExporter::WriteData - write error")
{
    auto stream = std::make_unique<CErrorFile>(CErrorFile::noError);
    CErrorFile* pStream = stream.get();

    const std::uint8_t data[16]{};

    hex::SRecordExporter writer{ std::move(stream), hex::SType::S1, 0x0000, 16 };

    pStream->writeThrows = true;
    writer.WriteData(data, sizeof(data));

    // actual error message doesn't really matter here, just that we start with the error.
    CHECK(writer.Error() != "");

    // make sure the dtor doesn't throw
    pStream->writeThrows = false;
}


TEST_CASE("SRecordExporter::WriteData - data record types")
{
    hex::SType stype = GENERATE(hex::SType::S1, hex::SType::S2, hex::SType::S3);

    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    hex::SRecordExporter writer{ std::move(stream), stype, 0x0000, 8 };

    const std::uint8_t data[8] =
    {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
    };

    writer.WriteData(data, sizeof(data));

    CString expected;

    switch (stype)
    {
    case hex::SType::S1:
        // S1, address is 2 bytes
        expected = "S10B0000112233445566778890\n";
        break;

    case hex::SType::S2:
        // S2, address is 3 bytes
        expected = "S20C00000011223344556677888F\n";
        break;

    case hex::SType::S3:
        // S3, address is 3 bytes
        expected = "S30D0000000011223344556677888E\n";
        break;

    default:
        FAIL("unexpected stype!");
    }

    // note: we can't expect the S5 record here since that's written
    // by the destructor, and letting that run would close the stream.

    CString actual = File::ReadAllText(*pStream);

    REQUIRE(expected == actual);
}


TEST_CASE("SRecordExporter::WriteData - addresses")
{
    const bool overridePutAddress = GENERATE(false, true);

    auto stream = std::make_unique<CMemFile>();
    CMemFile* pStream = stream.get();

    hex::SRecordExporter writer{ std::move(stream), hex::SType::S1, 0x8000, 8 };

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
            "S10BFFFF112233445566778892\n";
    }
    else
    {
        writer.WriteData(data, sizeof(data));

        // did not override the address, so the second record
        // is immediately after the first
        expectedSecondRecord =
            "S10B8008112233445566778808\n";
    }

    CString expected =
        "S10B8000112233445566778810\n"
        + expectedSecondRecord;

    CString actual = File::ReadAllText(*pStream);

    REQUIRE(expected == actual);
}
