#include "Stdafx.h"
#include "utils/Garbage.h"

#include "FindDlg.h"

#include "Boyer.h"

#include <catch.hpp>

#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <system_error>

template <CFindSheet::charset_t charset>
struct charset_info;

template<>
struct charset_info<CFindSheet::RB_CHARSET_ASCII>
{
    static constexpr int value = 1;
    static constexpr int char_size = 1;

    static std::unique_ptr<std::uint8_t[]> map_ascii(const char* psz, std::size_t& byteLengthWithoutNulTerm, BOOL toggleCase = FALSE)
    {
        assert(psz);

        std::size_t length = std::strlen(psz);

        auto mapped = std::make_unique<std::uint8_t[]>(length+1);
        if (toggleCase == FALSE)
        {
            std::memcpy(mapped.get(), psz, length);
        }
        else
        {
            for (size_t i = 0; i < length; i++)
            {
                char ch = psz[i];

                if (std::isupper(ch))
                {
                    ch = std::tolower(ch);
                }
                else if (std::islower(ch))
                {
                    ch = std::toupper(ch);
                }
                // else: not a letter, leave it alone

                mapped[i] = ch;
            }
        }
        mapped[length] = '\0';

        byteLengthWithoutNulTerm = length;
        return mapped;
    }
};

template<>
struct charset_info<CFindSheet::RB_CHARSET_UNICODE>
{
    static constexpr int value = 2;
    static constexpr int char_size = 2;

    static std::unique_ptr<std::uint8_t[]> map_ascii(const char* psz, std::size_t& byteLengthWithoutNulTerm, BOOL toggleCase = FALSE)
    {
        assert(psz);

        int convertedLength = MultiByteToWideChar(CP_ACP, 0, psz, -1, NULL, 0);

        if (convertedLength > 0)
        {
            // note: since we pass -1 for the source length, the converted
            // length includes the NUL terminator.
            std::size_t bufferSize = convertedLength * sizeof(wchar_t);

            auto buffer = std::make_unique<std::uint8_t[]>(bufferSize);
            std::memset(buffer.get(), 0, bufferSize);

            wchar_t* wszBuffer = reinterpret_cast<wchar_t*>(buffer.get());

            int result = MultiByteToWideChar(CP_ACP, 0, psz, -1, wszBuffer, convertedLength);
            if (result > 0)
            {
                if (toggleCase != FALSE)
                {
                    for (size_t i = 0; i < convertedLength; i++)
                    {
                        wchar_t ch = wszBuffer[i];

                        if (iswupper(ch))
                        {
                            ch = towlower(ch);
                        }
                        else if (iswlower(ch))
                        {
                            ch = towupper(ch);
                        }

                        wszBuffer[i] = ch;
                    }
                }

                byteLengthWithoutNulTerm = std::wcslen(wszBuffer) * sizeof(wchar_t);
                return buffer;
            }
        }
        throw std::system_error{ (int)GetLastError(), std::system_category(), "Failed to convert test string to wide chars." };
    }
};

template<>
struct charset_info<CFindSheet::RB_CHARSET_EBCDIC>
{
    static constexpr int value = 3;
    static constexpr int char_size = 1;

    static std::unique_ptr<std::uint8_t[]> map_ascii(const char* psz, std::size_t& byteLengthWithoutNulTerm, BOOL toggleCase = FALSE)
    {
        static const std::uint8_t ascii_to_ebcdic[128] =
        {
            // from: https://supportline.microfocus.com/documentation/books/sx51/cyebas.htm
            // 0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
            0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F, 0x16, 0x05, 0x25, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x10, 0x11, 0x12, 0x13, 0x3C, 0x3D, 0x32, 0x26, 0x18, 0x19, 0x3F, 0x27, 0x1C, 0x1D, 0x1E, 0x1F,
            0x40, 0x5A, 0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D, 0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61,
            0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F,
            0x7C, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
            0xD7, 0xD8, 0xD9, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xAD, 0xE0, 0xBD, 0x5F, 0x6D,
            0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
            0x97, 0x98, 0x99, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xC0, 0x6A, 0xD0, 0xA1, 0x07,
        };

        assert(psz);

        std::size_t length = std::strlen(psz);

        auto mapped = std::make_unique<std::uint8_t[]>(length+1);

        for (std::size_t i = 0; i < length; i++)
        {
            char ch = psz[i];

            if (toggleCase != FALSE)
            {
                if (std::isupper(ch))
                {
                    ch = std::tolower(ch);
                }
                else if (std::islower(ch))
                {
                    ch = std::toupper(ch);
                }
            }

            mapped[i] = ascii_to_ebcdic[ch];
        }

        mapped[length] = '\0';

        byteLengthWithoutNulTerm = length;
        return mapped;
    }
};

