#pragma once
#include "IDialogProvider.h"

namespace hex
{
    class DialogProvider : public IDialogProvider
    {
    public:
        bool GetInteger(
            const CString& prompt,
            std::int64_t& result,
            std::int64_t min = INT64_MIN,
            std::int64_t max = INT64_MAX) override;

        bool GetString(const CString& prompt, CString& result) override;

        bool GetBoolean(
            const CString& prompt,
            const CString& trueText,
            const CString& falseText) override;
    };
}
