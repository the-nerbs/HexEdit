#include <afx.h>


class CErrorFile : public CMemFile
{
public:
    enum ErrorOn
    {
        noError = 0,
        readError  = (1 << 0),
        writeError = (1 << 1),
        openError  = (1 << 2),
        closeError = (1 << 3),
    };


    explicit CErrorFile(int errorFlags);

    ~CErrorFile();

    BOOL Open(LPCTSTR lpszFileName, UINT nOpenFlags, CAtlTransactionManager* pTM, CFileException* pError) override;
    void Close() override;

    UINT Read(void* lpBuf, UINT nCount) override;
    void Write(const void* lpBuf, UINT nCount) override;

    bool readThrows;
    bool writeThrows;
    bool openThrows;
    bool closeThrows;
};
