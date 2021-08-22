#include "Stdafx.h"
#include "TestFiles.h"
#include "File.h"

#include <cstdint>
#include <fstream>

namespace TestFiles
{
    static CString GetTestFilesDir()
    {
        static CString _testFilesDir;

        if (_testFilesDir.IsEmpty())
        {
            _testFilesDir = R"(TestFiles)";
            CreateDirectory(_testFilesDir, NULL);
        }

        return _testFilesDir;
    }

    CString Get256FilePath(CString* filename)
    {
        static CString _path;
        static const CString _filename = "testfile.256";

        if (_path.IsEmpty())
        {
            _path = GetTestFilesDir() + "\\" + _filename;

            std::uint8_t buf[256];
            for (int i = 0; i < 256; i++)
            {
                buf[i] = i;
            }

            std::ofstream stream{ _path, std::ios::binary };
            stream.write((const char*)buf, 256);
        }

        if (filename)
        {
            *filename = _filename;
        }

        return _path;
    }

    CString GetMutableFilePath()
    {
        static CString _path;

        if (_path.IsEmpty())
        {
            _path = GetTestFilesDir() + "\\" + "testfile.writable";

            std::ofstream stream{ _path, std::ios::binary };
            stream.write("TEST", 4);
        }

        // make sure it's not read only.
        file_attrs attrs = File::GetAttributes(_path);
        File::SetAttributes(_path, attrs & ~file_attrs::readonly);

        return _path;
    }
}
