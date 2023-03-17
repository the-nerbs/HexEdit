#pragma once
#include <exception>
#include <string>
#include <utility>

namespace hex
{
    /// \brief  Indicates a cryptography error occurred.
    class cryptography_error : public std::exception
    {
    public:
        explicit cryptography_error(const char* const message)
            : _message{ message }
        { }

        explicit cryptography_error(std::string message)
            : _message{ std::move(message) }
        { }

        const char* what() const override { return _message.c_str(); }

    private:
        std::string _message;
    };
}
