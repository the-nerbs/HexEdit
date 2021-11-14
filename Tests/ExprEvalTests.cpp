#include "Stdafx.h"
#include "Expr.h"

#include <catch.hpp>

using value_t = expr_eval::value_t;

static constexpr DATE InvalidDate = -1e30;

static bool values_equal(const value_t& expected, const value_t& actual)
{
    if (expected.typ != actual.typ)
    {
        return false;
    }

    switch (expected.typ)
    {
    case expr_eval::TYPE_NONE:
        // no data to compare
        return true;

    case expr_eval::TYPE_BOOLEAN:
        return expected.boolean == actual.boolean;

    case expr_eval::TYPE_INT:
        return expected.int64 == actual.int64;

    case expr_eval::TYPE_DATE:
    case expr_eval::TYPE_REAL:
    {
        if (std::isinf(expected.real64))
        {
            return std::isinf(actual.real64)
                && (expected.real64 < 0) == (actual.real64 < 0);
        }
        else if (std::isnan(expected.real64))
        {
            return std::isnan(actual.real64);
        }
        // else: normal, subnormal, or zero

        return std::abs(expected.real64 - actual.real64) < 1e-10;
    }

    case expr_eval::TYPE_STRING:
        return (expected.pstr == nullptr && actual.pstr == nullptr)
            || (expected.pstr && actual.pstr && (*expected.pstr == *actual.pstr));

    case expr_eval::TYPE_BLOB:
    case expr_eval::TYPE_STRUCT:
    case expr_eval::TYPE_ARRAY:
        // not supported as these require querying the file data.
        return false;

    default:
        // unexpected type code.
        return false;
    }
}

static bool string_equal(const char* x, const char* y)
{
    return x == nullptr && y == nullptr
        || (x && y && strcmp(x, y) == 0);
}

static value_t empty_value_with_type(expr_eval::type_t type)
{
    value_t v;
    v.typ = type;
    v.int64 = 0;
    return v;
}

class test_expr : public expr_eval
{
public:
    test_expr(int max_radix = 10, bool const_sep_allowed = false)
        : expr_eval{ max_radix, const_sep_allowed }
    { }

    value_t find_symbol(
        const char* sym,
        value_t parent, std::size_t index,
        int* pac,
        std::int64_t& sym_size, std::int64_t& sym_address,
        CString& sym_str) override
    {
        struct sym_info_t
        {
            const char* name;
            std::size_t size;
            value_t value;
        };

        struct container_t
        {
            const char* name;
            std::size_t address;
            std::size_t size;
            expr_eval::type_t type;
            std::vector<const sym_info_t*> children;
        };

        static const sym_info_t types[] =
        {
            { "int", 4, value_t{}  },
            { "char", 1, value_t{} },
        };

        static const sym_info_t symbols[] =
        {
            // note: when adding anything here, make sure
            // to keep the container child indexes in sync!
            { "intField", 4, value_t{ 12345 } },
            { "realField", 8, value_t{ 123.456 } },
            { "stringField", 12, value_t{ "test string" } },

            // arrayField
            { "arrayField[0]", 4, value_t{ 5 } },
            { "arrayField[1]", 4, value_t{ 6 } },

            // structField
            { "value1", 4, value_t{ 7 } },
            { "value2", 4, value_t{ 8 } },

            { "trueField", 1, value_t{ true } },
            { "falseField", 1, value_t{ false } },
        };

        static const container_t containers[] =
        {
            { "arrayField", 24, 2 * 4, expr_eval::TYPE_ARRAY, { &symbols[3], &symbols[4] } },
            { "structField", 32, 2*4, expr_eval::TYPE_STRUCT, { &symbols[5], &symbols[6] } },
            { "blobField", 42, 4, expr_eval::TYPE_BLOB, { } }
        };

        static const std::uint8_t blobData[4] =
        {
            123, 21, 89, 72
        };

        if (parent.typ == expr_eval::TYPE_NONE)
        {
            // check types
            for (auto& type : types)
            {
                if (string_equal(type.name, sym))
                {
                    // type matched
                    sym_size = type.size;
                    sym_address = 0;

                    // return a non-empty value to signal that the lookup succeeded.
                    return value_t{ 0 };
                }
            }

            // check fields
            std::size_t address = 0;

            for (auto& info : symbols)
            {
                if (string_equal(info.name, sym))
                {
                    // found a field
                    sym_size = info.size;
                    sym_address = address;
                    return info.value;
                }

                address += info.size;
            }

            // check containers
            for (auto& info : containers)
            {
                if (string_equal(info.name, sym))
                {
                    // found a container
                    sym_size = info.size;
                    sym_address = info.address;
                    return empty_value_with_type(info.type);
                }
            }
        }
        else
        {
            // we're looking up a container child.
            for (auto& container : containers)
            {
                if (container.address == sym_address)
                {
                    const sym_info_t* target = nullptr;

                    if (parent.typ == expr_eval::TYPE_ARRAY)
                    {
                        if (index < container.children.size())
                        {
                            target = container.children[index];
                        }
                    }
                    else if (parent.typ == expr_eval::TYPE_STRUCT)
                    {
                        for (auto child : container.children)
                        {
                            if (string_equal(sym, child->name))
                            {
                                target = child;
                                break;
                            }
                        }
                    }
                    else if (parent.typ == expr_eval::TYPE_BLOB)
                    {
                        if (index < std::size(blobData))
                        {
                            sym_size = 1;
                            sym_address += index;
                            return value_t{ blobData[index] };
                        }
                    }

                    if (target)
                    {
                        sym_size = target->size;
                        sym_address = 0;

                        for (auto& sym : symbols)
                        {
                            if (&sym == target)
                            {
                                break;
                            }

                            sym_address += sym.size;
                        }

                        return target->value;
                    }
                }
            }
        }

        // just return an empty value to signal that the symbol was not found.
        return value_t{};
    }

    bool get_variable(const CString& name, value_t& value) const
    {
        auto itr = var_.find(name);

        if (itr != var_.end())
        {
            value = itr->second;
            return true;
        }

        return false;
    }

    void set_variable(const CString& name, value_t value)
    {
        var_[name] = value;
    }
};


TEST_CASE("expr_eval::value_t - constructors")
{
    SECTION("empty value")
    {
        value_t val{};
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_NONE);
    }

    SECTION("boolean value")
    {
        bool boolValue = GENERATE(false, true);
        value_t val{ boolValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_BOOLEAN);
        CHECK(val.boolean == boolValue);
    }

    SECTION("character value")
    {
        char charValue = GENERATE('\0', 'A', 'b', '5', '\n');
        value_t val{ charValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_INT);
        CHECK(val.int64 == charValue);
    }

    SECTION("integer value")
    {
        int intValue = GENERATE(0, INT_MIN, INT_MAX, -100, 100);
        value_t val{ intValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_INT);
        CHECK(val.int64 == intValue);
    }

    SECTION("int64 value")
    {
        int intValue = GENERATE(0, LONGLONG_MIN, LONGLONG_MAX, -100, 100);
        value_t val{ intValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_INT);
        CHECK(val.int64 == intValue);
    }

    SECTION("float value")
    {
        float floatValue = GENERATE(
            0.0f,
            std::numeric_limits<float>::min(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::epsilon(),
            -100.0f,
            100.0f
        );
        value_t val{ floatValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_REAL);
        CHECK(val.real64 == floatValue);
    }

    SECTION("double value")
    {
        double floatValue = GENERATE(
            0.0f,
            std::numeric_limits<double>::min(),
            std::numeric_limits<double>::max(),
            std::numeric_limits<double>::epsilon(),
            -100.0f,
            100.0f
        );
        value_t val{ floatValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_REAL);
        CHECK(val.real64 == floatValue);
    }

    SECTION("string value")
    {
        const char* stringValue = "stringValue";
        CStringW stringValueAsCStringW{ stringValue };

        value_t val{ stringValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_STRING);
        REQUIRE(val.pstr != nullptr);
        CHECK(val.pstr->Compare(stringValueAsCStringW) == 0);
    }

    SECTION("string value with length")
    {
        const char* stringValue = "stringValue";
        int length = 6;
        CStringW stringValueAsCStringW{ stringValue, length };

        value_t val{ stringValue, length };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_STRING);
        REQUIRE(val.pstr != nullptr);
        CHECK(val.pstr->Compare(stringValueAsCStringW) == 0);
    }

    SECTION("wide string value")
    {
        const wchar_t* stringValue = L"stringValue";

        value_t val{ stringValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_STRING);
        REQUIRE(val.pstr != nullptr);
        CHECK(val.pstr->Compare(stringValue) == 0);
    }

    SECTION("date/time value")
    {
        COleDateTime dateValue = GENERATE(
            COleDateTime{},
            COleDateTime{ 2021, 11, 6, 13, 33, 20 }
        );

        value_t val{ dateValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_DATE);
        CHECK(val.date == dateValue.m_dt);
    }

    SECTION("date/time value - error")
    {
        COleDateTime dateValue{ 2021, 11, 6, 13, 33, 20 };
        dateValue.m_status = COleDateTime::error;

        value_t val{ dateValue };
        CHECK(val.error == false);
        CHECK(val.typ == expr_eval::TYPE_DATE);
        CHECK(val.date == InvalidDate);
    }


    SECTION("copy constructor - empty")
    {
        value_t val{ };
        value_t val2{ val };

        CHECK(val2.typ == expr_eval::TYPE_NONE);
        CHECK(val2.error == false);
    }

    SECTION("copy constructor - integer")
    {
        value_t val{ LONGLONG_MIN };
        value_t val2{ val };

        CHECK(val2.typ == expr_eval::TYPE_INT);
        CHECK(val2.error == false);
        CHECK(val2.int64 == LONGLONG_MIN);
    }

    SECTION("copy constructor - double")
    {
        value_t val{ 123.456e7 };
        value_t val2{ val };

        CHECK(val2.typ == expr_eval::TYPE_REAL);
        CHECK(val2.error == false);
        CHECK(val2.real64 == 123.456e7);
    }

    SECTION("copy constructor - string")
    {
        value_t val{ L"test string value" };
        value_t val2{ val };

        CHECK(val2.typ == expr_eval::TYPE_STRING);
        CHECK(val2.error == false);

        // copy should have a new, but equal string object
        CHECK(val2.pstr != val.pstr);
        REQUIRE(val.pstr != nullptr);
        REQUIRE(val2.pstr != nullptr);
        CHECK(*val2.pstr == *val.pstr);
    }

    SECTION("copy constructor - date")
    {
        COleDateTime dateValue{ 2021, 11, 6, 13, 33, 20 };
        value_t val{ dateValue };
        value_t val2{ val };

        CHECK(val2.typ == expr_eval::TYPE_DATE);
        CHECK(val2.error == false);
        CHECK(val2.date == dateValue.m_dt);
    }
}

TEST_CASE("expr_eval::value_t - GetDataString - TYPE_BOOLEAN")
{
    struct row
    {
        const char* format;
        const wchar_t* trueString;
        const wchar_t* falseString;
    };

    row test = GENERATE(
        row{ "f",                L"t",     L"f" },
        row{ "F",                L"T",     L"F" },
        row{ "false",         L"true", L"false" },
        row{ "n",                L"y",     L"n" },
        row{ "N",                L"Y",     L"N" },
        row{ "no",             L"yes",    L"no" },
        row{ "NO",             L"YES",    L"NO" },
        row{ "off",             L"on",   L"off" },
        row{ "OFF",             L"ON",   L"OFF" },
        row{ "0",                L"1",     L"0" },
        row{ "anything else", L"TRUE", L"FALSE" }
    );

    {
        value_t val{ false };
        CStringW actual = val.GetDataString(test.format);
        CHECK(CStringW{ test.falseString } == actual);
    }

    {
        value_t val{ true };
        CStringW actual = val.GetDataString(test.format);
        CHECK(CStringW{ test.trueString } == actual);
    }
}

TEST_CASE("expr_eval::value_t - GetDataString - TYPE_INT")
{
    struct row
    {
        std::int64_t bits;
        const char* format;
        int size;
        bool unsgned;
        const wchar_t* expected;
    };

    row test = GENERATE(
        // sprintf-style formats
        row{ 0, "%d",  1, false, L"0" },
        row{ 0, "%u",  1,  true, L"0" },
        row{ 0, "%d",  2, false, L"0" },
        row{ 0, "%u",  2,  true, L"0" },
        row{ 0, "%d",  4, false, L"0" },
        row{ 0, "%u",  4,  true, L"0" },
        row{ 0, "%d",  8, false, L"0" },
        row{ 0, "%u",  8,  true, L"0" },
        row{ 0, "%d", -1, false, L"0" },
        row{ 0, "%u", -1,  true, L"0" },

        row{ 15, "%x",  1, false, L"f" },
        row{ 15, "%x",  1,  true, L"f" },
        row{ 15, "%x",  2, false, L"f" },
        row{ 15, "%x",  2,  true, L"f" },
        row{ 15, "%x",  4, false, L"f" },
        row{ 15, "%x",  4,  true, L"f" },
        row{ 15, "%x",  8, false, L"f" },
        row{ 15, "%x",  8,  true, L"f" },
        row{ 15, "%x", -1, false, L"f" },
        row{ 15, "%x", -1,  true, L"f" },

        row{ 95, "%X",  1, false, L"5F" },
        row{ 95, "%X",  1,  true, L"5F" },
        row{ 95, "%X",  2, false, L"5F" },
        row{ 95, "%X",  2,  true, L"5F" },
        row{ 95, "%X",  4, false, L"5F" },
        row{ 95, "%X",  4,  true, L"5F" },
        row{ 95, "%X",  8, false, L"5F" },
        row{ 95, "%X",  8,  true, L"5F" },
        row{ 95, "%X", -1, false, L"5F" },
        row{ 95, "%X", -1,  true, L"5F" },

        // non-sprintf formats
        row{ INT64_C(~0), "hex", -1,  true, L"ff ff ff ff ff ff ff ff" },
        row{ INT64_C(~0), "bin",  1,  true, L"11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111" },
        row{ INT64_C(~0), "oct",  2,  true, L"17 7777 7777 7777 7777 7777" },

        // any other formats
        row{        INT64_C(~0), "no format", -1,  false,                         L"-1" },
        row{ 0x7FFFFFFFFFFFFFFF, "no format", -1,  false,  L"9,223,372,036,854,775,807" },
        row{         INT64_C(0), "no format", -1,  true,                           L"0" },
        row{        INT64_C(~0), "no format", -1,  true,  L"18,446,744,073,709,551,615" },
        
        row{        INT64_C(~0), "no format",  1,  false,                         L"-1" },
        row{ 0x7FFFFFFFFFFFFFFF, "no format",  1,  false,  L"9,223,372,036,854,775,807" },
        row{         INT64_C(0), "no format",  1,  true,                           L"0" },
        row{        INT64_C(~0), "no format",  1,  true,  L"18,446,744,073,709,551,615" },

        row{        INT64_C(~0), "no format",  2,  false,                         L"-1" },
        row{ 0x7FFFFFFFFFFFFFFF, "no format",  2,  false,  L"9,223,372,036,854,775,807" },
        row{         INT64_C(0), "no format",  2,  true,                           L"0" },
        row{        INT64_C(~0), "no format",  2,  true,  L"18,446,744,073,709,551,615" },

        row{        INT64_C(~0), "no format",  4,  false,                         L"-1" },
        row{ 0x7FFFFFFFFFFFFFFF, "no format",  4,  false,  L"9,223,372,036,854,775,807" },
        row{         INT64_C(0), "no format",  4,  true,                           L"0" },
        row{        INT64_C(~0), "no format",  4,  true,  L"18,446,744,073,709,551,615" },
        
        row{        INT64_C(~0), "no format",  8,  false,                         L"-1" },
        row{ 0x7FFFFFFFFFFFFFFF, "no format",  8,  false,  L"9,223,372,036,854,775,807" },
        row{         INT64_C(0), "no format",  8,  true,                           L"0" },
        row{        INT64_C(~0), "no format",  8,  true,  L"18,446,744,073,709,551,615" },

        // all bits set for size
        row{          -1, "%d", 1, false,                   L"-1" },
        row{        0xFF, "%u", 1,  true,                  L"255" },
        row{          -1, "%d", 2, false,                   L"-1" },
        row{      0xFFFF, "%u", 2,  true,                L"65535" },
        row{          -1, "%d", 4, false,                   L"-1" },
        row{  0xFFFFFFFF, "%u", 4,  true,           L"4294967295" },
        row{          -1, "%d", 8, false,                   L"-1" },
        row{ INT64_C(~0), "%u", 8,  true, L"18446744073709551615" },

        // all int bits set, size less than #bits set
        row{ INT64_C(~0), "%d", -1,  false,                  L"-1" },
        row{ INT64_C(~0), "%u", -1,  true, L"18446744073709551615" },
        row{ INT64_C(~0), "%d",  1,  false,                  L"-1" },
        row{ INT64_C(~0), "%u",  1,  true, L"18446744073709551615" },
        row{ INT64_C(~0), "%d",  2,  false,                  L"-1" },
        row{ INT64_C(~0), "%u",  2,  true, L"18446744073709551615" },
        row{ INT64_C(~0), "%d",  4,  false,                  L"-1" },
        row{ INT64_C(~0), "%u",  4,  true, L"18446744073709551615" },

        row{ 0x12345, "",  4,  true, L"0x12345" }
    );

    value_t val{ test.bits };

    //TODO: refactor app code so we don't need this global state.
    theApp.dec_group_ = 3;
    theApp.dec_sep_char_ = ',';
    theApp.default_int_format_ = "0x%X";

    CStringW actual = val.GetDataString(test.format, test.size, test.unsgned);
    CHECK(actual == test.expected);
}