using ascii_info = charset_info<CFindSheet::RB_CHARSET_ASCII>;
using unicode_info = charset_info<CFindSheet::RB_CHARSET_UNICODE>;
using ebcdic_info = charset_info<CFindSheet::RB_CHARSET_EBCDIC>;


TEST_CASE("boyer constructors")
{
    const char pattern[] = "TEST";
    const std::size_t length = std::strlen(pattern);

    const std::uint8_t mask[4] = { 0xFF, 0x55, 0xAA, 0x00 };

    SECTION("boyer(pattern, length, NULL)")
    {
        boyer b{ (const std::uint8_t*)pattern, length, nullptr };

        CHECK(b.length() == length);

        REQUIRE(b.pattern() != nullptr);
        CHECK(b.pattern() != (const std::uint8_t*)&pattern[0]);
        CHECK(std::strncmp((const char*)b.pattern(), pattern, length) == 0);

        CHECK(b.mask() == nullptr);
    }

    SECTION("boyer(pattern, length, mask)")
    {
        boyer b{ (const std::uint8_t*)pattern, length, mask };

        CHECK(b.length() == length);
        
        REQUIRE(b.pattern() != nullptr);
        CHECK(b.pattern() != (const std::uint8_t*)&pattern[0]);
        CHECK(std::strncmp((const char*)b.pattern(), pattern, length) == 0);

        REQUIRE(b.mask() != nullptr);
        CHECK(b.mask() != &mask[0]);
        CHECK(std::memcmp(b.mask(), mask, sizeof(mask)) == 0);
    }

    SECTION("boyer(const boyer&)")
    {
        boyer source{ (const std::uint8_t*)pattern, length, mask };

        boyer b{ source };

        CHECK(b.length() == length);

        REQUIRE(b.pattern() != nullptr);
        CHECK(b.pattern() != (const std::uint8_t*)&pattern[0]);
        CHECK(std::strncmp((const char*)b.pattern(), pattern, length) == 0);

        REQUIRE(b.mask() != nullptr);
        CHECK(b.mask() != &mask[0]);
        CHECK(std::memcmp(b.mask(), mask, sizeof(mask)) == 0);
    }

    SECTION("boyer(const boyer&) - no mask")
    {
        boyer source{ (const std::uint8_t*)pattern, length, NULL };

        boyer b{ source };

        CHECK(b.length() == length);

        REQUIRE(b.pattern() != nullptr);
        CHECK(b.pattern() != (const std::uint8_t*)&pattern[0]);
        CHECK(std::strncmp((const char*)b.pattern(), pattern, length) == 0);

        CHECK(b.mask() == nullptr);
    }
}

TEST_CASE("Assignment operator")
{
    const char pattern[] = "TEST";
    const std::size_t length = std::strlen(pattern);

    const std::uint8_t mask[4] = { 0xFF, 0x55, 0xAA, 0x00 };
    boyer source{ (const std::uint8_t*)pattern, length, mask };

    SECTION("no mask")
    {
        boyer b{ (const std::uint8_t*)"TEST2", 5, nullptr };

        b = source;

        CHECK(b.length() == length);

        REQUIRE(b.pattern() != nullptr);
        CHECK(b.pattern() != (const std::uint8_t*)&pattern[0]);
        CHECK(std::strncmp((const char*)b.pattern(), pattern, length) == 0);

        REQUIRE(b.mask() != nullptr);
        CHECK(b.mask() != &mask[0]);
        CHECK(std::memcmp(b.mask(), mask, sizeof(mask)) == 0);
    }

    SECTION("with mask")
    {
        std::uint8_t mask2[] = { 0xFF, 0xFF, 0xAA, 0x55, 0x00 };
        boyer b{ (const std::uint8_t*)"TEST2", 5, mask2 };

        b = source;

        CHECK(b.length() == length);

        REQUIRE(b.pattern() != nullptr);
        CHECK(b.pattern() != (const std::uint8_t*)&pattern[0]);
        CHECK(std::strncmp((const char*)b.pattern(), pattern, length) == 0);

        REQUIRE(b.mask() != nullptr);
        CHECK(b.mask() != &mask[0]);
        CHECK(std::memcmp(b.mask(), mask, sizeof(mask)) == 0);
    }

    SECTION("assign to self")
    {
        std::uint8_t mask[] = { 0xFF, 0xFF, 0xAA, 0x55, 0x00 };
        boyer b{ (const std::uint8_t*)"TEST2", 5, mask };

        std::size_t prevLength = b.length();
        const std::uint8_t* prevPattern = b.pattern();
        const std::uint8_t* prevMask = b.mask();

        b = b;

        CHECK(b.length() == prevLength);
        CHECK(b.pattern() == prevPattern);
        CHECK(b.mask() == prevMask);
    }
}


