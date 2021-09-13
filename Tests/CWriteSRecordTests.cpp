#include "Stdafx.h"
#include "utils/AssertHelpers.h"
#include "utils/File.h"
#include "utils/Garbage.h"
#include "utils/TestFiles.h"

#include "SRecord.h"

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


TEST_CASE("CWriteSRecord constructors")
{
    std::unique_ptr<CWriteSRecord> writer;

    SECTION("from file path");
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
