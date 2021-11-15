#pragma once
#include <afxstr.h>

#include <cstdint>
#include <limits>

namespace hex
{
    /// \brief  Interface class for types that allow prompting the user for a value.
    class IDialogProvider
    {
    public:
        virtual ~IDialogProvider() = default;

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