// search without mask

TEMPLATE_TEST_CASE("boyer::findforw (no mask) - basic matching", "", ascii_info, unicode_info, ebcdic_info)
{
    std::size_t patternLength;
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii("TEST", patternLength);

    boyer b{ pattern.get(), patternLength, nullptr };

    SECTION("no match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdabcdabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("successful match at start of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TESTabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0 * TestType::char_size]);
    }

    SECTION("successful match in middle of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTESTabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match at end of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTEST", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match with multiple occurrences finds first")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTESTabcdTESTabcdTESTabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        // finds the first match
        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match case-insensitive")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdtEsTabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            TRUE,                           // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        // finds the first match
        CHECK(found == &teststr[4 * TestType::char_size]);
    }
}

TEMPLATE_TEST_CASE("boyer::findforw (no mask) - whole word", "", ascii_info, unicode_info, ebcdic_info)
{
    BOOL ignoreCase = GENERATE(FALSE, TRUE);
    INFO("ignoreCase = " << (ignoreCase == TRUE ? "true" : "false"));

    std::size_t patternLength;
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii("TEST", patternLength);

    boyer b{ pattern.get(), patternLength, nullptr };


    SECTION("successful match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd TEST abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[5 * TestType::char_size]);
    }


    SECTION("no match - bad boundaries")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTESTabcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("no match - bad left boundary")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTEST abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        if constexpr (std::is_same_v<TestType, unicode_info>)
        {
            // TODO(bugs): This test case seems to be hitting a bug where the left boundary isn't
            // being checked correctly for 'Unicode' text searches. I verified this in-app as well,
            // by creating a new file with the test string, and searching for the pattern with
            // 'whole word' and 'match case' enabled.
            WARN("!!! Possible bug in Unicode whole word search - is left boundary being checked correctly?");
            CHECK(found == &teststr[4 * TestType::char_size]);
        }
        else
        {
            CHECK(found == nullptr);
        }
    }

    SECTION("no match - bad right boundary")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd TESTabcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match - at start with alpha_before = FALSE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TEST abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0 * TestType::char_size]);
    }

    SECTION("no match - at start with alpha_before = TRUE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TEST abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            TRUE, FALSE,                    // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match - at end with alpha_after = FALSE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[5 * TestType::char_size]);
    }

    SECTION("no match - at end with alpha_after = TRUE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, TRUE,                    // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }
}