TEST_CASE("expr_eval::value_t - GetDataString - TYPE_DATE")
{
    struct row
    {
        COleDateTime time;
        const char* format;
        const wchar_t* expected;
    };

    // note: dates are ultimately formatted through strftime
    // also note for %G/%g/%V - Monday is the start of the week for ISO-8601, not Sunday!
    // local dependent specifiers not tested here: a, A, b/h, B, c, x, X, r, p, z, Z
    row test = GENERATE(
        row{ COleDateTime{ 2021, 11, 7, 16, 23, 10 }, "%%%n%t-%Y-%m-%d-%H-%M-%S", L"%\n\t-2021-11-07-16-23-10"},
        row{ COleDateTime{ 2021, 11, 7, 16, 23, 10 }, "%C%y-%j-%I", L"2021-311-04" },
        row{ COleDateTime{ 2021, 1, 4, 16, 23, 10 }, "%G-%e", L"2021- 4" },
        row{ COleDateTime{ 2021, 1, 1, 16, 23, 10 }, "%G", L"2020" },
        row{ COleDateTime{ 2021, 1, 4, 16, 23, 10 }, "%g", L"21" },
        row{ COleDateTime{ 2021, 1, 1, 16, 23, 10 }, "%g", L"20" },
        row{ COleDateTime{ 2021, 1, 3, 16, 23, 10 }, "%U-%W-%V", L"01-00-53" },
        row{ COleDateTime{ 2021, 11, 7, 16, 23, 10 }, "%w-%u", L"0-7" },
        row{ COleDateTime{ 2021, 11, 7, 16, 23, 10 }, "%D", L"11/07/21" },
        row{ COleDateTime{ 2021, 11, 7, 16, 23, 10 }, "%F", L"2021-11-07" },
        row{ COleDateTime{ 2021, 11, 7, 16, 23, 10 }, "%R", L"16:23" },
        row{ COleDateTime{ 2021, 11, 7, 16, 23, 10 }, "%T", L"16:23:10" },

        row{ COleDateTime{ InvalidDate }, "anything", L"##Invalid date##" },

        row{ COleDateTime{ 2021, 11, 7, 16, 23, 10 }, "", L"2021-11-07" }
    );

    //TODO: refactor app code so we don't need this global state.
    theApp.default_date_format_ = "%F";

    value_t val{ test.time };
    CStringW actual = val.GetDataString(test.format);
    CHECK(actual == test.expected);
}

TEST_CASE("expr_eval::value_t - GetDataString - TYPE_REAL")
{
    struct row
    {
        double value;
        const char* format;
        int size;
        const wchar_t* expected;
    };

    row test = GENERATE(
        row{ 123.456, "%f", 8, L"123.456000" },
        row{ 123.456, "%F", 8, L"123.456000" },

        row{ 123.456, "%e", 8, L"1.234560e+02" },
        row{ 123.456, "%E", 8, L"1.234560E+02" },

        row{ 15.625, "%a", 8, L"0x1.f400000000000p+3" },
        row{ 15.625, "%A", 8, L"0X1.F400000000000P+3" },

        row{ 1.234567, "%g", 8, L"1.23457" },
        row{ 1.234567, "%G", 8, L"1.23457" },

        row{ 1.2345678901234567, "anything", 4, L"1.234568" },
        row{ 1.2345678901234567, "anything", 8, L"1.23456789012346" },

        row{       NAN, "anything", 8, L"NaN" },
        row{  INFINITY, "anything", 8, L"+Inf" },
        row{ -INFINITY, "anything", 8, L"-Inf" },

        row{ 3.0, "", 8, L"3.000" }
    );

    //TODO: refactor app code so we don't need this global state.
    theApp.default_real_format_ = "%0.3f";

    value_t val{ test.value };
    CStringW actual = val.GetDataString(test.format, test.size);
    CHECK(actual == test.expected);
}

TEST_CASE("expr_eval::value_t - GetDataString - TYPE_STRING")
{
    struct row
    {
        const char* value;
        const char* format;
        const wchar_t* expected;
    };

    row test = GENERATE(
        row{ "testtest", "no format", L"testtest" },
        row{ "testtest", "%s", L"testtest" },
        row{ "testtest", "", L"\"testtest\"" }
    );

    //TODO: refactor app code so we don't need this global state.
    theApp.default_string_format_ = "\"%s\"";

    value_t val{ test.value };
    CStringW actual = val.GetDataString(test.format);
    CHECK(actual == test.expected);
}


