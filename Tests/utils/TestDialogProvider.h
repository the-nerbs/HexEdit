#pragma once
#include "Services/IDialogProvider.h"

#include <vector>

struct ShowMessageBoxCall
{
    CString message;
    hex::MessageBoxButtons buttons;
    hex::MessageBoxIcon icon;
};

struct GetIntegerCall
{
    CString prompt;
    std::int64_t value;
    std::int64_t min;
    std::int64_t max;
};

struct GetStringCall
{
    CString prompt;
    CString value;
};

struct GetBooleanCall
{
    CString prompt;
    CString trueText;
    CString falseText;
};

class TestDialogProvider : public hex::IDialogProvider
{
public:
    bool cancel = false;
    hex::MessageBoxResult messageBoxResult;

    std::vector<ShowMessageBoxCall> showMessageBoxCalls;
    std::vector<GetIntegerCall> getIntegerCalls;
    std::vector<GetStringCall> getStringCalls;
    std::vector<GetBooleanCall> getBooleanCalls;

    hex::MessageBoxResult ShowMessageBox(
        const CString& message,
        hex::MessageBoxButtons buttons,
        hex::MessageBoxIcon icon) override;
    bool GetInteger(const CString& prompt, std::int64_t& result, std::int64_t min = INT64_MIN, std::int64_t max = INT64_MAX) override;
    bool GetString(const CString& prompt, CString& result) override;
    bool GetBoolean(const CString& prompt, const CString& trueText, const CString& falseText) override;
};