TEMPLATE_TEST_CASE("boyer::findforw (no mask) - alignment", "", ascii_info, unicode_info, ebcdic_info)
{
    BOOL ignoreCase = GENERATE(FALSE, TRUE);
    INFO("ignoreCase = " << (ignoreCase == TRUE ? "true" : "false"));

    std::size_t patternLength;
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii("TEST", patternLength);

    boyer b{ pattern.get(), patternLength, nullptr };

    SECTION("successful match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("  TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            2,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[2 * TestType::char_size]);
    }

    SECTION("successful match with address")
    {
        std::string testStrAscii = "TEST";
        int expectedFoundIndex = 0;
        int misalignBytes = 2;
        while (misalignBytes > 0)
        {
            testStrAscii.insert(testStrAscii.begin(), ' ');
            misalignBytes -= TestType::char_size;
            expectedFoundIndex++;
        }

        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii(testStrAscii.c_str(), teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            4,                              // alignment
            0,                              // offset
            4, 6                            // base address + address
        );

        CHECK(found == &teststr[expectedFoundIndex * TestType::char_size]);
    }

    SECTION("no match - bad alignment")
    {
        std::string testStrAscii = "TEST";
        int misalignBytes = 2;
        while (misalignBytes > 0)
        {
            testStrAscii.insert(testStrAscii.begin(), ' ');
            misalignBytes -= TestType::char_size;
        }

        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii(testStrAscii.c_str(), teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            4,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("not match - bad alignment from address")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            4,                              // alignment
            0,                              // offset
            0, 1                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("no match - bad alignment from base address")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            4,                              // alignment
            0,                              // offset
            1, 4                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match with offset")
    {
        std::string testStrAscii = "TEST";
        int expectedFoundIndex = 0;
        int misalignBytes = 2;
        while (misalignBytes > 0)
        {
            testStrAscii.insert(testStrAscii.begin(), ' ');
            misalignBytes -= TestType::char_size;
            expectedFoundIndex++;
        }

        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii(testStrAscii.c_str(), teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            4,                              // alignment
            1,                              // offset
            4, 7                            // base address + address
        );

        CHECK(found == &teststr[expectedFoundIndex * TestType::char_size]);
    }


    SECTION("no match - bad alignment from offset")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("  TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            2,                              // alignment
            1,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }
}


TEMPLATE_TEST_CASE("boyer::findback (no mask) - basic matching", "", ascii_info, unicode_info, ebcdic_info)
{
    std::size_t patternLength;
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii("TEST", patternLength);

    boyer b{ pattern.get(), patternLength, nullptr };

    SECTION("no match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdabcdabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("successful match at start of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TESTabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0 * TestType::char_size]);
    }

    SECTION("successful match in middle of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTESTabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match at end of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTEST", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match with multiple occurrences finds last")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTESTabcdTESTabcdTESTabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        // finds the last match
        CHECK(found == &teststr[20 * TestType::char_size]);
    }

    SECTION("successful match case-insensitive")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdtEsTabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            TRUE,                           // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        // finds the first match
        CHECK(found == &teststr[4 * TestType::char_size]);
    }

}

TEMPLATE_TEST_CASE("boyer::findback (no mask) - whole word", "", ascii_info, unicode_info, ebcdic_info)
{
    BOOL ignoreCase = GENERATE(FALSE, TRUE);
    INFO("ignoreCase = " << (ignoreCase == TRUE ? "true" : "false"));

    std::size_t patternLength;
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii("TEST", patternLength);

    boyer b{ pattern.get(), patternLength, nullptr };


    SECTION("successful match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd TEST abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[5 * TestType::char_size]);
    }


    SECTION("no match - bad boundaries")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTESTabcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("no match - bad left boundary")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdTEST abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        if constexpr (std::is_same_v<TestType, unicode_info>)
        {
            // TODO(bugs): This test case seems to be hitting a bug where the left boundary isn't
            // being checked correctly for 'Unicode' text searches. I verified this in-app as well,
            // by creating a new file with the test string, and searching for the pattern with
            // 'whole word' and 'match case' enabled.
            WARN("!!! Possible bug in Unicode whole word search - is left boundary being checked correctly?");
            CHECK(found == &teststr[4 * TestType::char_size]);
        }
        else
        {
            CHECK(found == nullptr);
        }
    }

    SECTION("no match - bad right boundary")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd TESTabcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match - at start with alpha_before = FALSE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TEST abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0 * TestType::char_size]);
    }

    SECTION("no match - at start with alpha_before = TRUE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TEST abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            TRUE, FALSE,                    // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match - at end with alpha_after = FALSE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[5 * TestType::char_size]);
    }

    SECTION("no match - at end with alpha_after = TRUE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, TRUE,                    // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }
}

