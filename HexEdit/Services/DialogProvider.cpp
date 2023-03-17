#include "Stdafx.h"
#include "DialogProvider.h"

#include "../Dialog.h"

#include <afxwin.h>

#include <cassert>

namespace hex
{
    MessageBoxResult DialogProvider::ShowMessageBox(
        const CString& message,
        MessageBoxButtons buttons,
        MessageBoxIcon icon)
    {
        static constexpr UINT mfcButtons[] =
        {
            MB_OK,                      // Ok
            MB_OKCANCEL,                // OkCancel
            MB_ABORTRETRYIGNORE,        // AbortRetryIgnore
            MB_YESNOCANCEL,             // YesNoCancel
            MB_YESNO,                   // YesNo
            MB_RETRYCANCEL,             // RetryCancel
        };

        static constexpr UINT mfcIcons[] =
        {
            0,                          // None
            MB_ICONHAND,                // Hand
            MB_ICONQUESTION,            // Question
            MB_ICONEXCLAMATION,         // Exclamation
            MB_ICONASTERISK,            // Asterisk
        };

        static constexpr MessageBoxResult resultsFromMfc[] =
        {
            MessageBoxResult::Close,    // error
            MessageBoxResult::Ok,       // IDOK,
            MessageBoxResult::Cancel,   // IDCANCEL,
            MessageBoxResult::Abort,    // IDABORT,
            MessageBoxResult::Retry,    // IDRETRY,
            MessageBoxResult::Ignore,   // IDIGNORE,
            MessageBoxResult::Yes,      // IDYES,
            MessageBoxResult::No,       // IDNO,
        };


        const UINT type = mfcButtons[static_cast<int>(buttons)]
            | mfcIcons[static_cast<int>(icon)];

        int result = AfxMessageBox(message, type);

        if (static_cast<unsigned>(result) < (sizeof(resultsFromMfc) / sizeof(*resultsFromMfc)))
        {
            return resultsFromMfc[result];
        }

        // we should never be getting a status back that we didn't
        // pass to the call in the first place.
        assert(0);
        return MessageBoxResult::Close;
    }

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
