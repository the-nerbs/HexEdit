#pragma once
#include <afxstr.h>

namespace TestFiles
{
    /// \brief  Gets the path to a file containing 256 bytes with values 0 to 255.
    ///
    /// \param  filename  If not null, returns just the filename portion of the path.
    CString Get256FilePath(CString* filename = nullptr);


    /// \brief  Gets the path to a mutable file containing 256 bytes with values 0 to 255.
    ///
    /// \param  filename  If not null, returns just the filename portion of the path.
    /// 
    /// \remarks
    /// The content of this file can not be assumed by any test.
    CString GetMutableFilePath();


    /// \brief  Gets the path to the SRecords file.
    /// 
    /// \remarks
    /// The contents of this file match the example on Wikipedia:
    /// https://en.wikipedia.org/wiki/SREC_(file_format)#16-bit_memory_address
    /// Retrieved 6 Sept 2021.
    CString GetSRecordsFilePath();
}