TEMPLATE_TEST_CASE("boyer::findback (no mask) - alignment", "", ascii_info, unicode_info, ebcdic_info)
{
    BOOL ignoreCase = GENERATE(FALSE, TRUE);
    INFO("ignoreCase = " << (ignoreCase == TRUE ? "true" : "false"));

    std::size_t patternLength;
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii("TEST", patternLength);

    boyer b{ pattern.get(), patternLength, nullptr };

    SECTION("successful match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("  TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, TRUE,                    // alpha before/after
            2,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[2 * TestType::char_size]);
    }

    SECTION("successful match with address")
    {
        std::string testStrAscii = "TEST";
        int expectedFoundIndex = 0;
        int misalignBytes = 2;
        while (misalignBytes > 0)
        {
            testStrAscii.insert(testStrAscii.begin(), ' ');
            misalignBytes -= TestType::char_size;
            expectedFoundIndex++;
        }

        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii(testStrAscii.c_str(), teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, TRUE,                    // alpha before/after
            4,                              // alignment
            0,                              // offset
            4, 6                            // base address + address
        );

        CHECK(found == &teststr[expectedFoundIndex * TestType::char_size]);
    }

    SECTION("no match - bad alignment")
    {
        std::string testStrAscii = "TEST";
        int misalignBytes = 2;
        while (misalignBytes > 0)
        {
            testStrAscii.insert(testStrAscii.begin(), ' ');
            misalignBytes -= TestType::char_size;
        }

        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii(testStrAscii.c_str(), teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, TRUE,                    // alpha before/after
            4,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("not match - bad alignment from address")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, TRUE,                    // alpha before/after
            4,                              // alignment
            0,                              // offset
            0, 1                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("no match - bad alignment from base address")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, TRUE,                    // alpha before/after
            4,                              // alignment
            0,                              // offset
            1, 4                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match with offset")
    {
        std::string testStrAscii = "TEST";
        int expectedFoundIndex = 0;
        int misalignBytes = 2;
        while (misalignBytes > 0)
        {
            testStrAscii.insert(testStrAscii.begin(), ' ');
            misalignBytes -= TestType::char_size;
            expectedFoundIndex++;
        }

        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii(testStrAscii.c_str(), teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, TRUE,                    // alpha before/after
            4,                              // alignment
            1,                              // offset
            4, 7                            // base address + address
        );

        CHECK(found == &teststr[expectedFoundIndex * TestType::char_size]);
    }


    SECTION("no match - bad alignment from offset")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("  TEST", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, TRUE,                    // alpha before/after
            2,                              // alignment
            1,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }
}


// hex search with mask

TEST_CASE("boyer::findforw (mask) - hex - basic matching")
{
    std::uint8_t pattern[2] = { 0xFF, 0xFF };
    std::uint8_t mask[2] = { 0x0F, 0xF0, };

    boyer b{ pattern, 2, mask };


    SECTION("no match")
    {
        std::uint8_t teststr[] = { 0x00, 0xF0, 0x0F, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("successful match with exact mask bits")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match with all masked out bits set")
    {
        std::uint8_t teststr[] = { 0xFF, 0xFF };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match with left-hand masked out bits set")
    {
        std::uint8_t teststr[] = { 0xFF, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match with right-hand masked out bits set")
    {
        std::uint8_t teststr[] = { 0x0F, 0xFF };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match with some masked out bits set")
    {
        std::uint8_t teststr[] = { 0x5F, 0xFA };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match at start")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0, 0x00, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match in middle")
    {
        std::uint8_t teststr[] = { 0x00, 0x0F, 0xF0, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[1]);
    }

    SECTION("successful match at end")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x0F, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[2]);
    }

    SECTION("successful match with multiple occurrences finds first")
    {
        std::uint8_t teststr[] = { 0x00, 0x0F, 0xF0, 0x00, 0x0F, 0xF0, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[1]);
    }
}

TEST_CASE("boyer::findforw (mask) - hex - alignment")
{
    std::uint8_t pattern[2] = { 0xFF, 0xFF };
    std::uint8_t mask[2] = { 0x0F, 0xF0, };

    boyer b{ pattern, 2, mask };

    SECTION("successful match")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x0F, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            2,                      // alignment
            0,                      // offset
            0, 0                    // base address + address
        );

        CHECK(found == &teststr[2]);
    }

    SECTION("successful match with address")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x00, 0x0F, 0xF0, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            0,                      // offset
            1, 6                    // base address + address
        );

        CHECK(found == &teststr[3]);
    }

    SECTION("no match - bad alignment")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x0F, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            0,                      // offset
            0, 0                    // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("not match - bad alignment from address")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0, 0x00, 0x00  };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            0,                      // offset
            0, 1                    // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("no match - bad alignment from base address")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0, 0x00, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            0,                      // offset
            1, 4                    // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match with offset")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            1,                      // offset
            4, 7                    // base address + address
        );

        CHECK(found == &teststr[2]);
    }


    SECTION("no match - bad alignment from offset")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0, 0x00, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findforw(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            2,                      // alignment
            1,                      // offset
            0, 0                    // base address + address
        );

        CHECK(found == nullptr);
    }
}