TEST_CASE("expr_eval::evaluate - basic tests")
{
    struct row
    {
        const char* expression;
        value_t expected;
    };

    row test = GENERATE(
        // literals
        row{ "5", value_t{ 5 } },                   // int
        row{ "5.5", value_t{ 5.5 } },               // real
        row{ "2e3", value_t{ 2e3 } },               // real (scientific notation)
        row{ "2.1e+3", value_t{ 2.1e+3 } },         //  . . .
        row{ "2.1e-3", value_t{ 2.1e-3 } },         //  . . .
        row{ "PI", value_t{ 3.141592653589793 } },  // named real PI
        row{ "\"string\"", value_t{ "string" } },   // string
        row{ "'X'", value_t{ 0x58 } },              // character
        row{ "TRUE", value_t{ true } },             // boolean true
        row{ "FALSE", value_t{ false } },           // boolean false

        // string/character escape sequences
        row{ "\"\\a\\b\\t\\n\\v\\f\\r\\\"\"", value_t{ "\a\b\t\n\v\f\r\"" } },
        row{ "\"\\x9\\x54\\x45\\x53\\x54\\x3F\\x3f\"", value_t{ "\tTEST??" } },
        row{ "\"\\x\"", value_t{ "" } },
        row{ "'\\a'", value_t{ '\a' } },
        row{ "'\\b'", value_t{ '\b' } },
        row{ "'\\t'", value_t{ '\t' } },
        row{ "'\\n'", value_t{ '\n' } },
        row{ "'\\v'", value_t{ '\v' } },
        row{ "'\\f'", value_t{ '\f' } },
        row{ "'\\r'", value_t{ '\r' } },

        // misc operators
        row{ "1+1", value_t{ 2 } },
        row{ "1-1", value_t{ 0 } },
        row{ "2*2", value_t{ 4 } },
        row{ "4/2", value_t{ 2 } },
        row{ "5%2", value_t{ 1 } },
        row{ "15 & 3", value_t{ 3 } },
        row{ "8 | 3", value_t{ 11 } },
        row{ "15 ^ 3", value_t{ 12 } },
        row{ "1 < 2", value_t{ true } },
        row{ "1 > 2", value_t{ false } },
        row{ "true ? 1 : 0", value_t{ 1 } },
        row{ "false ? 1 : 0", value_t{ 0 } },
        row{ "true ? 1 : ()", value_t{ 1 } },               // ternary ignores if other
        row{ "false ? () : 0", value_t{ 0 } },              //  branch's expression is valid
        row{ "1 == 0", value_t{ false } },
        row{ "1 != 0", value_t{ true } },
        row{ "1 <= 2", value_t{ true } },
        row{ "1 >= 2", value_t{ false } },
        row{ "1 << 2", value_t{ 4 } },
        row{ "8 >> 2", value_t{ 2 } },
        row{ "TRUE && TRUE", value_t{ true } },
        row{ "TRUE && FALSE", value_t{ false } },
        row{ "FALSE || FALSE", value_t{ false } },
        row{ "TRUE || FALSE", value_t{ true } },

        // sizeof()
        row{ "sizeof(INT)", value_t{ 4 } },                 // sizeof(<TOK_INT>)
        row{ "sizeof(int)", value_t{ 4 } },                 // sizeof(<type symbol>)
        row{ "SIZEOF(int)", value_t{ 4 } },                 // uppercased 'sizeof'
        row{ "sizeof(realField)", value_t{ 8 } },           // sizeof(<field symbol>)
        row{ "sizeof(arrayField[0])", value_t{ 4 } },       // sizeof(<array field element>)
        row{ "sizeof(structField.value1)", value_t{ 4 } },  // sizeof(<struct field element>)
        row{ "sizeof(INT_VAR)", value_t{ 8 } },             // sizeof(<int variable>)
        row{ "sizeof(BOOL_VAR)", value_t{ 1 } },            // sizeof(<boolean variable>)
        row{ "sizeof(REAL_VAR)", value_t{ 8 } },            // sizeof(<real variable>)
        row{ "sizeof(STRING_VAR)", value_t{ 20 } },         // sizeof(<string variable>)
        row{ "sizeof(DATE_VAR)", value_t{ 8 } },            // sizeof(<date variable>)

        // defined
        row{ "defined intField", value_t{ true } },     // no parentheses
        row{ "DEFINED(intField)", value_t{ true } },    // uppercased 'defined'
        row{ "defined(intField)", value_t{ true } },    // defined(<field symbol>)
        row{ "defined(INT_VAR)", value_t{ true } },     // defined(<variable symbol>)
        row{ "defined(NOT_DEF)", value_t{ false } },    // name not defined

        // string()
        // note that this function:
        //  1) is not documented in the help at all?
        //  2) is mostly used to get enum strings, which is done from find_symbol, so we can't test that part here.
        //  3) has hard-coded behavior for variables
        row{ "string(INT_VAR)", value_t{ "123" } },             // string(<int variable>)
        row{ "string(BOOL_VAR)", value_t{ "TRUE" } },           // string(<boolean variable>)
        row{ "string(REAL_VAR)", value_t{ "123.456" } },        // string(<real variable>)
        row{ "string(STRING_VAR)", value_t{ "string_var" } },   // string(<string variable>)
//        row{ "string(DATE_VAR)", value_t{ "" } },               // string(<date variable>) - note: this is TODO in app code
        row{ "string(ARR_VAR[0])", value_t{ "456" } },          // string(<array variable element>)
        row{ "string(arrayField[0])", value_t{ "" }},           // string(<array field element>)
        row{ "string(structField.value1)", value_t{ "" }},      // string(<struct field element>)

        // TOK_GETINT, TOK_GETSTR, TOK_GETBOOL - show UIs

        // addressof()
        row{ "ADDRESSOF(intField)", value_t{ 0 } },             // uppercased 'addressof'
        row{ "addressof(realField)", value_t{ 4 } },            // addressof(<field symbol>)
        row{ "addressof(arrayField[0])", value_t{ 24 } },       // addressof(<array field element>)
        row{ "addressof(structField.value1)", value_t{ 32 } },  // addressof(<struct field element>)

        // abs()
        row{ "ABS(-123456)", value_t{ 123456 } },       // uppercase, integer
        row{ "abs(-123.456)", value_t{ 123.456 } },     // lowercase, real

        // min()
        row{ "min(10)", value_t{ 10 } },                    // one item
        row{ "min(10, 5)", value_t{ 5 } },                  // two items
        row{ "MIN(1, 2, 0)", value_t{ 0 } },                // uppercase, integers
        row{ "min(1.3, 0.3, 2.5)", value_t{ 0.3 } },        // lowercase, reals
        row{ "min(10.0, -3.14, -4, 5)", value_t{ -4.0 } },  // mixed integers and reals => real

        // max()
        row{ "max(10)", value_t{ 10 } },                    // one item
        row{ "max(10, 5)", value_t{ 10 } },                 // two items
        row{ "MAX(1, 2, 0)", value_t{ 2 } },                // uppercase, integers
        row{ "max(1.3, 0.3, 2.5)", value_t{ 2.5 } },        // lowercase, reals
        row{ "max(10.0, -3.14, -4, 5)", value_t{ 10.0 } },  // mixed integers and reals => real

        // fact()
        row{ "FACT(0)", value_t{ 1 } },                     // uppercase
        row{ "fact(0)", value_t{ 1 } },                     // zero
        row{ "fact(1)", value_t{ 1 } },                     // a few valid values
        row{ "fact(2)", value_t{ 2 } },                     // . . .
        row{ "fact(5)", value_t{ 120 } },                   // . . .
        row{ "fact(20)", value_t{ 2432902008176640000 } },  // max value
        row{ "fact(21)", value_t{ -4249290049419214848 } }, // overflow
        row{ "fact(99)", value_t{ 0 } },                    // overflow, max allowed argument. there's enough 2s in
                                                            //      the factors that the lowest set bit is past 64.
        row{ "fact(-1)", value_t{ 1 } },                    // not valid, but ignored

        // flip(value, #bytes)
        row{ "FLIP(0, 0)", value_t{ 0 } },                                      // uppercase
        row{ "flip(0, 0)", value_t{ 0 } },                                      // zero value, zero bytes
        row{ "flip(0xFF, 1)", value_t{ 0xFF } },                                // non-zero, 1 byte
        row{ "flip(0xFF, 2)", value_t{ 0xFF00 } },                              // non-zero, 2 bytes
        row{ "flip(0xFF, 3)", value_t{ 0xFF0000 } },                            // non-zero, 3 bytes
        row{ "flip(0xFF, 4)", value_t{ INT64_C(0xFF000000) } },                 // non-zero, 4 bytes
        row{ "flip(0xFF, 5)", value_t{ 0xFF00000000 } },                        // non-zero, 5 bytes
        row{ "flip(0xFF, 6)", value_t{ 0xFF0000000000 } },                      // non-zero, 6 bytes
        row{ "flip(0xFF, 7)", value_t{ 0xFF000000000000 } },                    // non-zero, 7 bytes
        row{ "flip(0xFF, 8)", value_t{ INT64_C(0xFF00000000000000) } },         // non-zero, 8 bytes
        row{ "flip(0xFEDCBA9876543210, 8)", value_t{ 0x1032547698BADCFE } },    // bit order not affected
        row{ "flip(-1, 8)", value_t{ -1 } },                                    // negative supported

        // reverse(value [, #bits])
        row{ "REVERSE(0)", value_t{ 0 } },                                  // uppercase, 1 arg
        row{ "REVERSE(0, 0)", value_t{ 0 } },                               // uppercase, 2 arg
        row{ "reverse(0)", value_t{ 0 } },                                  // zero, 1 arg
        row{ "reverse(0, 0)", value_t{ 0 } },                               // zero, 2 arg
        row{ "reverse(1, 0)", value_t{ 0 } },                               // value=1, zero bits
        row{ "reverse(1, 1)", value_t{ 1 } },                               // value=1, 1 bit
        row{ "reverse(1, 2)", value_t{ 2 } },                               // value=1, 2 bits
        row{ "reverse(1, 3)", value_t{ 4 } },                               // value=1, 3 bits
        row{ "reverse(1, 4)", value_t{ 8 } },                               // value=1, 4 bits
        row{ "reverse(1, 5)", value_t{ 16 } },                              // value=1, 5 bits
        row{ "reverse(1, 6)", value_t{ 32 } },                              // value=1, 6 bits
        row{ "reverse(1, 7)", value_t{ 64 } },                              // value=1, 7 bits
        row{ "reverse(1, 8)", value_t{ 128 } },                             // value=1, 8 bits
        row{ "reverse(1, 9)", value_t{ 256 } },                             // value=1, 9 bits
        row{ "reverse(1, 10)", value_t{ 512 } },                            // value=1, 9 bits
        row{ "reverse(1, 64)", value_t{ INT64_C(0x8000000000000000) } },    // value=1, max bits
        row{ "reverse(1, 65)", value_t{ INT64_C(0x8000000000000000) } },    // value=1, over max bits (clamped to max)
        row{ "reverse(1, -1)", value_t{ 0 } },                              // value=1, negative bits (clamped to 0)
        row{ "reverse(1)", value_t{ INT64_C(0x8000000000000000) } },        // value=1, default bits (=max)
        row{ "reverse(0xFEDCBA9876543210)", value_t{ INT64_C(0x084C2A6E195D3B7F) } },
                                                                            // non-0/1 value, default bits
        // rol(value [, #bits [, size]])
        row{ "ROL(0)", value_t{ 0 } },                                      // uppercase, 1 arg
        row{ "ROL(0, 0)", value_t{ 0 } },                                   // uppercase, 2 args
        row{ "ROL(0, 0, 0)", value_t{ 0 } },                                // uppercase, 3 args
        row{ "rol(0)", value_t{ 0 } },                                      // zero, 1 arg
        row{ "rol(0, 0)", value_t{ 0 } },                                   // zero, 2 arg
        row{ "rol(0, 0, 0)", value_t{ 0 } },                                // zero, 3 args
        row{ "rol(1, 0)", value_t{ 1 } },                                   // value=1, zero bits
        row{ "rol(1, 1)", value_t{ 2 } },                                   // value=1, 1 bit
        row{ "rol(1, 2)", value_t{ 4 } },                                   // value=1, 2 bits
        row{ "rol(1, 63)", value_t{ INT64_C(0x8000000000000000) } },        // value=1, max bits
        row{ "rol(1, 64)", value_t{ 1 } },                                  // value=1, max bits (clamped)
        row{ "rol(1, 65)", value_t{ 1 } },                                  // value=1, over max bits (clamped)
        row{ "rol(1)", value_t{ 2 } },                                      // value=1, default bits (=1)
        row{ "rol(-1)", value_t{ -1 } },                                    // negative value, default bits (=1)
        row{ "rol(-1, 63)", value_t{ -1 } },                                // negative value, max bits
        row{ "rol(0x7FFFFFFFFFFFFFFF, -1)", value_t{ 0 } },                 // negative bits, result filled as sign bit
        row{ "rol(0x8000000000000000, -1)", value_t{ -1 } },                // negative bits, result filled as sign bit
        row{ "rol(0xFEDCBA9876543210)", value_t{ -1 } },                    // non-0/1 value, default bits
        row{ "rol(0x7EDCBA9876543210)", value_t{ INT64_C(0xFDB97530ECA86420) } },
                                                                            // non-0/1 value, default bits
        row{ "rol(1, 1, 1)", value_t{ 1 } },                                // value=1, 1 bit, size=1 bit
        row{ "rol(1, 3, 4)", value_t{ 8 } },                                // value=1, 3 bits, size=4 bit
        row{ "rol(-1, 3, 4)", value_t{ 15 } },                              // value=-1, 3 bits, size=4 bit
        row{ "rol(1, 3, 64)", value_t{ 8 } },                               // value=-1, 3 bits, size=max
        row{ "rol(1, 3, 65)", value_t{ 8 } },                               // value=-1, 3 bits, size=over max (clamped)

        //  ror(value [, #bits [, size]])
        row{ "ROR(0)", value_t{ 0 } },                                      // uppercase, 1 arg
        row{ "ROR(0, 0)", value_t{ 0 } },                                   // uppercase, 2 args
        row{ "ROR(0, 0, 0)", value_t{ 0 } },                                // uppercase, 3 args
        row{ "ror(0)", value_t{ 0 } },                                      // zero, 1 arg
        row{ "ror(0, 0)", value_t{ 0 } },                                   // zero, 2 arg
        row{ "ror(0, 0, 0)", value_t{ 0 } },                                // zero, 3 args
        row{ "ror(1, 0)", value_t{ 1 } },                                   // value=1, zero bits
        row{ "ror(1, 1)", value_t{ INT64_C(0x8000000000000000) } },         // value=1, 1 bit
        row{ "ror(1, 2)", value_t{ INT64_C(0x4000000000000000) } },         // value=1, 2 bits
        row{ "ror(1, 63)", value_t{ 2 } },                                  // value=1, max bits
        row{ "ror(1, 64)", value_t{ 1 } },                                  // value=1, max bits (clamped)
        row{ "ror(1, 65)", value_t{ 1 } },                                  // value=1, over max bits (clamped)
        row{ "ror(1)", value_t{ INT64_C(0x8000000000000000) } },            // value=1, default bits (=1)
        row{ "ror(-1)", value_t{ -1 } },                                    // negative value, default bits (=1)
        row{ "ror(-1, 63)", value_t{ -1 } },                                // negative value, max bits
        row{ "ror(0x7FFFFFFFFFFFFFFF, -1)", value_t{ 0 } },                 // negative bits, result filled as sign bit
        row{ "ror(0x8000000000000000, -1)", value_t{ -1 } },                // negative bits, result filled as sign bit
        row{ "ror(0xFEDCBA9876543210)", value_t{ INT64_C(0xFF6E5D4C3B2A1908) } },
                                                                            // non-0/1 value, default bits
        row{ "ror(0x7EDCBA9876543210)", value_t{ INT64_C(0x3F6E5D4C3B2A1908) } },
                                                                            // non-0/1 value, default bits
        row{ "ror(1, 3, 4)", value_t{ 2 } },                                // value=1, 3 bits, size=4 bit
        row{ "ror(-1, 3, 4)", value_t{ 15 } },                              // value=-1, 3 bits, size=4 bit
        row{ "ror(1, 3, 64)", value_t{ INT64_C(0x2000000000000000) } },     // value=-1, 3 bits, size=max
        row{ "ror(1, 3, 65)", value_t{ INT64_C(0x2000000000000000) } },     // value=-1, 3 bits, size=over max (clamped)

        //  asr(value [, #bits [, size]])
        row{ "ASR(0)", value_t{ 0 } },                                      // uppercase, 1 arg
        row{ "ASR(0, 0)", value_t{ 0 } },                                   // uppercase, 2 args
        row{ "ASR(0, 0, 0)", value_t{ 0 } },                                // uppercase, 3 args
        row{ "asr(0)", value_t{ 0 } },                                      // zero, 1 arg
        row{ "asr(0, 0)", value_t{ 0 } },                                   // zero, 2 arg
        row{ "asr(0, 0, 0)", value_t{ 0 } },                                // zero, 3 args
        row{ "asr(0x80, 0)", value_t{ 0x80 } },                             // value=1, zero bits
        row{ "asr(0x80, 1)", value_t{ 0x40 } },                             // value=1, 1 bit
        row{ "asr(0x80, 2)", value_t{ 0x20 } },                             // value=1, 2 bits
        row{ "asr(0x8000000000000000, 63)", value_t{ -1 } },                // value=1, max bits
        row{ "asr(0x8000000000000000, 64)", value_t{ -1 } },                // value=1, max bits (clamped)
        row{ "asr(0x8000000000000000, 65)", value_t{ INT64_C(0x8000000000000000) } },
                                                                            // value=1, over max bits (clamped to 0)
        row{ "asr(1)", value_t{ INT64_C(0) } },                             // value=1, default bits (=1)
        row{ "asr(-1)", value_t{ -1 } },                                    // negative value, default bits (=1)
        row{ "asr(-1, 63)", value_t{ -1 } },                                // negative value, max bits
        row{ "asr(0x7FFFFFFFFFFFFFFF, -1)", value_t{ 0 } },                 // negative bits, result filled as sign bit
        row{ "asr(0x8000000000000000, -1)", value_t{ -1 } },                // negative bits, result filled as sign bit
        row{ "asr(0xFEDCBA9876543210)", value_t{ INT64_C(0xFF6E5D4C3B2A1908) } },
                                                                            // non-0/1 value, default bits
        row{ "asr(0x7EDCBA9876543210)", value_t{ INT64_C(0x3F6E5D4C3B2A1908) } },
                                                                            // non-0/1 value, default bits
        row{ "asr(8, 3, 4)", value_t{ 1 } },                                // value=1, 3 bits, size=4 bit
        row{ "asr(-1, 3, 4)", value_t{ 15 } },                              // value=-1, 3 bits, size=4 bit
        row{ "asr(0x8000000000000000, 3, 64)", value_t{ INT64_C(0xF000000000000000) } },
                                                                            // value=-1, 3 bits, size=max
        row{ "asr(0x8000000000000000, 3, 65)", value_t{ INT64_C(0xF000000000000000) } },
                                                                            // value=-1, 3 bits, size=over max (clamped)

        // TOK_GET - get(address, #bytes) - uses view/doc

        // pow(value, exponent)
        row{ "POW(1, 0)", value_t{ 1 } },                       // uppercase
        row{ "pow(1, 0)", value_t{ 1 } },                       // integers; x^0 = 1
        row{ "pow(1, 10)", value_t{ 1 } },                      // integers
        row{ "pow(2, 0)", value_t{ 1 } },                       // integers; x^0 = 1 for non-1 base
        row{ "pow(2, 10)", value_t{ 1024 } },                   // more integers
        row{ "pow(2, -1)", value_t{ 0 } },                      // integers, negative exponent = 0
        row{ "pow(1.0, 0.0)", value_t{ 1.0 } },                 // reals; x^0 = 1
        row{ "pow(1.0, 10)", value_t{ 1.0 } },                  // reals
        row{ "pow(2, 0.0)", value_t{ 1.0 } },                   // reals x^0 = 1 for non-1 base
        row{ "pow(2, 10.0)", value_t{ 1024.0 } },               // int base, real exponent => real
        row{ "pow(2, 0.5)", value_t{ 1.414213562373095048 } },  // fractional exponent => roots
        row{ "pow(2.0, -1)", value_t{ 0.5 } },                  // negative exponent => division

        // sqrt(value)
        row{ "SQRT(0)", value_t{ 0 } },                                         // uppercase
        row{ "sqrt(0)", value_t{ 0 } },                                         // integer, 0 => 0
        row{ "sqrt(1)", value_t{ 1 } },                                         // integer, 1 => 1
        row{ "sqrt(4611686018427387904)", value_t{ INT64_C(2147483648) } },     // integer, 2^62 => 2^31
        row{ "sqrt(0.0)", value_t{ 0.0 } },                                     // real, 0 => 0
        row{ "sqrt(1.0)", value_t{ 1.0 } },                                     // real, 1 => 1
        row{ "sqrt(4611686018427387904.0)", value_t{ 2147483648.0 } },          // real, 2^62 => 2^31

        // sin(value)
        row{ "SIN(0)", value_t{ 0.0 } },                        // uppercase
        row{ "sin(0)", value_t{ 0.0 } },                        // integer
        row{ "sin(1)", value_t{ 0.84147098480789650 } },        //  . . .
        row{ "sin(2)", value_t{ 0.90929742682568169 } },        //  . . .
        row{ "sin(3)", value_t{ 0.14112000805986722 } },        //  . . .
        row{ "sin(0.0)", value_t{ 0.0 } },                      // real
        row{ "sin(1.5707963267948966)", value_t{ 1.0 } },       //  . . .
        row{ "sin(3.1415926535897932)", value_t{ 0.0 } },       //  . . .
        row{ "sin(4.7123889803846898)", value_t{ -1.0 } },      //  . . .

        // cos(value)
        row{ "COS(0)", value_t{ 1.0 } },                        // uppercase
        row{ "cos(0)", value_t{ 1.0 } },                        // integer
        row{ "cos(1)", value_t{ 0.54030230586813971 } },        //  . . .
        row{ "cos(2)", value_t{ -0.41614683654714238 } },       //  . . .
        row{ "cos(3)", value_t{ -0.98999249660044545 } },       //  . . .
        row{ "cos(0.0)", value_t{ 1.0 } },                      // real
        row{ "cos(1.5707963267948966)", value_t{ 0.0 } },       //  . . .
        row{ "cos(3.1415926535897932)", value_t{ -1.0 } },      //  . . .
        row{ "cos(4.7123889803846898)", value_t{ 0.0 } },       //  . . .

        // tan(value)
        row{ "TAN(0)", value_t{ 0.0 } },                        // uppercase
        row{ "tan(0)", value_t{ 0.0 } },                        // integer
        row{ "tan(1)", value_t{ 1.55740772465490223 } },        //  . . .
        row{ "tan(2)", value_t{ -2.18503986326151899 } },       //  . . .
        row{ "tan(3)", value_t{ -0.14254654307427780 } },       //  . . .
        row{ "tan(0.0)", value_t{ 0.0 } },                      // real
        row{ "tan(0.7853981633974483)", value_t{ 1.0 } },       //  . . .
        row{ "tan(3.1415926535897932)", value_t{ 0.0 } },       //  . . .

        // asin(value)
        row{ "ASIN(0)", value_t{ 0.0 } },                       // uppercase
        row{ "asin(0)", value_t{ 0.0 } },                       // integer
        row{ "asin(1)", value_t{ 1.5707963267948966 } },        //  . . .
        row{ "asin(-1)", value_t{ -1.5707963267948966 } },      //  . . .
        row{ "asin(0.0)", value_t{ 0.0 } },                     // real
        row{ "asin(1.0)", value_t{ 1.5707963267948966 } },      //  . . .
        row{ "asin(-1.0)", value_t{ -1.5707963267948966 } },    //  . . .

        // acos(value)
        row{ "ACOS(0)", value_t{ 1.5707963267948966 } },        // uppercase
        row{ "acos(0)", value_t{ 1.5707963267948966 } },        // integer
        row{ "acos(1)", value_t{ 0.0 } },                       //  . . .
        row{ "acos(-1)", value_t{ 3.1415926535897932 } },       //  . . .
        row{ "acos(0.0)", value_t{ 1.5707963267948966 } },      // real
        row{ "acos(0.0)", value_t{ 1.5707963267948966 } },      //  . . .
        row{ "acos(1.0)", value_t{ 0.0 } },                     //  . . .
        row{ "acos(-1.0)", value_t{ 3.1415926535897932 } },     //  . . .

        // atan(value)
        row{ "ATAN(0)", value_t{ 0.0 } },                       // uppercase
        row{ "atan(0)", value_t{ 0.0 } },                       // integer
        row{ "atan(1)", value_t{ 0.7853981633974483 } },        //  . . .
        row{ "atan(-1)", value_t{ -0.7853981633974483 } },      //  . . .
        row{ "atan(0.0)", value_t{ 0.0 } },                     // real
        row{ "atan(1.0)", value_t{ 0.7853981633974483 } },      //  . . .
        row{ "atan(-1.0)", value_t{ -0.7853981633974483 } },    //  . . .

        // exp(value)
        row{ "EXP(0)", value_t{ 1.0 } },                        // uppercase
        row{ "exp(-2)", value_t{ 0.1353352832366126 } },        // integer
        row{ "exp(-1)", value_t{ 0.3678794411714423 } },        //  . . .
        row{ "exp(0)", value_t{ 1.0 } },                        //  . . .
        row{ "exp(1)", value_t{ 2.7182818284590452 } },         //  . . .
        row{ "exp(2)", value_t{ 7.3890560989306502 } },         //  . . .
        row{ "exp(-1.0)", value_t{ 0.3678794411714423 } },      //  . . .
        row{ "exp(-0.5)", value_t{ 0.6065306597126334 } },      // real
        row{ "exp(0.0)", value_t{ 1.0 } },                      //  . . .
        row{ "exp(0.5)", value_t{ 1.6487212707001281 } },       //  . . .
        row{ "exp(1.0)", value_t{ 2.7182818284590452 } },       //  . . .
        row{ "exp(PI)", value_t{ 23.140692632779269 } },        //  . . .

        // log(value)
        row{ "LOG(1)", value_t{ 0.0 } },                        // uppercase
        row{ "log(1)", value_t{ 0.0 } },                        // integer
        row{ "log(10)", value_t{ 2.3025850929940456 } },        //  . . .
        row{ "log(100)", value_t{ 4.6051701859880913 } },       //  . . .
        row{ "log(0.5)", value_t{ -0.6931471805599453 } },      // real
        row{ "log(1.0)", value_t{ 0.0 } },                      //  . . .
        row{ "log(10.0)", value_t{ 2.3025850929940456 } },      //  . . .
        row{ "log(148.41315910257660)", value_t{ 5.0 } },       //  . . .

        // int(value)
        row{ "INT(0)", value_t{ 0 } },                          // uppercase
        row{ "int(0)", value_t{ 0 } },                          // integers
        row{ "int(123)", value_t{ 123 } },                      //  . . .
        row{ "int(0.0)", value_t{ 0 } },                        // reals
        row{ "int(123.456)", value_t{ 123 } },                  //  . . .
        row{ "int(false)", value_t{ 0 } },                      // booleans
        row{ "int(true)", value_t{ 1 } },                       //  . . .

        // date(value)
        row{ "DATE(\"11/12/2021\")", value_t{ COleDateTime{ 2021, 11, 12, 0, 0, 0 } } },
        row{ "date(\"   11/12/2021\")", value_t{ COleDateTime{ 2021, 11, 12, 0, 0, 0 } } },
        row{ "date(\"11/12/2021   \")", value_t{ COleDateTime{ 2021, 11, 12, 0, 0, 0 } } },
        row{ "date(\"11/12/2021\")", value_t{ COleDateTime{ 2021, 11, 12, 0, 0, 0 } } },
        row{ "date(\"11/12/2021 20:06:15\")", value_t{ COleDateTime{ 2021, 11, 12, 20, 6, 15 } } },
        row{ "date(\"11/12/2021 , 20:06:15\")", value_t{ COleDateTime{ 2021, 11, 12, 20, 6, 15 } } },
        row{ "date(\"\")", value_t{ COleDateTime{ InvalidDate } } },
        row{ "date(\"13/32/9999\")", value_t{ COleDateTime{ InvalidDate } } },
        row{ "date(\"11/12/2021 23:59:60\")", value_t{ COleDateTime{ InvalidDate } } },
        row{ "date(\"11/12/2021 2A:59:60\")", value_t{ COleDateTime{ InvalidDate } } },

        // time(value)
        row{ "TIME(\"20:06:15\")", value_t{ COleDateTime{ 1899, 12, 30, 20, 6, 15 } } },
        row{ "time(\"   20:06:15\")", value_t{ COleDateTime{ 1899, 12, 30, 20, 6, 15 } } },
        row{ "time(\"20:06:15   \")", value_t{ COleDateTime{ 1899, 12, 30, 20, 6, 15 } } },
        row{ "time(\"20:06:15\")", value_t{ COleDateTime{ 1899, 12, 30, 20, 6, 15 } } },
        row{ "time(\"\")", value_t{ COleDateTime{ InvalidDate } } },
        row{ "time(\"23:59:60\")", value_t{ COleDateTime{ InvalidDate } } },
        row{ "time(\"2A:59:60\")", value_t{ COleDateTime{ InvalidDate } } },

        // TOK_NOW - now() - uses system time, can't test exact value

        // year(value)
        row{ "YEAR(0)", value_t{ 0.0 } },                       // uppercase
        row{ "year(0)", value_t{ 0.0 } },                       // integer
        row{ "year(1)", value_t{ 365.0 } },                     //  . . .
        row{ "year(2021)", value_t{ 738170.0 } },               //  . . .
        row{ "year(0.5)", value_t{ 183.0 } },                   // real
        row{ "year(1.25)", value_t{ 457.0 } },                  //  . . .
        row{ "year(2021.9)", value_t{ 738499.0 } },             //  . . .

        // month(value)
        row{ "MONTH(0)", value_t{ 0.0 } },                      // uppercase
        row{ "month(0)", value_t{ 0.0 } },                      // integer
        row{ "month(1)", value_t{ 30.0 } },                     //  . . .
        row{ "month(12)", value_t{ 365.0 } },                   //  . . .
        row{ "month(0.5)", value_t{ 15.0 } },                   // real
        row{ "month(0.75)", value_t{ 23.0 } },                  //  . . .
        row{ "month(PI)", value_t{ 96.0 } },                    //  . . .

        // day(value)
        row{ "DAY(0)", value_t{ 0.0 } },                        // uppercase
        row{ "day(0)", value_t{ 0.0 } },                        // integer
        row{ "day(1)", value_t{ 1.0 } },                        //  . . .
        row{ "day(12)", value_t{ 12.0 } },                      //  . . .
        row{ "day(0.5)", value_t{ 1.0 } },                      // real
        row{ "day(123.654)", value_t{ 124.0 } },                //  . . .
        row{ "day(PI)", value_t{ 3.0 } },                       //  . . .

        // hour(value)
        row{ "HOUR(0)", value_t{ 0.0 } },                       // uppercase
        row{ "hour(0)", value_t{ 0.0 } },                       // integer
        row{ "hour(1)", value_t{ 0.04166666666666667 } },       //  . . .
        row{ "hour(12)", value_t{ 0.5 } },                      //  . . .
        row{ "hour(0.5)", value_t{ 0.02083333333333333 } },     // real
        row{ "hour(24.0)", value_t{ 1.0 } },                    //  . . .
        row{ "hour(PI)", value_t{ 0.13089969389957472 } },      //  . . .

        // minute(value)
        row{ "MINUTE(0)", value_t{ 0.0 } },                     // uppercase
        row{ "minute(0)", value_t{ 0.0 } },                     // integer
        row{ "minute(1)", value_t{ 6.9444444444444444e-4 } },   //  . . .
        row{ "minute(60)", value_t{ 0.04166666666666667 } },    //  . . .
        row{ "minute(0.5)", value_t{ 3.4722222222222222e-4 } }, // real
        row{ "minute(1440.0)", value_t{ 1.0 } },                //  . . .
        row{ "minute(PI)", value_t{ 2.18166156499291197e-3 } }, //  . . .

        // second(value)
        row{ "SECOND(0)", value_t{ 0.0 } },                     // uppercase
        row{ "second(0)", value_t{ 0.0 } },                     // integer
        row{ "second(1)", value_t{ 1.1574074074074074e-5 } },   //  . . .
        row{ "second(60)", value_t{ 6.9444444444444444e-4 } },  //  . . .
        row{ "second(0.5)", value_t{ 5.7870370370370370e-6 } }, // real
        row{ "second(86400.0)", value_t{ 1.0 } },               //  . . .
        row{ "second(PI)", value_t{ 3.63610260832151995e-5 } }, //  . . .

        // atoi(value)
        row{ "ATOI(\"0\")", value_t{ 0 } },
        row{ "atoi(\"0\")", value_t{ 0 } },
        row{ "atoi(\"1\")", value_t{ 1 } },
        row{ "atoi(\"1000000\")", value_t{ 1000000 } },
        row{ "atoi(\"-23\")", value_t{ -23 } },

        // atof(value)
        row{ "ATOF(\"0\")", value_t{ 0.0 } },
        row{ "atof(\"0\")", value_t{ 0.0 } },
        row{ "atof(\"1\")", value_t{ 1.0 } },
        row{ "atof(\"0.123\")", value_t{ 0.123 } },
        row{ "atof(\"123.456\")", value_t{ 123.456 } },
        row{ "atof(\"-12345.6789\")", value_t{ -12345.6789 } },

        // TOK_RAND - rand() - non-deterministic output

        // strlen(string)
        row{ "STRLEN(\"\")", value_t{ 0 } },                    // uppercase
        row{ "strlen(\"\")", value_t{ 0 } },                    // empty string
        row{ "strlen(\"A\")", value_t{ 1 } },                   // non-empty string
        row{ "strlen(\"Test string\")", value_t{ 11 } },        //  . . .

        // left(string, count)
        row{ "LEFT(\"\", 0)", value_t{ "" } },                  // uppercase
        row{ "left(\"\", 0)", value_t{ "" } },                  // empty string
        row{ "left(\"TEST\", 0)", value_t{ "" } },              // non-empty string
        row{ "left(\"TEST\", 1)", value_t{ "T" } },             //  . . .
        row{ "left(\"TEST\", 2)", value_t{ "TE" } },            //  . . .
        row{ "left(\"TEST\", 3)", value_t{ "TES" } },           //  . . .
        row{ "left(\"TEST\", 4)", value_t{ "TEST" } },          //  . . .

        // right(string, count)
        row{ "RIGHT(\"\", 0)", value_t{ "" } },                 // uppercase
        row{ "right(\"\", 0)", value_t{ "" } },                 // empty string
        row{ "right(\"TEST\", 0)", value_t{ "" } },             // non-empty string
        row{ "right(\"TEST\", 1)", value_t{ "T" } },            //  . . .
        row{ "right(\"TEST\", 2)", value_t{ "ST" } },           //  . . .
        row{ "right(\"TEST\", 3)", value_t{ "EST" } },          //  . . .
        row{ "right(\"TEST\", 4)", value_t{ "TEST" } },         //  . . .

        // mid(string, start [, count])
        row{ "MID(\"\", 0)", value_t{ "" } },                   // uppercase, 2-arg
        row{ "MID(\"\", 0, 0)", value_t{ "" } },                // uppercase, 3-arg
        row{ "mid(\"\", 0)", value_t{ "" } },                   // empty string, 2-arg
        row{ "MID(\"\", 0, 0)", value_t{ "" } },                // uppercase, 3-arg
        row{ "mid(\"TEST\", 0)", value_t{ "TEST" } },           // non-empty string
        row{ "mid(\"TEST\", 1)", value_t{ "EST" } },            //  . . .
        row{ "mid(\"TEST\", 2)", value_t{ "ST" } },             //  . . .
        row{ "mid(\"TEST\", 3)", value_t{ "T" } },              //  . . .
        row{ "mid(\"TEST\", 4)", value_t{ "" } },               //  . . .
        row{ "mid(\"TEST\", 0, 1)", value_t{ "T" } },           //  . . .
        row{ "mid(\"TEST\", 1, 1)", value_t{ "E" } },           //  . . .
        row{ "mid(\"TEST\", 2, 1)", value_t{ "S" } },           //  . . .
        row{ "mid(\"TEST\", 3, 1)", value_t{ "T" } },           //  . . .
        row{ "mid(\"TEST\", 4, 1)", value_t{ "" } },            //  . . .

        // ltrim(string)
        row{ "LTRIM(\"X\")", value_t{ "X" } },                  // uppercase
        row{ "ltrim(\"X\")", value_t{ "X" } },                  // no leading spaces
        row{ "ltrim(\"  X  \")", value_t{ "X  " } },            // yes leading spaces

        // rtrim(string)
        row{ "RTRIM(\"X\")", value_t{ "X" } },                  // uppercase
        row{ "rtrim(\"X\")", value_t{ "X" } },                  // no trailing spaces
        row{ "rtrim(\"  X  \")", value_t{ "  X" } },            // yes trailing spaces

        // strchr(string, char)
        row{ "STRCHR(\"\", 0)", value_t{ 0 } },                 // uppercase
        row{ "strchr(\"\", 0)", value_t{ 0 } },                 // whatever this is
        row{ "strchr(\"TEST\", 'T')", value_t{ 0 } },           // non-empty search string
        row{ "strchr(\"TEST\", 'E')", value_t{ 1 } },           // non-empty search string
        row{ "strchr(\"TEST\", 'S')", value_t{ 2 } },           // non-empty search string
        row{ "strchr(\"TEST\", 'X')", value_t{ -1 } },          // not found

        // strstr(string, substring)
        row{ "STRSTR(\"\", \"\")", value_t{ 0 } },              // uppercase
        row{ "strstr(\"\", \"\")", value_t{ 0 } },              // whatever this is
        row{ "strstr(\"TEST\", \"T\")", value_t{ 0 } },         // non-empty search string
        row{ "strstr(\"TEST\", \"E\")", value_t{ 1 } },         // non-empty search string
        row{ "strstr(\"TEST\", \"S\")", value_t{ 2 } },         // non-empty search string
        row{ "strstr(\"TEST\", \"ST\")", value_t{ 2 } },        // non-empty search string
        row{ "strstr(\"TEST\", \"X\")", value_t{ -1 } },        // not found

        // strcmp(string, comparand)
        row{ "STRCMP(\"\", \"\")", value_t{ 0 } },              // uppercase
        row{ "strcmp(\"\", \"\")", value_t{ 0 } },              // empty == empty
        row{ "strcmp(\"A\", \"a\")", value_t{ -1 } },           // upper < lower
        row{ "strcmp(\"A\", \"B\")", value_t{ -1 } },           // ordinal order
        row{ "strcmp(\"AA\", \"A\")", value_t{ +1 } },          // longer > shorter

        // stricmp(string, comparand)
        row{ "STRICMP(\"\", \"\")", value_t{ 0 } },             // uppercase
        row{ "stricmp(\"\", \"\")", value_t{ 0 } },             // empty == empty
        row{ "stricmp(\"A\", \"a\")", value_t{ 0 } },           // upper == lower
        row{ "stricmp(\"A\", \"B\")", value_t{ -1 } },          // ordinal order
        row{ "stricmp(\"AA\", \"A\")", value_t{ +1 } },         // longer > shorter

        // asc2ebcd(string)
        row{ "ASC2EBC(\"\")", value_t{ "" } },                      // uppercase
        row{ "asc2ebc(\"\")", value_t{ "" } },                      // empty string
        row{ "asc2ebc(\"TEST\")", value_t{ "\xE3\xC5\xE2\xE3" } },  // non-empty string
        row{ "asc2ebc(\"\xC0\")", value_t{ "\x6F" } },              // unmappable character

        // ebd2asc(string)
        row{ "EBC2ASC(\"\")", value_t{ "" } },                      // uppercase
        row{ "ebc2asc(\"\")", value_t{ "" } },                      // empty string
        row{ "ebc2asc(\"\xE3\xC5\xE2\xE3\")", value_t{ "TEST" } },  // non-empty string
        row{ "ebc2asc(\"\x30\")", value_t{ "?" } },                 // unmappable character

        // TOK_SYMBOL
        row{ "intField", value_t{ 12345 } },                        // integer field symbol
        row{ "realField", value_t{ 123.456 } },                     // real field symbol
        row{ "arrayField[0]", value_t{ 5 } },                       // array element symbol
        row{ "structField.value1", value_t{ 7 } },                  // struct field element
        row{ "blobField[2]", value_t{ 89 } },                       // blob field element
        row{ "stringField[2]", value_t{ 's' } },                    // string element
        row{ "INT_VAR", value_t{ 123 } },                           // int variable
        row{ "BOOL_VAR", value_t{ true } },                         // boolean variable
        row{ "REAL_VAR", value_t{ 123.456 } },                      // real variable
        row{ "STRING_VAR", value_t{ "string_var" } },               // string variable
        row{ "DATE_VAR", value_t{ COleDateTime{ 2021, 11, 10, 20, 29, 05 } } },
                                                                    // date variable

        // TOK_LPAR
        row{ "(0)", value_t{ 0 } },
        row{ "(intField)", value_t{ 12345 } },

        // TOK_PLUS
        row{ "+0", value_t{ 0 } },
        row{ "+5", value_t{ 5 } },
        row{ "+0.1", value_t{ 0.1 } },
        row{ "+5.5", value_t{ 5.5 } },

        // TOK_MINUS
        row{ "-0", value_t{ 0 } },
        row{ "-5", value_t{ -5 } },
        row{ "-0.1", value_t{ -0.1 } },
        row{ "-5.5", value_t{ -5.5 } },

        // TOK_NOT
        row{ "!BOOL_VAR", value_t{false} },

        // TOK_BITNOT
        row{ "~0", value_t{ INT64_C(0xFFFFFFFFFFFFFFFF) } },
        row{ "~0xFFFFFFFFFFFFFFFF", value_t{ 0 } }
    );

    // TODO: refactor app code so we don't need this global state.
    theApp.dec_point_ = '.';
    theApp.dec_sep_char_ = ',';

    test_expr expr;

    expr.set_variable("INT_VAR", value_t{ 123 });
    expr.set_variable("BOOL_VAR", value_t{ true });
    expr.set_variable("REAL_VAR", value_t{ 123.456 });
    expr.set_variable("STRING_VAR", value_t{ "string_var" });
    expr.set_variable("DATE_VAR", value_t{ COleDateTime{ 2021, 11, 10, 20, 29, 05 } });
    expr.set_variable("ARR_VAR[0]", value_t{ 456 });

    int ref_ac = 0;
    value_t actual = expr.evaluate(test.expression, 0, ref_ac);

    CAPTURE(test.expression, test.expected, actual);
    CAPTURE(expr.get_error_message());

    CHECK(actual.error == false);
    CHECK(values_equal(test.expected, actual));
}

