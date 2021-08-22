#include "Stdafx.h"
#include "utils/Garbage.h"
#include "utils/TestFiles.h"

#include "CFile64.h"

#include <catch.hpp>

#include <cstdint>

TEST_CASE("CFile64::CFile64()")
{
    CFile64 file;
    garbage_fill_and_construct<CFile64>(file);

    SECTION("CFile64 query functions")
    {
        CHECK(file.GetHandle() == INVALID_HANDLE_VALUE);
        CHECK(file.GetLength() == 0);

        CHECK(file.Duplicate() == nullptr);
        CHECK(file.GetFileName() == CString{ "" });
        CHECK(file.GetFilePath() == CString{ "" });
        CHECK(file.GetFileTitle() == CString{ "" });

        BY_HANDLE_FILE_INFORMATION info;
        CHECK(file.GetInformation(info) == FALSE);

        CHECK(file.SectorSize() == 0);
        CHECK(file.GetPosition() == -1);
        CHECK(file.GetSecurityAttributes() != nullptr);
        CHECK(file.GetSecurityDescriptor() != nullptr);

        CFileStatus status;
        CHECK(file.GetStatus(status) == FALSE);
    }

    SECTION("CFile64 read functions")
    {
        std::uint8_t buffer[16];
        CHECK(file.Read(buffer, sizeof(buffer)) == 0);

        CByteArray buffer2{};
        CHECK(file.Read(buffer2, 16) == 0);
    }

    SECTION("CFile64 seek functions")
    {
        CHECK(file.Seek(16, CFile64::begin) == -1);
        // expect that position is unchanged
        CHECK(file.GetPosition() == -1);

        file.SeekToBegin();
        // expect that position is unchanged
        CHECK(file.GetPosition() == -1);

        file.SeekToEnd();
        // expect that position is unchanged
        CHECK(file.GetPosition() == -1);
    }

    SECTION("CFile64 setters")
    {
        CHECK(file.SetEndOfFile(16) == FALSE);

        CHECK_NOTHROW([&]() {
            try
            {
                file.SetLength(16);
                FAIL("Expected CFileException");
            }
            catch (CFileException* ex)
            {
                // expected
                ex->Delete();
            }
            catch (CException* ex)
            {
                ex->Delete();
                FAIL("Expected CFileException");
            }
        });

        // note: SetFilePath _is not_ the setter for the value that GetFilePath returns!
        const CString testPath = _T("TEST TEST");
        file.SetFilePath(testPath);
        CHECK(file.GetFileName() == testPath);
    }
}

TEST_CASE("CFile64::CFile64(filename, open_flags)")
{
    CFile64 file;

    CString fname;
    CString path = TestFiles::Get256FilePath(&fname);

    CString fullpath;
    ::GetFullPathName(path, MAX_PATH + 1, fullpath.GetBuffer(MAX_PATH + 1), NULL);
    fullpath.ReleaseBuffer();

    garbage_fill_and_construct<CFile64>(file, path, (UINT)CFile64::modeRead);

    SECTION("CFile64 query functions")
    {
        CHECK(file.GetHandle() != INVALID_HANDLE_VALUE);
        CHECK(static_cast<HFILE>(file) != (HFILE)INVALID_HANDLE_VALUE);
        CHECK(file.GetLength() == 256);

        {
            CFile64* dup = file.Duplicate();
            CHECK(dup != nullptr);
            delete dup;
        }

        CHECK(file.GetFileName() == fname);
        CHECK(file.GetFilePath() == fullpath);
        CHECK(file.GetFileTitle() == fname);

        BY_HANDLE_FILE_INFORMATION info;
        CHECK(file.GetInformation(info) == TRUE);
        // most info values are provided by the OS with no good linkage
        // to values we can test against, so just test the file size.
        CHECK(info.nFileSizeHigh == 0);
        CHECK(info.nFileSizeLow == 256);

        CHECK(file.SectorSize() != 0);
        CHECK(file.GetPosition() == 0);
        CHECK(file.GetSecurityAttributes() != nullptr);
        CHECK(file.GetSecurityDescriptor() != nullptr);

        CFileStatus status;
        CHECK(file.GetStatus(status) == TRUE);
        // same as GetInformation...
        CHECK(status.m_size == 256);
        CHECK(CString{ status.m_szFullName } == path);
    }

    SECTION("CFile64 read functions")
    {
        std::uint8_t buffer[16];
        CHECK(file.Read(buffer, sizeof(buffer)) == 16);
        for (int i = 0; i < 16; i++)
        {
            CHECK(buffer[i] == i);
        }

        CByteArray buffer2{};
        CHECK(file.Read(buffer2, 16) == 16);
        for (int i = 0; i < 16; i++)
        {
            CHECK(buffer2[i] == i + 16);
        }
    }

    SECTION("CFile64 seek functions")
    {
        CHECK(file.Seek(16, CFile64::begin) == 16);
        CHECK(file.GetPosition() == 16);

        file.SeekToBegin();
        CHECK(file.GetPosition() == 0);

        file.SeekToEnd();
        CHECK(file.GetPosition() == 256);
    }

    SECTION("CFile64 setters")
    {
        CHECK(file.SetEndOfFile(16) == FALSE);

        CHECK_NOTHROW([&]() {
            try
            {
                file.SetLength(16);
                FAIL("Expected CFileException");
            }
            catch (CFileException* ex)
            {
                // expected
                ex->Delete();
            }
            catch (CException* ex)
            {
                ex->Delete();
                FAIL("Expected CFileException");
            }
        });

        // note: SetFilePath _is not_ the setter for the value that GetFilePath returns!
        const CString testPath = _T("TEST TEST");
        file.SetFilePath(testPath);
        CHECK(file.GetFileName() == testPath);
    }
}

TEST_CASE("CFile64 Open")
{
    CString fname;
    CString path = TestFiles::Get256FilePath(&fname);

    CString fullpath;
    ::GetFullPathName(path, MAX_PATH + 1, fullpath.GetBuffer(MAX_PATH + 1), NULL);
    fullpath.ReleaseBuffer();

    CFile64 file;
    REQUIRE(file.Open(path, CFile64::modeRead) == TRUE);

    CHECK(file.GetPosition() == 0);
    CHECK(file.GetLength() == 256);
}

TEST_CASE("CFile64 Write")
{
    CFile64 file{ TestFiles::GetMutableFilePath(), CFile64::modeReadWrite };

    file.SetLength(0);
    file.Write("123456789", 9);
    file.Flush();

    REQUIRE(file.GetLength() == 9);

    file.SeekToBegin();

    uint8_t buffer[9];
    REQUIRE(file.Read(buffer, 9) == 9);

    for (int i = 0; i < 9; i++)
    {
        CHECK(buffer[i] == '1' + i);
    }
}

TEST_CASE("CFile64::SetLength")
{
    CFile64 file{ TestFiles::GetMutableFilePath(), CFile64::modeReadWrite };

    file.SetLength(256);
    REQUIRE(file.GetLength() == 256);

    file.SetLength(0);
    REQUIRE(file.GetLength() == 0);
}