TEST_CASE("boyer::findback (mask) - hex - basic matching")
{
    std::uint8_t pattern[2] = { 0xFF, 0xFF };
    std::uint8_t mask[2] = { 0x0F, 0xF0, };

    boyer b{ pattern, 2, mask };


    SECTION("no match")
    {
        std::uint8_t teststr[] = { 0x00, 0xF0, 0x0F, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("successful match with exact mask bits")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match with all masked out bits set")
    {
        std::uint8_t teststr[] = { 0xFF, 0xFF };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match with left-hand masked out bits set")
    {
        std::uint8_t teststr[] = { 0xFF, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match with right-hand masked out bits set")
    {
        std::uint8_t teststr[] = { 0x0F, 0xFF };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match with some masked out bits set")
    {
        std::uint8_t teststr[] = { 0x5F, 0xFA };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match at start")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0, 0x00, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[0]);
    }

    SECTION("successful match in middle")
    {
        std::uint8_t teststr[] = { 0x00, 0x0F, 0xF0, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[1]);
    }

    SECTION("successful match at end")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x0F, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[2]);
    }

    SECTION("successful match with multiple occurrences finds last")
    {
        std::uint8_t teststr[] = { 0x00, 0x0F, 0xF0, 0x00, 0x0F, 0xF0, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,        // string and length
            FALSE,                      // ignore case
            1,                          // char set
            FALSE,                      // match whole word
            FALSE, FALSE,               // alpha before/after
            1,                          // alignment
            0,                          // offset
            0, 0                        // base address + address
        );

        CHECK(found == &teststr[4]);
    }
}

TEST_CASE("boyer::findback (mask) - hex - alignment")
{
    std::uint8_t pattern[2] = { 0xFF, 0xFF };
    std::uint8_t mask[2] = { 0x0F, 0xF0, };

    boyer b{ pattern, 2, mask };

    SECTION("successful match")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x0F, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            2,                      // alignment
            0,                      // offset
            0, 0                    // base address + address
        );

        CHECK(found == &teststr[2]);
    }

    SECTION("successful match with address")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x00, 0x0F, 0xF0, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            0,                      // offset
            1, 6                    // base address + address
        );

        CHECK(found == &teststr[3]);
    }

    SECTION("no match - bad alignment")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x0F, 0xF0 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            0,                      // offset
            0, 0                    // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("not match - bad alignment from address")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0, 0x00, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            0,                      // offset
            0, 1                    // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("no match - bad alignment from base address")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0, 0x00, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            0,                      // offset
            1, 4                    // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match with offset")
    {
        std::uint8_t teststr[] = { 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            4,                      // alignment
            1,                      // offset
            4, 7                    // base address + address
        );

        CHECK(found == &teststr[2]);
    }


    SECTION("no match - bad alignment from offset")
    {
        std::uint8_t teststr[] = { 0x0F, 0xF0, 0x00, 0x00 };
        std::size_t teststrLen = _countof(teststr);

        const std::uint8_t* found = b.findback(
            teststr, teststrLen,    // string and length
            FALSE,                  // ignore case
            1,                      // char set
            FALSE,                  // match whole word
            FALSE, FALSE,           // alpha before/after
            2,                      // alignment
            1,                      // offset
            0, 0                    // base address + address
        );

        CHECK(found == nullptr);
    }
}


// text search with mask (wildcards)

TEMPLATE_TEST_CASE("boyer::findforw (mask) - text - basic matching", "", ascii_info, unicode_info, ebcdic_info)
{
    std::size_t patternLength;
    const char* asciiPattern = "M??E";
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii(asciiPattern, patternLength);
    auto mask = std::make_unique<std::uint8_t[]>(patternLength);

    for (std::size_t i = 0; i < std::strlen(asciiPattern); i++)
    {
        std::uint8_t maskValue = (asciiPattern[i] != '?') ? 0xFF : 0x00;

        for (size_t byte = 0; byte < TestType::char_size; byte++)
        {
            mask[i*TestType::char_size + byte] = maskValue;
        }
    }

    boyer b{ pattern.get(), patternLength, mask.get() };

    SECTION("no match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdabcdabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("successful matches")
    {
        // test that anything between the M and E will match
        const char* asciiTestStr = GENERATE(
            "MAKE", "MOVE", "MAAE", "MZZE",
            "M--E", "M??E", "M00E", "M99E",
            "M\x01\x01" "E",    // note: '\x01E' is interpreted as an escape code, so it needs to be broken up
            "M\xFF\xFF" "E"     // note: '\xFFE' is interpreted as an escape code, so it needs to be broken up
        );
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii(asciiTestStr, teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        if (found != &teststr[0 * TestType::char_size])
        {
            FAIL("Test string = [" << asciiTestStr << "]");
        }
    }

    SECTION("successful match at start of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("MAKEabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0 * TestType::char_size]);
    }

    SECTION("successful match in middle of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKEabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match at end of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKE", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match with multiple occurrences finds first")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKEabcdMAKEabcdMAKEabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        // finds the first match
        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match case-insensitive")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdmAkEeabcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            TRUE,                           // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        // finds the first match
        CHECK(found == &teststr[4 * TestType::char_size]);
    }
}