TEST_CASE("expr_eval::evaluate - string embedded NUL")
{
    test_expr expr;

    int ref_ac = 0;
    COleDateTime nowTime = COleDateTime::GetCurrentTime();
    value_t actual = expr.evaluate("\"a\\0a\"", 0, ref_ac);

    CHECK(actual.error == false);
    CHECK(actual.typ == expr_eval::TYPE_STRING);

    CHECK(actual.pstr->GetLength() == 1);
    CHECK(actual.pstr->GetAt(0) == 'a');
    WARN("!!! Possible bug in expr_eval::get_next - string allows for embedding NULs"
        " using \\0 or \\x escape sequences, but only characters before that are emitted.");
}

TEST_CASE("expr_eval::evaluate - now()")
{
    test_expr expr;

    int ref_ac = 0;
    COleDateTime nowTime = COleDateTime::GetCurrentTime();
    value_t actual = expr.evaluate("now()", 0, ref_ac);

    CHECK(actual.error == false);
    CHECK(actual.typ == expr_eval::TYPE_DATE);

    // just make sure it's less than 1 second away from the actual `now` time.
    COleDateTimeSpan diff = COleDateTime{ actual.date } - nowTime;
    CHECK(diff.m_span < COleDateTimeSpan{ 0, 0, 0, 1 }.m_span);
}

