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
}