TEMPLATE_TEST_CASE("boyer::findforw (mask) - text - all wildcards", "", ascii_info, unicode_info, ebcdic_info)
{
    std::size_t patternLength;
    const char* asciiPattern = "????";
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii(asciiPattern, patternLength);
    auto mask = std::make_unique<std::uint8_t[]>(patternLength);

    for (std::size_t i = 0; i < std::strlen(asciiPattern); i++)
    {
        std::uint8_t maskValue = (asciiPattern[i] != '?') ? 0xFF : 0x00;

        for (size_t byte = 0; byte < TestType::char_size; byte++)
        {
            mask[i * TestType::char_size + byte] = maskValue;
        }
    }

    boyer b{ pattern.get(), patternLength, mask.get() };

    SECTION("no match - string too short")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abc", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("successful match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd", teststrLen);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0]);
    }
}

TEMPLATE_TEST_CASE("boyer::findforw (mask) - text - whole word", "", ascii_info, unicode_info, ebcdic_info)
{
    BOOL ignoreCase = GENERATE(FALSE, TRUE);
    INFO("ignoreCase = " << (ignoreCase == TRUE ? "true" : "false"));

    std::size_t patternLength;
    const char* asciiPattern = "M??E";
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii(asciiPattern, patternLength);
    auto mask = std::make_unique<std::uint8_t[]>(patternLength);

    for (std::size_t i = 0; i < std::strlen(asciiPattern); i++)
    {
        std::uint8_t maskValue = (asciiPattern[i] != '?') ? 0xFF : 0x00;

        for (size_t byte = 0; byte < TestType::char_size; byte++)
        {
            mask[i * TestType::char_size + byte] = maskValue;
        }
    }

    boyer b{ pattern.get(), patternLength, mask.get() };


    SECTION("successful match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd MAKE abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[5 * TestType::char_size]);
    }


    SECTION("no match - bad boundaries")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKEabcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("no match - bad left boundary")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKE abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        if constexpr (std::is_same_v<TestType, unicode_info>)
        {
            // TODO(bugs): This test case seems to be hitting a bug where the left boundary isn't
            // being checked correctly for 'Unicode' text searches. I verified this in-app as well,
            // by creating a new file with the test string, and searching for the pattern with
            // 'whole word' and 'match case' enabled.
            WARN("!!! Possible bug in Unicode whole word search - is left boundary being checked correctly?");
            CHECK(found == &teststr[4 * TestType::char_size]);
        }
        else
        {
            CHECK(found == nullptr);
        }
    }

    SECTION("no match - bad right boundary")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd MAKEabcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match - at start with alpha_before = FALSE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("MAKE abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0 * TestType::char_size]);
    }

    SECTION("no match - at start with alpha_before = TRUE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("MAKE abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            TRUE, FALSE,                    // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match - at end with alpha_after = FALSE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd MAKE", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[5 * TestType::char_size]);
    }

    SECTION("no match - at end with alpha_after = TRUE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd MAKE", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findforw(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, TRUE,                    // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }
}


TEMPLATE_TEST_CASE("boyer::findback (mask) - text - basic matching", "", ascii_info, unicode_info, ebcdic_info)
{
    std::size_t patternLength;
    const char* asciiPattern = "M??E";
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii(asciiPattern, patternLength);
    auto mask = std::make_unique<std::uint8_t[]>(patternLength);

    for (std::size_t i = 0; i < std::strlen(asciiPattern); i++)
    {
        std::uint8_t maskValue = (asciiPattern[i] != '?') ? 0xFF : 0x00;

        for (size_t byte = 0; byte < TestType::char_size; byte++)
        {
            mask[i * TestType::char_size + byte] = maskValue;
        }
    }

    boyer b{ pattern.get(), patternLength, mask.get() };

    SECTION("no match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdabcdabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("successful matches")
    {
        // test that anything between the M and E will match
        const char* asciiTestStr = GENERATE(
            "MAKE", "MOVE", "MAAE", "MZZE",
            "M--E", "M??E", "M00E", "M99E",
            "M\x01\x01" "E",    // note: '\x01E' is interpreted as an escape code, so it needs to be broken up
            "M\xFF\xFF" "E"     // note: '\xFFE' is interpreted as an escape code, so it needs to be broken up
        );
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii(asciiTestStr, teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        if (found != &teststr[0 * TestType::char_size])
        {
            FAIL("Test string = [" << asciiTestStr << "]");
        }
    }

    SECTION("successful match at start of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("MAKEabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0 * TestType::char_size]);
    }

    SECTION("successful match in middle of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKEabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match at end of text")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKE", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[4 * TestType::char_size]);
    }

    SECTION("successful match with multiple occurrences finds last")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKEabcdMAKEabcdMAKEabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        // finds the first match
        CHECK(found == &teststr[20 * TestType::char_size]);
    }

    SECTION("successful match case-insensitive")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdmAkEeabcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            TRUE,                           // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        // finds the first match
        CHECK(found == &teststr[4 * TestType::char_size]);
    }
}