TEST_CASE("expr_eval::evaluate - rand()")
{
    // TODO: refactor app code so we don't need this global state.
    // seed the RNG with a fixed seed so we can assert an exact result later
    rand_good_seed(0);

    test_expr expr;

    int ref_ac = 0;
    value_t actual = expr.evaluate("rand()", 0, ref_ac);

    CHECK(actual.error == false);
    CHECK(actual.typ == expr_eval::TYPE_INT);
    CHECK(actual.int64 == INT64_C(0x8c7f0aac97c4aa2f));
}

TEST_CASE("expr_eval::evaluate - post-increment")
{
    bool sideEffects = GENERATE(false, true);

    test_expr expr;
    expr.set_variable("INT_VAR", value_t{ 123 });

    int ref_ac = 0;
    value_t actual = expr.evaluate("INT_VAR++", 0, ref_ac, 10, sideEffects);

    // check evaluation result
    CHECK(actual.error == false);
    CHECK(actual.typ == expr_eval::TYPE_INT);
    CHECK(actual.int64 == 123);
    
    // check the variable
    bool gotVar = expr.get_variable("INT_VAR", actual);
    CHECK(gotVar);
    CHECK(actual.typ == expr_eval::TYPE_INT);
    int expectedVarValue = 123 + (sideEffects ? 1 : 0);
    CHECK(actual.int64 == expectedVarValue);
}

TEST_CASE("expr_eval::evaluate - post-decrement")
{
    bool sideEffects = GENERATE(false, true);

    test_expr expr;
    expr.set_variable("INT_VAR", value_t{ 123 });

    int ref_ac = 0;
    value_t actual = expr.evaluate("INT_VAR--", 0, ref_ac, 10, sideEffects);

    // check evaluation result
    CHECK(actual.error == false);
    CHECK(actual.typ == expr_eval::TYPE_INT);
    CHECK(actual.int64 == 123);

    // check the variable
    bool gotVar = expr.get_variable("INT_VAR", actual);
    CHECK(gotVar);
    CHECK(actual.typ == expr_eval::TYPE_INT);
    int expectedVarValue = 123 - (sideEffects ? 1 : 0);
    CHECK(actual.int64 == expectedVarValue);
}

TEST_CASE("expr_eval::evaluate - pre-increment")
{
    bool sideEffects = GENERATE(false, true);

    test_expr expr;
    expr.set_variable("INT_VAR", value_t{ 123 });

    int ref_ac = 0;
    value_t actual = expr.evaluate("++INT_VAR", 0, ref_ac, 10, sideEffects);

    // check evaluation result
    CHECK(actual.error == false);
    CHECK(actual.typ == expr_eval::TYPE_INT);
    CHECK(actual.int64 == 124);
    
    // check the variable
    bool gotVar = expr.get_variable("INT_VAR", actual);
    CHECK(gotVar);
    CHECK(actual.typ == expr_eval::TYPE_INT);
    int expectedVarValue = 123 + (sideEffects ? 1 : 0);
    CHECK(actual.int64 == expectedVarValue);
}

TEST_CASE("expr_eval::evaluate - pre-decrement")
{
    bool sideEffects = GENERATE(false, true);

    test_expr expr;
    expr.set_variable("INT_VAR", value_t{ 123 });

    int ref_ac = 0;
    value_t actual = expr.evaluate("--INT_VAR", 0, ref_ac, 10, sideEffects);

    // check evaluation result
    CHECK(actual.error == false);
    CHECK(actual.typ == expr_eval::TYPE_INT);
    CHECK(actual.int64 == 122);

    // check the variable
    bool gotVar = expr.get_variable("INT_VAR", actual);
    CHECK(gotVar);
    CHECK(actual.typ == expr_eval::TYPE_INT);
    int expectedVarValue = 123 - (sideEffects ? 1 : 0);
    CHECK(actual.int64 == expectedVarValue);
}

TEST_CASE("expr_eval::evaluate - assignment")
{
    struct row
    {
        const char* expression;
        const char* variable;
        value_t expected;
    };

    row test = GENERATE(
        row{                                // assign integer
            "INT_VAR = 5",
            "INT_VAR",
            value_t{5}
        },
        row{                                // assign boolean
            "BOOL_VAR = false",
            "BOOL_VAR",
            value_t{ false }
        },
        row{                                // assign real
            "REAL_VAR = 1.23",
            "REAL_VAR",
            value_t{1.23}
        },
        row{                                // assign string
            "STRING_VAR = \"X\"",
            "STRING_VAR",
            value_t{"X"}
        },
        row{                                // assign date
            "DATE_VAR = DATE(\"11/13/2021\")",
            "DATE_VAR",
            value_t{COleDateTime{2021, 11, 13, 0, 0, 0}}
        },
        row{                                // add-equal integer
            "INT_VAR += 5",
            "INT_VAR",
            value_t{ 10 }
        },
        row{                                // add-equal real
            "REAL_VAR += 5.0",
            "REAL_VAR",
            value_t{ 128.456 }
        },
        row{                                // add-equal integer to real
            "REAL_VAR += 10",
            "REAL_VAR",
            value_t{ 133.456 }
        },
        row{                                // assign string
            "STRING_VAR += \"X\"",
            "STRING_VAR",
            value_t{ "string_varX" }
        }
    );

    test_expr expr;

    expr.set_variable("INT_VAR", value_t{ 5 });
    expr.set_variable("BOOL_VAR", value_t{ true });
    expr.set_variable("REAL_VAR", value_t{ 123.456 });
    expr.set_variable("STRING_VAR", value_t{ "string_var" });
    expr.set_variable("DATE_VAR", value_t{ COleDateTime{ 2021, 11, 10, 20, 29, 05 } });

    int ref_ac = 0;
    value_t actual = expr.evaluate(test.expression, 0, ref_ac);

    value_t variable;
    CHECK(expr.get_variable(test.variable, variable));

    CAPTURE(test.expression, test.expected, actual, variable);
    CAPTURE(expr.get_error_message());

    // check the result value
    CHECK(actual.error == false);
    CHECK(values_equal(test.expected, actual));
    CHECK(values_equal(test.expected, variable));
}

TEST_CASE("expr_eval::evaluate - short circuiting")
{
    struct row
    {
        const char* expression;
        value_t expected;
    };

    row test = GENERATE(
        row{                                // TRUE && side-effect inside FALSE (mutation)
            "TRUE && ((INT_VAR = 0) > 0)",
            value_t{ 0 }
        },
        row{                                // TRUE && side-effect inside TRUE (mutation)
            "TRUE && ((INT_VAR = 0) == 0)",
            value_t{ 0 }
        },
        row{                                // FALSE && side-effect inside FALSE (no mutation)
            "FALSE && ((INT_VAR = 0) > 0)",
            value_t{ 123 }
        },
        row{                                // FALSE && side-effect inside TRUE (no mutation)
            "FALSE && ((INT_VAR = 0) == 0)",
            value_t{ 123 }
        },

        row{                                // TRUE || side-effect inside FALSE (no mutation)
            "TRUE || ((INT_VAR = 0) > 0)",
            value_t{ 123 }
        },
        row{                                // TRUE || side-effect inside TRUE (no mutation)
            "TRUE || ((INT_VAR = 0) == 0)",
            value_t{ 123 }
        },
        row{                                // FALSE || side-effect inside FALSE (mutation)
            "FALSE || ((INT_VAR = 0) > 0)",
            value_t{ 0 }
        },
        row{                                // FALSE || side-effect inside TRUE (mutation)
            "FALSE || ((INT_VAR = 0) == 0)",
            value_t{ 0 }
        }
    );

    test_expr expr;

    expr.set_variable("INT_VAR", value_t{ 123 });

    int ref_ac = 0;
    expr.evaluate(test.expression, 0, ref_ac);

    value_t variable;
    CHECK(expr.get_variable("INT_VAR", variable));

    CAPTURE(test.expression, test.expected, variable);
    CAPTURE(expr.get_error_message());

    // check the variable's value
    CHECK(variable.error == false);
    CHECK(values_equal(test.expected, variable));
}

TEST_CASE("expr_eval::evaluate - comparison types")
{
    struct row
    {
        const char* expression;
        value_t expected;
    };

    row test = GENERATE(
        // equal
        row{ "\"X\" == \"X\"", value_t{ true } },       // string true
        row{ "\"X\" == \"x\"", value_t{ false } },      // string false
        row{ "DATE(\"11/13/2021\") == DATE(\"11/13/2021\")", value_t{ true } },
                                                        // date true
        row{ "DATE(\"11/13/2021\") == DATE(\"11/12/2021\")", value_t{ false } },
                                                        // date false
        row{ "1.23 == 1.23", value_t{ true } },         // real true
        row{ "1.23 == 1.24", value_t{ false } },        // real false
        row{ "1 == 1", value_t{ true } },               // integer true
        row{ "1 == 2", value_t{ false } },              // integer false
        row{ "1.0 == 1", value_t{ true } },             // real-int true
        row{ "1.23 == 1", value_t{ false } },           // real-int false
        row{ "1 == 1.0", value_t{ true } },             // int-real true
        row{ "1 == 1.23", value_t{ false } },           // int-real false
//        row{ "TRUE == TRUE", value_t{ true } },         // boolean true
//        row{ "TRUE == FALSE", value_t{ false } },       // boolean false

        // not equal
        row{ "\"X\" != \"X\"", value_t{ false } },      // string false
        row{ "\"X\" != \"x\"", value_t{ true } },       // string true
        row{ "DATE(\"11/13/2021\") != DATE(\"11/13/2021\")", value_t{ false } },
                                                        // date false
        row{ "DATE(\"11/13/2021\") != DATE(\"11/12/2021\")", value_t{ true } },
                                                        // date true
        row{ "1.23 != 1.23", value_t{ false } },        // real false
        row{ "1.23 != 1.24", value_t{ true } },         // real true
        row{ "1 != 1", value_t{ false } },              // integer false
        row{ "1 != 2", value_t{ true } },               // integer true
        row{ "1.0 != 1", value_t{ false } },            // real-int false
        row{ "1.23 != 1", value_t{ true } },            // real-int true
        row{ "1 != 1.0", value_t{ false } },            // int-real false
        row{ "1 != 1.23", value_t{ true } },            // int-real true
//        row{ "TRUE != TRUE", value_t{ false } },        // boolean false
//        row{ "TRUE != FALSE", value_t{ true } },        // boolean true

        // less than
        row{ "\"X\" < \"X\"", value_t{ false } },       // string false
        row{ "\"X\" < \"x\"", value_t{ true } },        // string true
        row{ "DATE(\"11/14/2021\") < DATE(\"11/13/2021\")", value_t{ false } },
                                                        // date false
        row{ "DATE(\"11/13/2021\") < DATE(\"11/14/2021\")", value_t{ true } },
                                                        // date true
        row{ "1.23 < 1.23", value_t{ false } },         // real false
        row{ "1.23 < 1.24", value_t{ true } },          // real true
        row{ "1 < 1", value_t{ false } },               // integer false
        row{ "1 < 2", value_t{ true } },                // integer true
        row{ "1.0 < 1", value_t{ false } },             // real-int false
        row{ "0.23 < 1", value_t{ true } },             // real-int true
        row{ "1 < 1.0", value_t{ false } },             // int-real false
        row{ "1 < 1.23", value_t{ true } },             // int-real true
//        row{ "TRUE < TRUE", value_t{ false } },         // boolean false
//        row{ "FALSE < TRUE", value_t{ true } },         // boolean true

        // less or equal
        row{ "\"Z\" <= \"X\"", value_t{ false } },      // string false
        row{ "\"X\" <= \"X\"", value_t{ true } },       // string true
        row{ "DATE(\"11/14/2021\") <= DATE(\"11/13/2021\")", value_t{ false } },
                                                        // date false
        row{ "DATE(\"11/13/2021\") <= DATE(\"11/13/2021\")", value_t{ true } },
                                                        // date true
        row{ "1.24 <= 1.23", value_t{ false } },        // real false
        row{ "1.23 <= 1.23", value_t{ true } },         // real true
        row{ "2 <= 1", value_t{ false } },              // integer false
        row{ "1 <= 1", value_t{ true } },               // integer true
        row{ "1.23 <= 1", value_t{ false } },           // real-int false
        row{ "1.0 <= 1", value_t{ true } },             // real-int true
        row{ "2 <= 1.23", value_t{ false } },           // int-real false
        row{ "1 <= 1.0", value_t{ true } },             // int-real true
//        row{ "TRUE <= TRUE", value_t{ false } },         // boolean false
//        row{ "FALSE <= TRUE", value_t{ true } },         // boolean true

        // greater than
        row{ "\"X\" > \"X\"", value_t{ false } },       // string false
        row{ "\"x\" > \"X\"", value_t{ true } },        // string true
        row{ "DATE(\"11/13/2021\") > DATE(\"11/14/2021\")", value_t{ false } },
                                                        // date false
        row{ "DATE(\"11/14/2021\") > DATE(\"11/13/2021\")", value_t{ true } },
                                                        // date true
        row{ "1.23 > 1.23", value_t{ false } },         // real false
        row{ "1.24 > 1.23", value_t{ true } },          // real true
        row{ "1 > 1", value_t{ false } },               // integer false
        row{ "2 > 1", value_t{ true } },                // integer true
        row{ "1.0 > 1", value_t{ false } },             // real-int false
        row{ "1.23 > 1", value_t{ true } },             // real-int true
        row{ "1 > 1.0", value_t{ false } },             // int-real false
        row{ "1 > 0.23", value_t{ true } },             // int-real true
//        row{ "TRUE > TRUE", value_t{ false } },         // boolean false
//        row{ "TRUE > FALSE", value_t{ true } },         // boolean true

        // greater or equal
        row{ "\"X\" >= \"Z\"", value_t{ false } },      // string false
        row{ "\"X\" >= \"X\"", value_t{ true } },       // string true
        row{ "DATE(\"11/13/2021\") >= DATE(\"11/14/2021\")", value_t{ false } },
                                                        // date false
        row{ "DATE(\"11/13/2021\") >= DATE(\"11/13/2021\")", value_t{ true } },
                                                        // date true
        row{ "1.23 >= 1.24", value_t{ false } },        // real false
        row{ "1.23 >= 1.23", value_t{ true } },         // real true
        row{ "1 >= 2", value_t{ false } },              // integer false
        row{ "1 >= 1", value_t{ true } },               // integer true
        row{ "1.23 >= 2", value_t{ false } },           // real-int false
        row{ "1.0 >= 1", value_t{ true } },             // real-int true
        row{ "1 >= 1.23", value_t{ false } },           // int-real false
        row{ "1 >= 1.0", value_t{ true } }              // int-real true
//        row{ "TRUE >= TRUE", value_t{ false } },         // boolean false
//        row{ "FALSE >= TRUE", value_t{ true } }          // boolean true
    );

    test_expr expr;

    int ref_ac = 0;
    value_t actual = expr.evaluate(test.expression, 0, ref_ac);

    CAPTURE(test.expression, test.expected, actual);
    CAPTURE(expr.get_error_message());

    // check the result value
    CHECK(actual.error == false);
    CHECK(values_equal(test.expected, actual));
}

