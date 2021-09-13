#include "Stdafx.h"
#include "File.h"

#include <fstream>
#include <strstream>
#include <system_error>
#include <utility>

namespace File
{
    CString ReadAllText(CString path, bool normalizeEOLs)
    {
        std::ios::openmode mode = std::ios::in;

        if (!normalizeEOLs)
        {
            mode |= std::ios::binary;
        }

        std::fstream in{ path, mode };

        if (!in)
        {
            __debugbreak();
        }

        in.seekg(0, std::ios::end);
        int length = static_cast<int>(in.tellg());
        in.seekg(0, std::ios::beg);

        CString str;
        LPSTR pdata = str.GetBuffer(length + 1);
        in.read(pdata, length);
        int readCount = static_cast<int>(in.gcount());
        pdata[readCount] = '\0';
        str.ReleaseBuffer();

        return str;
    }


    static const std::pair<file_attrs, DWORD> fileAttrMap[]
    {
        { file_attrs::none, 0 },
        { file_attrs::readonly, FILE_ATTRIBUTE_READONLY },
        { file_attrs::directory, FILE_ATTRIBUTE_DIRECTORY },
    };

    file_attrs GetAttributes(CString path)
    {
        DWORD platformAttrs = GetFileAttributes(path);

        file_attrs attrs = file_attrs::none;

        for (const auto& mapping : fileAttrMap)
        {
            if (platformAttrs & mapping.second)
            {
                attrs |= mapping.first;
            }
        }

        return attrs;
    }

    void SetAttributes(CString path, file_attrs attrs)
    {
        DWORD platformAttrs = 0;

        for (const auto& mapping : fileAttrMap)
        {
            if ((attrs & mapping.first) != file_attrs::none)
            {
                platformAttrs |= mapping.second;
            }
        }

        if (!SetFileAttributes(path, platformAttrs))
        {
            DWORD error = GetLastError();
            throw std::system_error{ static_cast<int>(error), std::system_category() };
        }
    }
}
