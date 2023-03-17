#pragma once
#include <afxstr.h>

#include <cstdint>
#include <limits>

namespace hex
{
    enum class MessageBoxButtons
    {
        Ok,
        OkCancel,
        AbortRetryIgnore,
        YesNoCancel,
        YesNo,
        RetryCancel,
    };

    enum class MessageBoxIcon
    {
        None,
        Hand,
        Question,
        Exclamation,
        Asterisk,
    };

    enum class MessageBoxResult
    {
        Ok,
        Cancel,
        Abort,
        Retry,
        Ignore,
        Yes,
        No,
        Close,
    };

    /// \brief  Interface class for types that show dialogs.
    class IDialogProvider
    {
    public:
        virtual ~IDialogProvider() = default;

        /// \brief  Shows a message to the user with the given set of buttons.
        ///
        /// \param       message  The message to show.
        /// \param[opt]  buttons  The buttons to show. Defaults to only an OK button.
        /// \param[opt]  icon     The icon to show. Defaults to no icon.
        /// 
        /// \returns  The button that was pressed or \p MessageBoxResult::Close if the dialog was closed.
        virtual MessageBoxResult ShowMessageBox(
            const CString& message,
            MessageBoxButtons buttons = MessageBoxButtons::Ok,
            MessageBoxIcon icon = MessageBoxIcon::None) = 0;

        /// \brief  Gets an integer from the user.
        /// 
        /// \param  prompt   The prompt message.
        /// \param  result   The value from the user. Also used as the value that is initially shown to the user.
        /// \param  min,max  The minimum and maximum allowed values.
        /// 
        /// \returns  True if the user entered a value, or false if the user canceled.
        virtual bool GetInteger(
            const CString& prompt,
            std::int64_t& result,
            std::int64_t min = INT64_MIN,
            std::int64_t max = INT64_MAX) = 0;

        /// \brief  Gets a string from the user.
        /// 
        /// \param  prompt   The prompt message.
        /// \param  result   The value from the user. Also used as the value that is initially shown to the user.
        /// 
        /// \returns  True if the user entered a value, or false if the user canceled.
        virtual bool GetString(const CString& prompt, CString& result) = 0;

        /// \brief  Gets a boolean from the user.
        /// 
        /// \param  prompt     The prompt message.
        /// \param  trueText   If not empty, the text to show on the true button.
        /// \param  falseText  If not empty, the text to show on the false button.
        /// 
        /// \returns  True if the user selected true, or false if the user
        ///           selected the false button or canceled.
        virtual bool GetBoolean(
            const CString& prompt,
            const CString& trueText,
            const CString& falseText) = 0;
    };
}