TEMPLATE_TEST_CASE("boyer::findback (mask) - text - all wildcards", "", ascii_info, unicode_info, ebcdic_info)
{
    std::size_t patternLength;
    const char* asciiPattern = "????";
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii(asciiPattern, patternLength);
    auto mask = std::make_unique<std::uint8_t[]>(patternLength);

    for (std::size_t i = 0; i < std::strlen(asciiPattern); i++)
    {
        std::uint8_t maskValue = (asciiPattern[i] != '?') ? 0xFF : 0x00;

        for (size_t byte = 0; byte < TestType::char_size; byte++)
        {
            mask[i * TestType::char_size + byte] = maskValue;
        }
    }

    boyer b{ pattern.get(), patternLength, mask.get() };

    SECTION("no match - string too short")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abc", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("successful match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd", teststrLen);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            FALSE,                          // ignore case
            TestType::value,                // char set
            FALSE,                          // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0]);
    }
}

TEMPLATE_TEST_CASE("boyer::findback (mask) - text - whole word", "", ascii_info, unicode_info, ebcdic_info)
{
    BOOL ignoreCase = GENERATE(FALSE, TRUE);
    INFO("ignoreCase = " << (ignoreCase == TRUE ? "true" : "false"));

    std::size_t patternLength;
    const char* asciiPattern = "M??E";
    std::unique_ptr<std::uint8_t[]> pattern = TestType::map_ascii(asciiPattern, patternLength);
    auto mask = std::make_unique<std::uint8_t[]>(patternLength);

    for (std::size_t i = 0; i < std::strlen(asciiPattern); i++)
    {
        std::uint8_t maskValue = (asciiPattern[i] != '?') ? 0xFF : 0x00;

        for (size_t byte = 0; byte < TestType::char_size; byte++)
        {
            mask[i * TestType::char_size + byte] = maskValue;
        }
    }

    boyer b{ pattern.get(), patternLength, mask.get() };


    SECTION("successful match")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd MAKE abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[5 * TestType::char_size]);
    }


    SECTION("no match - bad boundaries")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKEabcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }

    SECTION("no match - bad left boundary")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcdMAKE abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        if constexpr (std::is_same_v<TestType, unicode_info>)
        {
            // TODO(bugs): This test case seems to be hitting a bug where the left boundary isn't
            // being checked correctly for 'Unicode' text searches. I verified this in-app as well,
            // by creating a new file with the test string, and searching for the pattern with
            // 'whole word' and 'match case' enabled.
            WARN("!!! Possible bug in Unicode whole word search - is left boundary being checked correctly?");
            CHECK(found == &teststr[4 * TestType::char_size]);
        }
        else
        {
            CHECK(found == nullptr);
        }
    }

    SECTION("no match - bad right boundary")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd MAKEabcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match - at start with alpha_before = FALSE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("MAKE abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[0 * TestType::char_size]);
    }

    SECTION("no match - at start with alpha_before = TRUE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("MAKE abcd", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            TRUE, FALSE,                    // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }


    SECTION("successful match - at end with alpha_after = FALSE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd MAKE", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, FALSE,                   // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == &teststr[5 * TestType::char_size]);
    }

    SECTION("no match - at end with alpha_after = TRUE")
    {
        std::size_t teststrLen;
        std::unique_ptr<std::uint8_t[]> teststr = TestType::map_ascii("abcd MAKE", teststrLen, ignoreCase);

        const std::uint8_t* found = b.findback(
            teststr.get(), teststrLen,      // string and length
            ignoreCase,                     // ignore case
            TestType::value,                // char set
            TRUE,                           // match whole word
            FALSE, TRUE,                    // alpha before/after
            1,                              // alignment
            0,                              // offset
            0, 0                            // base address + address
        );

        CHECK(found == nullptr);
    }
}
