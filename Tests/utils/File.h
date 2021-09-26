#pragma once
#include <afx.h>
#include <afxstr.h>

enum file_attrs
{
    none      = 0,
    readonly  = (1 << 0),
    directory = (1 << 1),
};
inline file_attrs operator~(file_attrs x) { return (file_attrs)(~((int)x)); }

inline file_attrs operator|(file_attrs x, file_attrs y) { return (file_attrs)((int)x | (int)y); }
inline file_attrs& operator|=(file_attrs& x, file_attrs y) { x = (file_attrs)((int)x | (int)y); return x; }

inline file_attrs operator&(file_attrs x, file_attrs y) { return (file_attrs)((int)x & (int)y); }
inline file_attrs& operator&=(file_attrs& x, file_attrs y) { x = (file_attrs)((int)x & (int)y); return x; }

inline file_attrs operator^(file_attrs x, file_attrs y) { return (file_attrs)((int)x ^ (int)y); }
inline file_attrs& operator^=(file_attrs& x, file_attrs y) { x = (file_attrs)((int)x ^ (int)y); return x; }

namespace File
{
    /// \brief  Reads the entirety of the given file as text.
    ///
    /// \param  path           The path to the file to read.
    /// \param  normalizeEOLs  If true, EOL sequences will be normalized to just LF.
    CString ReadAllText(CString path, bool normalizeEOLs = false);

    /// \brief  Reads the entirety of the given stream as text.
    ///
    /// \param  stream       The stream to read.
    /// \param  seekToStart  If true, the stream will be seeked to the beginning.
    CString ReadAllText(CFile& stream, bool seekToStart = true);

    file_attrs GetAttributes(CString path);
    void SetAttributes(CString path, file_attrs attrs);
}