TEST_CASE("expr_eval::evaluate - arithmetic types")
{
    struct row
    {
        const char* expression;
        value_t expected;
    };

    row test = GENERATE(
        // addition
        row{ "\"Y\" + \"Y\"", value_t{ "YY" } },                // string-string
        row{ "1.0 + 1.0", value_t{ 2.0 } },                     // real-real
        row{ "1 + 1", value_t{ 2 } },                           // int-int
        row{ "1.0 + 1", value_t{ 2.0 } },                       // real-int
        row{ "1 + 1.0", value_t{ 2.0 } },                       // int-real
        row{                                                    // date-int
            "DATE(\"11/13/2021\") + 1",
            value_t{ COleDateTime{ 2021, 11, 14, 0, 0, 0 } }
        },
        row{                                                    // date-real
            "DATE(\"11/13/2021\") + 1.5",
            value_t{ COleDateTime{ 2021, 11, 14, 12, 0, 0 } }
        },
        row{                                                    // int-date
            "1 + DATE(\"11/13/2021\")",
            value_t{ COleDateTime{ 2021, 11, 14, 0, 0, 0 } }
        },
        row{                                                    // real-date
            "1.5 + DATE(\"11/13/2021\")",
            value_t{ COleDateTime{ 2021, 11, 14, 12, 0, 0 } }
        },

        // subtraction
        row{ "1.0 - 1.0", value_t{ 0.0 } },                     // real-real
        row{ "1.0 - 1", value_t{ 0.0 } },                       // real-int
        row{ "1 - 1.0", value_t{ 0.0 } },                       // int-real
        row{ "1 - 1", value_t{ 0 } },                           // int-int
        row{                                                    // date-int
            "DATE(\"11/13/2021\") - 1",
            value_t{ COleDateTime{ 2021, 11, 12, 0, 0, 0 } }
        },
        row{                                                    // date-real
            "DATE(\"11/13/2021\") - 1.5",
            value_t{ COleDateTime{ 2021, 11, 11, 12, 0, 0 } }
        },
        row{                                                    // date-date
            "DATE(\"11/13/2021\") - DATE(\"11/12/2021\")",
            value_t{ 1.0 }
        },

        // multiplication
        row{ "2.0 * 2.0", value_t{ 4.0 } },         // real-real => real
        row{ "2.0 * 2", value_t{ 4.0 } },           // real-int  => real
        row{ "2 * 2.0", value_t{ 4.0 } },           // int-real  => real
        row{ "2 * 2", value_t{ 4 } },               // int-int   => int

        // division
        row{ "4.0 / 2.0", value_t{ 2.0 } },         // real-real => real
        row{ "4.0 / 2", value_t{ 2.0 } },           // real-int  => real
        row{ "4 / 2.0", value_t{ 2.0 } },           // int-real  => real
        row{ "4 / 2", value_t{ 2 } },               // int-int   => int

        // remainder
        row{ "5 % 2", value_t{ 1 } }                // int-int   => int
    );

    test_expr expr;

    int ref_ac = 0;
    value_t actual = expr.evaluate(test.expression, 0, ref_ac);

    CAPTURE(test.expression, test.expected, actual);
    CAPTURE(expr.get_error_message());

    // check the result value
    CHECK(actual.error == false);
    CHECK(values_equal(test.expected, actual));
}

TEST_CASE("expr_eval::evaluate - get_var indexing")
{
    test_expr expr;

    expr.set_variable("ARR_VAR[0]", value_t{ "TEST" });
    expr.set_variable("INT_VAR", value_t{ 0 });

    int ref_ac = 0;
    value_t actual = expr.evaluate("ARR_VAR[INT_VAR][1]", 0, ref_ac);

    value_t expected{ 'E' };

    CAPTURE(actual);
    CAPTURE(expr.get_error_message());

    // check the result value
    CHECK(actual.error == false);
    CHECK(values_equal(expected, actual));
}

TEST_CASE("expr_eval::evaluate - @identifier")
{
    test_expr expr;

    int ref_ac = 0;
    value_t actual = expr.evaluate("@123 = 123", 0, ref_ac);

    value_t expected{ 123 };

    CAPTURE(actual);
    CAPTURE(expr.get_error_message());

    // check the result value
    CHECK(actual.error == false);
    CHECK(values_equal(expected, actual));

    value_t variable;
    REQUIRE(expr.get_variable("123", variable));
    CHECK(values_equal(variable, expected));
}

TEST_CASE("expr_eval::evaluate - C++ scope delimiters")
{
    test_expr expr;

    int ref_ac = 0;
    value_t actual = expr.evaluate("STD::X = 123", 0, ref_ac);

    value_t expected{ 123 };

    CAPTURE(actual);
    CAPTURE(expr.get_error_message());

    // check the result value
    CHECK(actual.error == false);
    CHECK(values_equal(expected, actual));

    value_t variable;
    REQUIRE(expr.get_variable("STD::X", variable));
    CHECK(values_equal(variable, expected));
}

TEST_CASE("expr_eval::evaluate - non-decimal integers")
{
    test_expr expr{ 16 };

    int ref_ac = 0;
    value_t actual = expr.evaluate("A1", 0, ref_ac, 16);

    value_t expected{ 161 };

    CAPTURE(actual);
    CAPTURE(expr.get_error_message());

    // check the result value
    CHECK(actual.error == false);
    CHECK(values_equal(expected, actual));
}

TEST_CASE("expr_eval::evaluate - non-decimal integers with separator")
{
    test_expr expr{ 16, true };

    int ref_ac = 0;
    value_t actual = expr.evaluate("A1 b2", 0, ref_ac, 16);

    value_t expected{ 41394 };

    CAPTURE(actual);
    CAPTURE(expr.get_error_message());

    // check the result value
    CHECK(actual.error == false);
    CHECK(values_equal(expected, actual));
}

TEST_CASE("expr_eval::evaluate - digit separator")
{
    // TODO: refactor app code to not need this global state!
    theApp.dec_sep_char_ = ',';

    test_expr expr{ 10, true };

    int ref_ac = 0;
    value_t actual = expr.evaluate("123,456", 0, ref_ac);

    value_t expected{ 123456 };

    CAPTURE(actual);
    CAPTURE(expr.get_error_message());

    // check the result value
    CHECK(actual.error == false);
    CHECK(values_equal(expected, actual));
}


