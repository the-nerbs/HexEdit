#include "Stdafx.h"
#include "DialogProvider.h"

#include "../Dialog.h"

namespace hex
{
    bool DialogProvider::GetInteger(
        const CString& prompt,
        std::int64_t& result,
        std::int64_t min,
        std::int64_t max)
    {
        GetInt dialog{};

        dialog.prompt_ = prompt;
        dialog.value_ = result;
        dialog.min_ = min;
        dialog.max_ = max;

        bool accepted = dialog.DoModal() == IDOK;

        if (accepted)
        {
            result = dialog.value_;
            return true;
        }

        return false;
    }

    bool DialogProvider::GetString(const CString& prompt, CString& result)
    {
        GetStr dialog{};

        dialog.prompt_ = prompt;
        dialog.value_ = result;

        bool accepted = dialog.DoModal() == IDOK;

        if (accepted)
        {
            result = dialog.value_;
            return true;
        }

        return false;
    }

    bool DialogProvider::GetBoolean(
        const CString& prompt,
        const CString& trueText,
        const CString& falseText)
    {
        GetBool dialog{};

        dialog.prompt_ = prompt;

        if (!trueText.IsEmpty())
        {
            dialog.yes_ = trueText;
        }

        if (!falseText.IsEmpty())
        {
            dialog.no_ = falseText;
        }

        return dialog.DoModal() == IDOK;
    }
}
