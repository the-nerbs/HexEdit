#include "Stdafx.h"
#include "ErrorFile.h"

CErrorFile::CErrorFile(int errorFlags) :
    CMemFile{ 1024u },
    readThrows{ (errorFlags & readError) != 0 },
    writeThrows{ (errorFlags & writeError) != 0 },
    openThrows{ (errorFlags & openError) != 0 },
    closeThrows{ (errorFlags & closeError) != 0 }
{ }

CErrorFile::~CErrorFile()
{
    // don't throw from dtor, it tends to lead to std::terminate
    closeThrows = false;
    Close();
}

BOOL CErrorFile::Open(
    LPCTSTR lpszFileName,
    UINT nOpenFlags,
    CAtlTransactionManager* pTM,
    CFileException* pError)
{
    if (openThrows)
    {
        CFileException* ex = new CFileException(CFileException::genericException);

        if (pError)
        {
            pError = ex;
            return FALSE;
        }
        else
        {
            THROW(ex);
        }
    }

    return CMemFile::Open(lpszFileName, nOpenFlags, pTM, pError);
}

void CErrorFile::Close()
{
    CMemFile::Close();

    if (closeThrows)
    {
        AfxThrowFileException(CFileException::genericException);
    }
}

UINT CErrorFile::Read(void* lpBuf, UINT nCount)
{
    if (readThrows)
    {
        AfxThrowFileException(CFileException::genericException);
    }

    return CMemFile::Read(lpBuf, nCount);
}

void CErrorFile::Write(const void* lpBuf, UINT nCount)
{
    if (writeThrows)
    {
        AfxThrowFileException(CFileException::genericException);
    }

    CMemFile::Write(lpBuf, nCount);
}