TEST_CASE("expr_eval::evaluate - errors")
{
    struct row
    {
        const char* expression;
        const char* expected;
    };

    row test = GENERATE(
        row{
            "1 &| 1",
            "Unexpected characters \"&|\""
        },
        row{
            "1 # 1",
            "Unexpected character \"#\""
        },
        row{
            "'x",
            "Expected ' after character constant"
        },
        row{
            "'\\x'",
            "Unexpected character constant escape sequence \\x"
        },
        row{
            "\"x",
            "Unterminated string constant"
        },
//        // can't be hit
//        row{
//            "\"\\w\"",
//            "Unexpected character escape sequence \\%w in string"
//        },
        row{
            "0x10000000000000000",
            "Overflow: Hex integer \"10000000000000000\" too big"
        },
        row{
            "1e+",
            "Invalid number: 1e+"
        },

        row{
            "ARR_VAR[0][30]",
            "Index on string out of bounds"
        },
        row{
            "ARR_VAR[\"X\"]",
            "Array index must be an integer"
        },
        row{
            "ARR_VAR[0",
            "Closing bracket (]) expected"
        },
        row{
            "ARR_VAR[0][",
            "Not implemented"
            // should be "Expected array index", but get_var does not overwrite the error
        },

        row{
            ",",
            "Not implemented"
            // note: this one *should* actually be the 'not implemented' message.
        },

        row{
            "~\"X\"",
            "Bitwise NOT operator must have an INTEGER operand"
        },
        row{
            "~",
            "Not implemented"
            // should be "Expected integer for bitwise NOT (~)", but prec_prim, case TOK_BITNOT does not overwrite the error
        },

        row{
            "!\"X\"",
            "NOT operator must have a BOOLEAN operand"
        },

        row{
            "!",
            "Not implemented"
            // should be "Expected integer for bitwise NOT (~)", but prec_prim, case TOK_NOT does not overwrite the error
        },

        row{
            "-\"X\"",
            "Invalid operand for negation"
        },
        row{
            "-",
            "Not implemented"
            // should be "Expected number after minus", but prec_prim, case TOK_MINUS does not overwrite the error
        },

        row{
            "+\"X\"",
            "Invalid operand for unary +"
        },
        row{
            "+",
            "Not implemented"
            // should be "Expected number after +", but prec_prim, case TOK_PLUS does not overwrite the error
        },

        row{
            "--\"X\"",
            "Pre-decrement (--) requires an integer variable"
        },
        row{
            "--",
            "Pre-decrement (--) requires an integer variable"
            // note: parsing subexpr is not wrapped in `error` call and return if true, so no 'not implemented' message expected
        },

        row{
            "++\"X\"",
            "Pre-increment (++) requires an integer variable"
        },
        row{
            "++",
            "Pre-increment (++) requires an integer variable"
            // note: parsing subexpr is not wrapped in `error` call and return if true, so no 'not implemented' message expected
        },

        row{
            "(0",
            "Closing parenthesis expected"
        },
        row{
            "(",
            "Not implemented"
            // should be "Expected number after +", but prec_prim, case TOK_LPAR does not overwrite the error
        },

        row{
            "end",
            "\"end\" is a reserved symbol"
        },
        row{
            "next",
            "\"next\" is a reserved symbol"
        },
        row{
            "index",
            "\"index\" is a reserved symbol"
        },
        row{
            "member",
            "\"member\" is a reserved symbol"
        },
        row{
            "intField.value1",
            "Dot operator (.) must only be used with struct"
        },
        row{
            "structField.",
            "Symbol expected"
            // should be "Expected member name"; prec_prim, case TOK_SYMBOL
            // This at least makes sense, so maybe just get rid of the `error` call here?
        },
        row{
            "structField.1e+",
            "Invalid number: 1e+"
            // "Expected member name"
        },
        row{
            "structField.1",
            "Symbol expected"
        },
        row{
            "structField.does_not_exist",
            "Unrecognised member name for struct"
        },
        row{
            "intField[0]",
            "Unexpected array index"
        },
        row{
            "arrayField[",
            "Not implemented"
            // should be "Expected member name"; prec_prim, case TOK_SYMBOL
        },
        row{
            "arrayField[0",
            "Closing bracket (]) expected"
        },
        row{
            "arrayField[\"X\"]",
            "Array (for) index must be an integer"
        },
        row{
            "arrayField[9999]",
            "Invalid array (for) index"
        },
        row{
            "blobField[9999]",
            "Invalid index into \"none\" DATA element"
        },
        row{
            "stringField[9999]",
            "Index on string out of bounds"
        },
        row{
            "structField",
            "Dot operator (.) expected after struct"
        },
        row{
            "arrayField",
            "Array index expected after for"
        },
        row{
            "STRING_VAR++",
            "Post-increment (++) requires an integer variable"
        },
        row{
            "STRING_VAR--",
            "Post-decrement (--) requires an integer variable"
        },

        row{
            "ebc2asc",
            "Opening parenthesis expected after \"ebc2asc\""
        },
        row{
            "ebc2asc(",
            "Not implemented"
            // Should be "Expected parameter for \"ebc2asc\""; prec_prim, TOK_E2A
        },
        row{
            "ebc2asc(\"A\"",
            "Closing parenthesis expected for \"ebc2asc\""
        },
        row{
            "ebc2asc(2)",
            "Parameter for \"ebc2asc\" must be a string"
        },

        row{
            "asc2ebc",
            "Opening parenthesis expected after \"asc2ebc\""
        },
        row{
            "asc2ebc(",
            "Not implemented"
            // Should be "Expected parameter for \"asc2ebc\""; prec_prim, TOK_A2E
        },
        row{
            "asc2ebc(\"A\"",
            "Closing parenthesis expected for \"asc2ebc\""
        },
        row{
            "asc2ebc(2)",
            "Parameter for \"asc2ebc\" must be a string"
        },

        row{
            "stricmp",
            "Opening parenthesis expected after \"stricmp\""
        },
        row{
            "stricmp(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"stricmp\""; prec_prim, TOK_STRICMP
        },
        row{
            "stricmp(2",
            "First parameter for \"stricmp\" must be a string"
        },
        row{
            "stricmp(\"X\")",
            "Expected comma and 2nd parameter for \"stricmp\""
        },
        row{
            "stricmp(\"X\",",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"stricmp\""; prec_prim, TOK_STRICMP
        },
        row{
            "stricmp(\"X\", 1)",
            "Second parameter for \"stricmp\" must be an string"
        },
        row{
            "stricmp(\"X\", \"x\"",
            "Closing parenthesis expected for \"stricmp\""
        },

        row{
            "strcmp",
            "Opening parenthesis expected after \"strcmp\""
        },
        row{
            "strcmp(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"strcmp\""; prec_prim, TOK_STRCMP
        },
        row{
            "strcmp(2",
            "First parameter for \"strcmp\" must be a string"
        },
        row{
            "strcmp(\"X\")",
            "Expected comma and 2nd parameter for \"strcmp\""
        },
        row{
            "strcmp(\"X\",",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"strcmp\""; prec_prim, TOK_STRCMP
        },
        row{
            "strcmp(\"X\", 1)",
            "Second parameter for \"strcmp\" must be an string"
        },
        row{
            "strcmp(\"X\", \"x\"",
            "Closing parenthesis expected for \"strcmp\""
        },

        row{
            "strstr",
            "Opening parenthesis expected after \"strstr\""
        },
        row{
            "strstr(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"strstr\""; prec_prim, TOK_STRSTR
        },
        row{
            "strstr(2",
            "First parameter for \"strstr\" must be a string"
        },
        row{
            "strstr(\"X\")",
            "Expected comma and 2nd parameter for \"strstr\""
        },
        row{
            "strstr(\"X\",",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"strstr\""; prec_prim, TOK_STRSTR
        },
        row{
            "strstr(\"X\", 1)",
            "Second parameter for \"strstr\" must be an string"
        },
        row{
            "strstr(\"X\", \"x\"",
            "Closing parenthesis expected for \"strstr\""
        },

        row{
            "strchr",
            "Opening parenthesis expected after \"strchr\""
        },
        row{
            "strchr(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"strchr\""; prec_prim, TOK_STRCHR
        },
        row{
            "strchr(2",
            "First parameter for \"strchr\" must be a string"
        },
        row{
            "strchr(\"X\")",
            "Expected comma and 2nd parameter for \"strchr\""
        },
        row{
            "strchr(\"X\",",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"strchr\""; prec_prim, TOK_STRCHR
        },
        row{
            "strchr(\"X\", \"X\")",
            "Second parameter for \"strchr\" must be an integer"
        },
        row{
            "strchr(\"X\", 'x'",
            "Closing parenthesis expected for \"strchr\""
        },

        row{
            "rtrim",
            "Opening parenthesis expected after \"rtrim\""
        },
        row{
            "rtrim(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"rtrim\""; prec_prim, TOK_RTRIM
        },
        row{
            "rtrim(2)",
            "Parameter for \"rtrim\" must be a string"
        },
        row{
            "rtrim(\"X\"",
            "Closing parenthesis expected for \"rtrim\""
        },

        row{
            "ltrim",
            "Opening parenthesis expected after \"ltrim\""
        },
        row{
            "ltrim(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"ltrim\""; prec_prim, TOK_LTRIM
        },
        row{
            "ltrim(2)",
            "Parameter for \"ltrim\" must be a string"
        },
        row{
            "ltrim(\"X\"",
            "Closing parenthesis expected for \"ltrim\""
        },

        row{
            "mid",
            "Opening parenthesis expected after \"mid\""
        },
        row{
            "mid(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"mid\""; prec_prim, TOK_MID
        },
        row{
            "mid(2",
            "First parameter for \"mid\" must be a string"
        },
        row{
            "mid(\"X\")",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"mid\""; prec_prim, TOK_MID
        },
        row{
            "mid(\"X\",",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"mid\""; prec_prim, TOK_MID
        },
        row{
            "mid(\"X\", \"X\")",
            "Second parameter for \"mid\" must be an integer"
        },
        row{
            "mid(\"X\", 0,",
            "Not implemented"
            // Should be "Expected 3rd parameter to \"mid\""; prec_prim, TOK_MID
        },
        row{
            "mid(\"X\", 0, \"X\")",
            "Third parameter for \"mid\" must be an integer"
        },
        row{
            "mid(\"X\", 0, 1",
            "Closing parenthesis expected for \"mid\""
        },
        row{
            "mid(\"X\", 0",
            "Closing parenthesis/3rd parameter expected for \"mid\""
        },

        row{
            "right",
            "Opening parenthesis expected after \"right\""
        },
        row{
            "right(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"right\""; prec_prim, TOK_RIGHT
        },
        row{
            "right(2",
            "First parameter for \"right\" must be a string"
        },
        row{
            "right(\"X\")",
            "Expected comma and 2nd parameter for \"right\""
        },
        row{
            "right(\"X\",",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"right\""; prec_prim, TOK_RIGHT
        },
        row{
            "right(\"X\", \"X\")",
            "Second parameter for \"right\" must be an integer"
        },
        row{
            "right(\"X\", 1",
            "Closing parenthesis expected for \"right\""
        },

        row{
            "left",
            "Opening parenthesis expected after \"left\""
        },
        row{
            "left(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"left\""; prec_prim, TOK_LEFT
        },
        row{
            "left(2",
            "First parameter for \"left\" must be a string"
        },
        row{
            "left(\"X\")",
            "Expected comma and 2nd parameter for \"left\""
        },
        row{
            "left(\"X\",",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"left\""; prec_prim, TOK_LEFT
        },
        row{
            "left(\"X\", \"X\")",
            "Second parameter for \"left\" must be an integer"
        },
        row{
            "left(\"X\", 1",
            "Closing parenthesis expected for \"left\""
        },

        row{
            "strlen",
            "Opening parenthesis expected after \"strlen\""
        },
        row{
            "strlen(",
            "Not implemented"
            // Should be "Expected string parameter to \"strlen\""; prec_prim, TOK_STRLEN
        },
        row{
            "strlen(2)",
            "Parameter for \"strlen\" must be a string"
        },
        row{
            "strlen(\"X\"",
            "Closing parenthesis expected for \"strlen\""
        },

        row{
            "rand",
            "Opening parenthesis expected after \"rand\""
        },
        row{
            "rand(",
            "Closing parenthesis expected for \"rand\""
        },

        row{
            "atof",
            "Opening parenthesis expected after \"atof\""
        },
        row{
            "atof(",
            "Not implemented"
            // Should be "Expected parameter for \"atof\""; prec_prim, TOK_ATOF
        },
        row{
            "atof(2)",
            "Parameter for \"atof\" must be a string"
        },
        row{
            "atof(\"X\"",
            "Closing parenthesis expected for \"atof\""
        },

        row{
            "atoi",
            "Opening parenthesis expected after \"atoi\""
        },
        row{
            "atoi(",
            "Not implemented"
            // Should be "Expected parameter for \"atoi\""; prec_prim, TOK_ATOI
        },
        row{
            "atoi(2)",
            "Parameter for \"atoi\" must be a string"
        },
        row{
            "atoi(\"X\"",
            "Closing parenthesis expected for \"atoi\""
        },

        row{
            "second",
            "Opening parenthesis expected after \"second\""
        },
        row{
            "second(",
            "Not implemented"
            // Should be "Expected parameter for \"second\""; prec_prim, TOK_SECOND
        },
        row{
            "second(\"X\")",
            "Parameter for \"second\" must be a number"
        },
        row{
            "second(2",
            "Closing parenthesis expected for \"second\""
        },

        row{
            "minute",
            "Opening parenthesis expected after \"minute\""
        },
        row{
            "minute(",
            "Not implemented"
            // Should be "Expected parameter for \"minute\""; prec_prim, TOK_MINUTE
        },
        row{
            "minute(\"X\")",
            "Parameter for \"minute\" must be a number"
        },
        row{
            "minute(2",
            "Closing parenthesis expected for \"minute\""
        },

        row{
            "hour",
            "Opening parenthesis expected after \"hour\""
        },
        row{
            "hour(",
            "Not implemented"
            // Should be "Expected parameter for \"hour\""; prec_prim, TOK_HOUR
        },
        row{
            "hour(\"X\")",
            "Parameter for \"hour\" must be a number"
        },
        row{
            "hour(2",
            "Closing parenthesis expected for \"hour\""
        },

        row{
            "day",
            "Opening parenthesis expected after \"day\""
        },
        row{
            "day(",
            "Not implemented"
            // Should be "Expected parameter for \"day\""; prec_prim, TOK_DAY
        },
        row{
            "day(\"X\")",
            "Parameter for \"day\" must be a number"
        },
        row{
            "day(2",
            "Closing parenthesis expected for \"day\""
        },

        row{
            "month",
            "Opening parenthesis expected after \"month\""
        },
        row{
            "month(",
            "Not implemented"
            // Should be "Expected parameter for \"month\""; prec_prim, TOK_MONTH
        },
        row{
            "month(\"X\")",
            "Parameter for \"month\" must be a number"
        },
        row{
            "month(2",
            "Closing parenthesis expected for \"month\""
        },

        row{
            "year",
            "Opening parenthesis expected after \"year\""
        },
        row{
            "year(",
            "Not implemented"
            // Should be "Expected parameter for \"year\""; prec_prim, TOK_YEAR
        },
        row{
            "year(\"X\")",
            "Parameter for \"year\" must be a number"
        },
        row{
            "year(2",
            "Closing parenthesis expected for \"year\""
        },

        row{
            "now",
            "Opening parenthesis expected after \"now\""
        },
        row{
            "now(",
            "Closing parenthesis expected for \"now\""
        },

        row{
            "time",
            "Opening parenthesis expected after \"time\""
        },
        row{
            "time(",
            "Not implemented"
            // Should be "Expected parameter for \"time\""; prec_prim, TOK_TIME
        },
        row{
            "time(2)",
            "Parameter for \"time\" must be a time string"
        },
        row{
            "time(\"X\"",
            "Closing parenthesis expected for \"time\""
        },

        row{
            "date",
            "Opening parenthesis expected after \"date\""
        },
        row{
            "date(",
            "Not implemented"
            // Should be "Expected parameter for \"date\""; prec_prim, TOK_DATE
        },
        row{
            "date(2)",
            "Parameter for \"date\" must be a date string"
        },
        row{
            "date(\"X\"",
            "Closing parenthesis expected for \"date\""
        },

        row{
            "int",
            "Opening parenthesis expected after \"int\""
        },
        row{
            "int(",
            "Not implemented"
            // Should be "Expected parameter for \"int\""; prec_prim, TOK_INT
        },
        row{
            "int(\"X\")",
            "Parameter for \"int\" must be numeric or boolean"
        },
        row{
            "int(0",
            "Closing parenthesis expected for \"int\""
        },

        row{
            "log",
            "Opening parenthesis expected after \"log\""
        },
        row{
            "log(",
            "Not implemented"
            // Should be "Expected parameter for \"log\""; prec_prim, TOK_LOG
        },
        row{
            "log(\"X\")",
            "Parameter for \"log\" must be numeric"
        },
        row{
            "log(0",
            "Closing parenthesis expected for \"log\""
        },

        row{
            "exp",
            "Opening parenthesis expected after \"exp\""
        },
        row{
            "exp(",
            "Not implemented"
            // Should be "Expected parameter for \"exp\""; prec_prim, TOK_EXP
        },
        row{
            "exp(\"X\")",
            "Parameter for \"exp\" must be numeric"
        },
        row{
            "exp(0",
            "Closing parenthesis expected for \"exp\""
        },

        row{
            "atan",
            "Opening parenthesis expected after \"atan\""
        },
        row{
            "atan(",
            "Not implemented"
            // Should be "Expected parameter for \"atan\""; prec_prim, TOK_ATAN
        },
        row{
            "atan(\"X\")",
            "Parameter for \"atan\" must be numeric"
        },
        row{
            "atan(0",
            "Closing parenthesis expected for \"atan\""
        },

        row{
            "acos",
            "Opening parenthesis expected after \"acos\""
        },
        row{
            "acos(",
            "Not implemented"
            // Should be "Expected parameter for \"acos\""; prec_prim, TOK_ACOS
        },
        row{
            "acos(\"X\")",
            "Parameter for \"acos\" must be numeric"
        },
        row{
            "acos(0",
            "Closing parenthesis expected for \"acos\""
        },

        row{
            "asin",
            "Opening parenthesis expected after \"asin\""
        },
        row{
            "asin(",
            "Not implemented"
            // Should be "Expected parameter for \"asin\""; prec_prim, TOK_ASIN
        },
        row{
            "asin(\"X\")",
            "Parameter for \"asin\" must be numeric"
        },
        row{
            "asin(0",
            "Closing parenthesis expected for \"asin\""
        },

        row{
            "tan",
            "Opening parenthesis expected after \"tan\""
        },
        row{
            "tan(",
            "Not implemented"
            // Should be "Expected parameter for \"tan\""; prec_prim, TOK_TAN
        },
        row{
            "tan(\"X\")",
            "Parameter for \"tan\" must be numeric"
        },
        row{
            "tan(0",
            "Closing parenthesis expected for \"tan\""
        },

        row{
            "cos",
            "Opening parenthesis expected after \"cos\""
        },
        row{
            "cos(",
            "Not implemented"
            // Should be "Expected parameter for \"cos\""; prec_prim, TOK_COS
        },
        row{
            "cos(\"X\")",
            "Parameter for \"cos\" must be numeric"
        },
        row{
            "cos(0",
            "Closing parenthesis expected for \"cos\""
        },

        row{
            "sin",
            "Opening parenthesis expected after \"sin\""
        },
        row{
            "sin(",
            "Not implemented"
            // Should be "Expected parameter for \"sin\""; prec_prim, TOK_SIN
        },
        row{
            "sin(\"X\")",
            "Parameter for \"sin\" must be numeric"
        },
        row{
            "sin(0",
            "Closing parenthesis expected for \"sin\""
        },

        row{
            "sqrt",
            "Opening parenthesis expected after \"sqrt\""
        },
        row{
            "sqrt(",
            "Not implemented"
            // Should be "Expected parameter for \"sqrt\""; prec_prim, TOK_SQRT
        },
        row{
            "sqrt(\"X\")",
            "Parameter for \"sqrt\" must be numeric"
        },
        row{
            "sqrt(0",
            "Closing parenthesis expected for \"sqrt\""
        },
        row{
            "sqrt(-1.0)",
            "Parameter for \"sqrt\" must not be negative"
        },
        row{
            "sqrt(-1)",
            "Parameter for \"sqrt\" must not be negative"
        },

        row{
            "pow",
            "Opening parenthesis expected after \"pow\""
        },
        row{
            "pow(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"pow\""; prec_prim, TOK_POW
        },
        row{
            "pow(1",
            "Expected comma and 2nd parameter for \"pow\""
        },
        row{
            "pow(1,",
            "Not implemented"
            // Should be "Expected comma and 2nd parameter for \"pow\""; prec_prim, TOK_POW
        },
        row{
            "pow(1, 0",
            "Closing parenthesis expected for \"pow\""
        },
        row{
            "pow(\"X\", 0)",
            "Parameter for \"pow\" must be numeric"
        },
        row{
            "pow(1, \"X\")",
            "Parameter for \"pow\" must be numeric"
        },
        row{
            "pow(-1, 0.5)",
            "Invalid parameters to pow"
        },

        row{
            "asr",
            "Opening parenthesis expected after \"asr\""
        },
        row{
            "asr(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"asr\""; prec_prim, TOK_ASR
        },
        row{
            "asr(\"X\"",
            "1st parameter for \"asr\" must be an integer"
        },
        row{
            "asr(0,",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"asr\""; prec_prim, TOK_ASR
        },
        row{
            "asr(0, \"X\")",
            "2nd parameter for \"asr\" must be an integer"
        },
        row{
            "asr(0, 0,",
            "Not implemented"
            // Should be "Expected 3rd parameter to \"asr\""; prec_prim, TOK_ASR
        },
        row{
            "asr(0, 0, \"X\")",
            "3rd parameter for \"asr\" must be an integer"
        },
        row{
            "asr(0, 0, 1",
            "Closing parenthesis expected for \"asr\""
        },
        row{
            "asr(0, 0",
            "Closing parenthesis expected for \"asr\""
        },

        row{
            "ror",
            "Opening parenthesis expected after \"ror\""
        },
        row{
            "ror(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"ror\""; prec_prim, TOK_ROR
        },
        row{
            "ror(\"X\"",
            "1st parameter for \"ror\" must be an integer"
        },
        row{
            "ror(0,",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"ror\""; prec_prim, TOK_ROR
        },
        row{
            "ror(0, \"X\")",
            "2nd parameter for \"ror\" must be an integer"
        },
        row{
            "ror(0, 0,",
            "Not implemented"
            // Should be "Expected 3rd parameter to \"ror\""; prec_prim, TOK_ROR
        },
        row{
            "ror(0, 0, \"X\")",
            "3rd parameter for \"ror\" must be an integer"
        },
        row{
            "ror(0, 0, 1",
            "Closing parenthesis expected for \"ror\""
        },
        row{
            "ror(0, 0",
            "Closing parenthesis expected for \"ror\""
        },

        row{
            "rol",
            "Opening parenthesis expected after \"rol\""
        },
        row{
            "rol(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"rol\""; prec_prim, TOK_ROL
        },
        row{
            "rol(\"X\"",
            "1st parameter for \"rol\" must be an integer"
        },
        row{
            "rol(0,",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"rol\""; prec_prim, TOK_ROL
        },
        row{
            "rol(0, \"X\")",
            "2nd parameter for \"rol\" must be an integer"
        },
        row{
            "rol(0, 0,",
            "Not implemented"
            // Should be "Expected 3rd parameter to \"rol\""; prec_prim, TOK_ROL
        },
        row{
            "rol(0, 0, \"X\")",
            "3rd parameter for \"rol\" must be an integer"
        },
        row{
            "rol(0, 0, 1",
            "Closing parenthesis expected for \"rol\""
        },
        row{
            "rol(0, 0",
            "Closing parenthesis expected for \"rol\""
        },

        row{
            "reverse",
            "Opening parenthesis expected after \"reverse\""
        },
        row{
            "reverse(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"reverse\""; prec_prim, TOK_REVERSE
        },
        row{
            "reverse(\"X\"",
            "First parameter for \"reverse\" must be an integer"
        },
        row{
            "reverse(0,",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"reverse\""; prec_prim, TOK_REVERSE
        },
        row{
            "reverse(0, \"X\")",
            "Bits (2nd) parameter for \"reverse\" must be an integer"
        },
        row{
            "reverse(0, 0",
            "Closing parenthesis expected for \"reverse\""
        },

        row{
            "flip",
            "Opening parenthesis expected after \"flip\""
        },
        row{
            "flip(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"flip\""; prec_prim, TOK_FLIP
        },
        row{
            "flip(1",
            "Expected comma and 2nd parameter for \"flip\""
        },
        row{
            "flip(1,",
            "Not implemented"
            // Should be "Expected 2nd parameter to \"flip\""; prec_prim, TOK_FLIP
        },
        row{
            "flip(1, 0",
            "Closing parenthesis expected for \"flip\""
        },
        row{
            "flip(\"X\", 0)",
            "Parameters for \"flip\" must be integers"
        },
        row{
            "flip(1, \"X\")",
            "Parameters for \"flip\" must be integers"
        },

        row{
            "fact",
            "Opening parenthesis expected after \"fact\""
        },
        row{
            "fact(",
            "Not implemented"
            // Should be "Expected parameter for \"fact\""; prec_prim, TOK_FACT
        },
        row{
            "fact(0",
            "Closing parenthesis expected for \"fact\""
        },
        row{
            "fact(100)",
            "Parameter for \"fact\" is too big"
        },
        row{
            "fact(1.0)",
            "Parameter for \"fact\" must be an integer"
        },

        row{
            "max",
            "Opening parenthesis expected after \"max\""
        },
        row{
            "max(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"max\""; prec_prim, TOK_MAX
        },
        row{
            "max(0",
            "Closing parenthesis expected for \"max\""
        },
        row{
            "max(\"X\")",
            "Parameter for \"max\" must be numeric"
        },
        row{
            "max(0,",
            "Not implemented"
            // Should be "Expected parameter for \"max\""; prec_prim, TOK_MAX
        },
        row{
            "max(0, \"X\")",
            "Parameter for \"max\" must be numeric"
        },

        row{
            "min",
            "Opening parenthesis expected after \"min\""
        },
        row{
            "min(",
            "Not implemented"
            // Should be "Expected 1st parameter to \"min\""; prec_prim, TOK_MIN
        },
        row{
            "min(0",
            "Closing parenthesis expected for \"min\""
        },
        row{
            "min(\"X\")",
            "Parameter for \"min\" must be numeric"
        },
        row{
            "min(0,",
            "Not implemented"
            // Should be "Expected parameter for \"min\""; prec_prim, TOK_MIN
        },
        row{
            "min(0, \"X\")",
            "Parameter for \"min\" must be numeric"
        },

        row{
            "abs",
            "Opening parenthesis expected after \"abs\""
        },
        row{
            "abs(",
            "Not implemented"
            // Should be "Expected parameter for \"abs\""; prec_prim, TOK_ABS
        },
        row{
            "abs(0",
            "Closing parenthesis expected for \"abs\""
        },
        row{
            "abs(\"X\")",
            "Parameter for \"abs\" must be numeric"
        },

        row{
            "addressof",
            "Opening parenthesis expected after \"addressof\""
        },
        row{
            "addressof(",
            "Symbol expected"
            // should be "Expected symbol name"; prec_prim, case TOK_ADDRESSOF
            // This at least makes sense, so maybe just get rid of the `error` call here?
        },
        row{
            "addressof(1e+",
            "Invalid number: 1e+"
            // "Expected symbol name"
        },
        row{
            "addressof(intField",
            "Closing parenthesis expected for \"addressof\""
        },
        row{
            "addressof(\"X\")",
            "Symbol expected"
        },
        row{
            "addressof(intField.x)",
            "Dot operator (.) must only be used with struct"
        },
        row{
            "addressof(structField.",
            "Symbol expected"
            // should be "Expected member name"; prec_prim, case TOK_ADDRESSOF
            // This at least makes sense, so maybe just get rid of the `error` call here?
        },
        row{
            "addressof(structField.1e+",
            "Invalid number: 1e+"
            // "Expected member name"
        },
        row{
            "addressof(structField.does_not_exist)",
            "Unrecognised member name for struct"
        },
        row{
            "addressof(intField[0])",
            "Array index on non-array symbol"
        },
        row{
            "addressof(arrayField[",
            "Not implemented"
            // Should be "Expected array index"; prec_prim, TOK_ADDRESSOF
        },
        row{
            "addressof(arrayField[0",
            "Closing bracket (]) expected"
        },
        row{
            "addressof(arrayField[\"X\"])",
            "Array (for) index must be an integer"
        },
        row{
            "addressof(arrayField[9999])",
            "Invalid array (for) index"
        },
        row{
            "addressof(does_not_exist)",
             "Symbol \"does_not_exist\" not in file"
        },

        row{
            "string",
            "Opening parenthesis expected after \"string\""
        },
        row{
            "string(",
            "Symbol expected"
            // should be "Expected member name"; prec_prim, case TOK_SIZEOF
            // This at least makes sense, so maybe just get rid of the `error` call here?
        },
        row{
            "string(0",
            "Symbol expected"
        },
        row{
            "string(1e+",
            "Invalid number: 1e+"
            // "Expected symbol name"
        },
        row{
            "string(intField",
            "Closing parenthesis expected for \"string\""
        },
        row{
            "string(does_not_exist)",
            "Unknown symbol \"does_not_exist\""
        },
        row{
            "string(intField.value)",
            "Dot operator (.) must only be used with struct"
        },
        row{
            "string(structField.",
            "Symbol expected"
            // should be "Expected member name"; prec_prim, case TOK_SIZEOF
            // This at least makes sense, so maybe just get rid of the `error` call here?
        },
        row{
            "string(structField.1e+",
            "Invalid number: 1e+"
            // "Expected member name"
        },
        row{
            "string(structField.0)",
            "Symbol expected"
        },
        row{
            "string(structField.does_not_exist)",
            "Unrecognised member name for struct"
        },
        row{
            "string(intField[0])",
            "Array index on non-array symbol"
        },
        row{
            "string(arrayField[",
            "Not implemented"
            // Should be "Expected array index"; prec_prim, TOK_SIZEOF
        },
        row{
            "string(arrayField[0",
            "Closing bracket (]) expected"
        },
        row{
            "string(arrayField[\"X\"])",
            "Array (for) index must be an integer"
        },
        row{
            "string(arrayField[9999])",
            "Invalid array (for) index"
        },

        row{
            "defined(",
            "Symbol expected"
            // Should be "Expected identifier after \"defined\""; prec_prim, TOK_DEFINED
        },
        row{
            "defined",
            "Symbol expected"
            // Should be "Expected identifier after \"defined\""; prec_prim, TOK_DEFINED
        },
        row{
            "defined(0)",
            "Symbol expected"
        },
        row{
            "defined(1e+",
            "Invalid number: 1e+"
            // "Expected identifier after \"defined\""
        },
        row{
            "defined(intField",
            "Closing parenthesis expected for \"defined\""
        },

        row{
            "sizeof",
            "Opening parenthesis expected after \"sizeof\""
        },
        row{
            "sizeof(",
            "Symbol expected"
            // should be "Expected member name"; prec_prim, case TOK_SIZEOF
            // This at least makes sense, so maybe just get rid of the `error` call here?
        },
        row{
            "sizeof(0",
            "Symbol expected"
        },
        row{
            "sizeof(1e+",
            "Invalid number: 1e+"
            // "Expected symbol name"
        },
        row{
            "sizeof(intField",
            "Closing parenthesis expected for \"sizeof\""
        },
        row{
            "sizeof(does_not_exist)",
            "Unknown symbol \"does_not_exist\""
        },
        row{
            "sizeof(intField.value)",
            "Dot operator (.) must only be used with struct"
        },
        row{
            "sizeof(structField.",
            "Symbol expected"
            // should be "Expected member name"; prec_prim, case TOK_SIZEOF
            // This at least makes sense, so maybe just get rid of the `error` call here?
        },
        row{
            "sizeof(structField.1e+",
            "Invalid number: 1e+"
            // "Expected member name"
        },
        row{
            "sizeof(structField.0)",
            "Symbol expected"
        },
        row{
            "sizeof(structField.does_not_exist)",
            "Unrecognised member name for struct"
        },
        row{
            "sizeof(intField[0])",
            "Array index on non-array symbol"
        },
        row{
            "sizeof(arrayField[",
            "Not implemented"
            // Should be "Expected array index"; prec_prim, TOK_SIZEOF
        },
        row{
            "sizeof(arrayField[0",
            "Closing bracket (]) expected"
        },
        row{
            "sizeof(arrayField[\"X\"])",
            "Array (for) index must be an integer"
        },
        row{
            "sizeof(arrayField[9999])",
            "Invalid array (for) index"
        },

        row{
            "2 * \"x\"",
            "Illegal operand types for multiplication"
        },
        row{
            "2 *",
            "Not implemented"
            // should be "Expected 2nd operand for multiplication"; prec_mul
        },
        row{
            "2 / \"x\"",
            "Illegal operand types for division"
        },
        row{
            "2 /",
            "Not implemented"
            // should be "Expected 2nd operand for division"; prec_mul
        },
        row{
            "2 / 0",
            "Integer divide by zero"
        },
        row{
            "2.0 / 0",
            "Divide by zero"
        },
        row{
            "2 %",
            "Not implemented"
            // should be "Expected 2nd operand of modulus"; prec_mul
        },
        row{
            "2.0 % 1",
            "Modulus operands must be integers"
        },
        row{
            "2 % 0",
            "Divide by zero (modulus operation)"
        },

        row{
            "TRUE + FALSE",
            "Illegal operand types for addition"
        },
        row{
            "TRUE +",
            "Not implemented"
            // should be "Expected 2nd operand for addition"; prec_add
        },
        row{
            "TRUE - FALSE",
            "Illegal operand types for subtraction"
        },
        row{
            "1 -",
            "Not implemented"
            // should be "Expected 2nd operand of subtraction"; prec_add
        },

        row{
            "2.0 << 1",
            "Left shift operands must be integers"
        },
        row{
            "2 <<",
            "Not implemented"
            // should be "Expected 2nd operand of shift left"; prec_shift
        },
        row{
            "2.0 >> 1",
            "Right shift operands must be integers"
        },
        row{
            "2 >>",
            "Not implemented"
            // should be "Expected 2nd operand of shift right"; prec_shift
        },

        row{
            "2.0 & 1",
            "Bitwise AND requires integer operands"
        },
        row{
            "2.0 &",
            "Not implemented"
            // should be "Expected 2nd operand of bitwise AND (&)"; prec_bitand
        },

        row{
            "2.0 ^ 1",
            "Exclusive OR (^) requires integer operands"
        },
        row{
            "2 ^",
            "Not implemented"
            // should be "Expected 2nd operand of exclusive OR (^)"; prec_xor
        },

        row{
            "2.0 | 1",
            "Bitwise OR requires integer operands"
        },
        row{
            "2 |",
            "Not implemented"
            // should be "Expected 2nd operand of bitwise OR (|)"; prec_bitor
        },

        row{
            "\"X\" == FALSE",
            "Illegal operand types for comparison"
        },
        row{
            "\"X\" ==",
            "Not implemented"
            // should be "Expected 2nd operand for comparison"; prec_bitor
        },

        row{
            "1 && 2",
            "Operands for AND (&&) must be BOOLEAN"
        },
        row{
            "1 &&",
            "Not implemented"
            // should be "Expected 2nd operand for AND (&&)"; prec_and
        },


        row{
            "1 || 2",
            "Operands for OR (||) must be BOOLEAN"
        },
        row{
            "1 ||",
            "Not implemented"
            // should be "Expected 2nd operand for OR (||)"; prec_or
        },

        row{
            "1 ? 2 : 3",
            "Condition for ternary operator must be BOOLEAN"
        },
        row{
            "TRUE ? () : 3",
            "Not implemented"
            // should be "Expected \"if\" expression for conditional (?:) operator"; prec_ternary
        },
        row{
            "TRUE ? 2",
            "Expected colon (:) for conditional (?:) operator"
        },
        row{
            "FALSE ? 2 : ()",
            "Not implemented"
            // should be "Expected \"else\" expression for conditional (?:) operator"; prec_ternary
        },

        row{
            "1 = 1",
            "Assignment requires a variable name on the left of the equals sign"
        },
        row{
            "VAR =",
            "Not implemented"
            // should be "Expected expression for assignment"; prec_assign
        },
        row{
            "1 += 1",
            "Variable name required on the left of +="
        },
        row{
            "VAR +=",
            "Not implemented"
            // should be "Expected expression for +="; prec_assign
        },
        row{
            "INT_VAR += \"X\"",
            "Incompatible types for +="
        },

        row{
            "1,",
            "Not implemented"
            // should be "Expected another expression after comma"; prec_assign
        }
    );

    // TODO: refactor app code so we don't need this global state.
    theApp.dec_point_ = '.';
    theApp.dec_sep_char_ = ',';

    test_expr expr;

    expr.set_variable("INT_VAR", value_t{ 123 });
    expr.set_variable("BOOL_VAR", value_t{ true });
    expr.set_variable("REAL_VAR", value_t{ 123.456 });
    expr.set_variable("STRING_VAR", value_t{ "string_var" });
    expr.set_variable("DATE_VAR", value_t{ COleDateTime{ 2021, 11, 10, 20, 29, 05 } });
    expr.set_variable("ARR_VAR[0]", value_t{ "TEST" });

    int ref_ac = 0;
    value_t result = expr.evaluate(test.expression, 0, ref_ac);

    CString errorMsg = expr.get_error_message();

    CAPTURE(test.expression, test.expected, result, errorMsg);

    CHECK(result.error == false);
    REQUIRE(errorMsg == test.expected);
    REQUIRE(result.typ == expr_eval::TYPE_NONE);
}

