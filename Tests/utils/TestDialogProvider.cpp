#include "Stdafx.h"
#include "TestDialogProvider.h"



hex::MessageBoxResult TestDialogProvider::ShowMessageBox(
    const CString& message,
    hex::MessageBoxButtons buttons,
    hex::MessageBoxIcon icon)
{
    showMessageBoxCalls.push_back(ShowMessageBoxCall{
        message, buttons, icon
    });

    return messageBoxResult;
}

bool TestDialogProvider::GetInteger(
    const CString& prompt,
    std::int64_t& result,
    std::int64_t min,
    std::int64_t max)
{
    getIntegerCalls.push_back(GetIntegerCall{
        prompt, result, min, max
    });

    if (!cancel)
    {
        // just yield the given value
        return true;
    }

    return false;
}

bool TestDialogProvider::GetString(
    const CString& prompt,
    CString& result)
{
    getStringCalls.push_back(GetStringCall{
        prompt, result
    });

    if (!cancel)
    {
        // just yield the given value
        return true;
    }

    return false;
}

bool TestDialogProvider::GetBoolean(
    const CString& prompt,
    const CString& trueText,
    const CString& falseText)
{
    getBooleanCalls.push_back(GetBooleanCall{
        prompt, trueText, falseText
    });

    if (!cancel)
    {
        return true;
    }

    return false;
}
