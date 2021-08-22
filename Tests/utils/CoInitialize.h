#pragma once

#include <Windows.h>

// adapted from: Raymond Chen, The Old New Thing,
// https://devblogs.microsoft.com/oldnewthing/20040520-00/?p=39243
class CCoInitialize
{
public:
    CCoInitialize(COINIT init = COINIT_APARTMENTTHREADED) :
        m_hr{ CoInitializeEx(nullptr, init) }
    { }

    ~CCoInitialize()
    {
        if (SUCCEEDED(m_hr))
        {
            CoUninitialize();
        }
    }

    operator HRESULT() const { return m_hr; }

    HRESULT m_hr;
};