TEST_CASE("expr_eval::evaluate - non-decimal integer overflow")
{
    test_expr expr{ 16 };

    int ref_ac = 0;
    value_t result = expr.evaluate("10000000000000000", 0, ref_ac, 16);

    CString errorMsg = expr.get_error_message();

    CAPTURE(result, errorMsg);

    // check the result value
    CHECK(result.typ == expr_eval::TYPE_NONE);
    CHECK(errorMsg == "Overflow: \"10000000000000000\" too big");
}


namespace Catch
{
    template<>
    struct StringMaker<CStringW>
    {
        static std::string convert(const CStringW& value)
        {
            CString cstr{ value };
            return std::string{ cstr };
        }
    };

    template<>
    struct StringMaker<value_t>
    {
        static std::string convert(const value_t& value)
        {
            static const char* type_names[] =
            {
                "NONE",
                "BOOLEAN",
                "INT",
                "DATE",
                "REAL",
                "STRING",
                "BLOB",
                "STRUCT",
                "ARRAY",
            };

            const char* type = ((unsigned)value.typ) < (sizeof(type_names) / sizeof(*type_names))
                ? type_names[value.typ]
                : "UNKNOWN";

            std::string data;

            switch (value.typ)
            {
            case expr_eval::TYPE_NONE:
                break;

            case expr_eval::TYPE_BOOLEAN:
                data = value.boolean ? "true" : "false";
                break;

            case expr_eval::TYPE_INT:
                data = std::to_string(value.int64);
                break;

            case expr_eval::TYPE_DATE:
                data = value.date > InvalidDate
                    ? COleDateTime{ value.date }.Format("%Y-%m-%d %H-%M-%S")
                    : "<invalid date>";
                break;

            case expr_eval::TYPE_REAL:
                data = std::to_string(value.real64);
                break;

            case expr_eval::TYPE_STRING:
                data = value.pstr ? CString{ *value.pstr } : "(null)";
                break;

            default:
                break;
            }

            std::string as_string;

            if (data.length() > 0)
            {
                as_string = type + std::string{ " : " } + data;
            }
            else
            {
                as_string = type;
            }

            return as_string;
        }
    };
}