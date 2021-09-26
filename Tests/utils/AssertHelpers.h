#pragma once
#include <cstddef>
#include <sstream>
#include <string>

#include "catch.hpp"

namespace test
{
    /// \brief  Encapsulates the result of a test assertion and any message it may produce.
    struct assert_result
    {
        bool passed;            //!< True if the assertion passed, or false if not.
        std::string message;    //!< A message describing the result. Typically only set on failure.


        assert_result()
            : passed{ true }, message{}
        { }

        explicit assert_result(bool passed)
            : passed{ passed }, message{}
        { }

        explicit assert_result(std::string message)
            : passed{ false }, message{ std::move(message) }
        { }

        assert_result(const assert_result&) = default;
        assert_result(assert_result&&) = default;

        explicit operator bool() const
        {
            return passed;
        }
    };


    /// \brief  Allows for converting types for assert messages.
    template <typename T>
    struct for_message
    {
        static T mutate(T t)
        {
            return t;
        }
    };

    // print bytes as they're integer values, rather than their character values.
    //TODO (cpp17): what about std::byte?
    template <>
    struct for_message<std::uint8_t>
    {
        static int mutate(std::uint8_t t)
        {
            return t;
        }
    };


    /// \brief  Determines if two arrays are equal.
    ///
    /// \param  expected  A pointer to the expected data.
    /// \param  actual    A pointer to the actual data.
    /// \param  length    The number of elements to compare.
    /// \param  comparer  An object defining the comparison.
    /// 
    /// \returns  An object describing the result of the comparison.
    template <typename T, typename Comparer = std::equal_to<T>>
    assert_result areEqual(
        const T* expected,
        const T* actual,
        std::size_t length,
        Comparer comparer = Comparer{})
    {
        for (std::size_t i = 0; i < length; i++)
        {
            if (!comparer(expected[i], actual[i]))
            {
                std::stringstream buffer;
                buffer << "Elements at index " << i << " are not equal. Expected<"
                       << for_message<T>::mutate(expected[i]) << ">. Actual<"
                       << for_message<T>::mutate(actual[i]) << ">.";
                return assert_result{ buffer.str() };
            }
        }

        return assert_result{};
    }

    /// \brief  Determines if two arrays are equal.
    ///
    /// \param  expected  A pointer to the expected data.
    /// \param  actual    A pointer to the actual data.
    /// \param  length    The number of elements to compare.
    /// \param  comparer  An object defining the comparison.
    /// 
    /// \returns  An object describing the result of the comparison.
    template <typename T, std::size_t N, typename Comparer = std::equal_to<T>>
    assert_result areEqual(
        const T (&expected)[N],
        const T* actual,
        std::size_t length,
        Comparer comparer = Comparer{})
    {
        using namespace std::string_literals;

        if (length < N)
        {
            return assert_result{ "actual is shorter than expected."s };
        }

        return areEqual(&expected[0], actual, length, comparer);
    }

    /// \brief  Determines if two arrays are equal.
    ///
    /// \param  expected  A pointer to the expected data.
    /// \param  actual    A pointer to the actual data.
    /// \param  comparer  An object defining the comparison.
    /// 
    /// \returns  An object describing the result of the comparison.
    template <typename T, std::size_t N1, std::size_t N2, typename Comparer = std::equal_to<T>>
    assert_result areEqual(
        const T (&expected)[N1],
        const T (&actual)[N2],
        Comparer comparer = Comparer{})
    {
        using namespace std::string_literals;

        if (N2 < N1)
        {
            return assert_result{ "actual is shorter than expected."s };
        }

        return areEqual(&expected[0], &actual[0], N1, comparer);
    }
}

// override Catch2's stringification for assert_result.
// According to the docs, I should be able to override `operator<<` for this, but the
// `operator bool()` conversion seems to take precedence, even if declared explicit, 
// meaning it always prints just true/false instead of the message. In case this is
// some compiler bug, the compiler I'm using is MSVC 19.29.30040
namespace Catch
{
    template<>
    struct StringMaker<test::assert_result>
    {
        static std::string convert(const test::assert_result& value)
        {
            return value.message;
        }
    };
}
