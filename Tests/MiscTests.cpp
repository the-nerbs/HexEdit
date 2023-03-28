#include "Stdafx.h"
#include "Misc.h"

#include <gsl/util>

#include <catch.hpp>

#include <algorithm>
#include <clocale>
#include <cmath>


struct color_row
{
    COLORREF bgr;
    int hue;
    int lightness;
    int saturation;
    const char* name;   // for diagnostics
};

// note: Hue is a percentage here, not the typical degrees!
// This table was generated from this C# snippet:
//  foreach (var prop in typeof(Color).GetProperties(BindingFlags.Public | BindingFlags.Static)) {
//      var c = (Color)prop.GetValue(null);
//  
//      if (c.A == 255)
//      {
//          int bgr = (c.B << 16) | (c.G << 8) | (c.R << 0);
//          int hue = c.R == c.G && c.G == c.B ? -1 : (int)(100 * c.GetHue() / 360.0);
//          int lum = (int)Math.Round(c.GetBrightness() * 100);
//          int sat = (int)Math.Round(c.GetSaturation() * 100);
//  
//          WriteLine($"{{ 0x{bgr:X8}, {hue,3}, {lum,3}, {sat,3}, \"{c.Name}\" }},");
//      }
//  }
static constexpr color_row color_testrows[] = {
    //    BBGGRR   H%   L%   S%
    { 0x00FFF8F0,  57,  97, 100, "AliceBlue" },
    { 0x00D7EBFA,   9,  91,  78, "AntiqueWhite" },
    { 0x00FFFF00,  50,  50, 100, "Aqua" },
    { 0x00D4FF7F,  44,  75, 100, "Aquamarine" },
    { 0x00FFFFF0,  50,  97, 100, "Azure" },
    { 0x00DCF5F5,  16,  91,  56, "Beige" },
    { 0x00C4E4FF,   9,  88, 100, "Bisque" },
    { 0x00000000,  -1,   0,   0, "Black" },
    { 0x00CDEBFF,  10,  90, 100, "BlanchedAlmond" },
    { 0x00FF0000,  66,  50, 100, "Blue" },
    { 0x00E22B8A,  75,  53,  76, "BlueViolet" },
    { 0x002A2AA5,   0,  41,  59, "Brown" },
    { 0x0087B8DE,   9,  70,  57, "BurlyWood" },
    { 0x00A09E5F,  50,  50,  25, "CadetBlue" },
    { 0x0000FF7F,  25,  50, 100, "Chartreuse" },
    { 0x001E69D2,   6,  47,  75, "Chocolate" },
    { 0x00507FFF,   4,  66, 100, "Coral" },
    { 0x00ED9564,  60,  66,  79, "CornflowerBlue" },
    { 0x00DCF8FF,  13,  93, 100, "Cornsilk" },
    { 0x003C14DC,  96,  47,  83, "Crimson" },
    { 0x00FFFF00,  50,  50, 100, "Cyan" },
    { 0x008B0000,  66,  27, 100, "DarkBlue" },
    { 0x008B8B00,  50,  27, 100, "DarkCyan" },
    { 0x000B86B8,  11,  38,  89, "DarkGoldenrod" },
    { 0x00A9A9A9,  -1,  66,   0, "DarkGray" },
    { 0x00006400,  33,  20, 100, "DarkGreen" },
    { 0x006BB7BD,  15,  58,  38, "DarkKhaki" },
    { 0x008B008B,  83,  27, 100, "DarkMagenta" },
    { 0x002F6B55,  22,  30,  39, "DarkOliveGreen" },
    { 0x00008CFF,   9,  50, 100, "DarkOrange" },
    { 0x00CC3299,  77,  50,  61, "DarkOrchid" },
    { 0x0000008B,   0,  27, 100, "DarkRed" },
    { 0x007A96E9,   4,  70,  72, "DarkSalmon" },
    { 0x008BBC8F,  31,  64,  27, "DarkSeaGreen" },
    { 0x008B3D48,  69,  39,  39, "DarkSlateBlue" },
    { 0x004F4F2F,  50,  25,  25, "DarkSlateGray" },
    { 0x00D1CE00,  50,  41, 100, "DarkTurquoise" },
    { 0x00D30094,  78,  41, 100, "DarkViolet" },
    { 0x009314FF,  90,  54, 100, "DeepPink" },
    { 0x00FFBF00,  54,  50, 100, "DeepSkyBlue" },
    { 0x00696969,  -1,  41,   0, "DimGray" },
    { 0x00FF901E,  58,  56, 100, "DodgerBlue" },
    { 0x002222B2,   0,  42,  68, "Firebrick" },
    { 0x00F0FAFF,  11,  97, 100, "FloralWhite" },
    { 0x00228B22,  33,  34,  61, "ForestGreen" },
    { 0x00FF00FF,  83,  50, 100, "Fuchsia" },
    { 0x00DCDCDC,  -1,  86,   0, "Gainsboro" },
    { 0x00FFF8F8,  66,  99, 100, "GhostWhite" },
    { 0x0000D7FF,  14,  50, 100, "Gold" },
    { 0x0020A5DA,  11,  49,  74, "Goldenrod" },
    { 0x00808080,  -1,  50,   0, "Gray" },
    { 0x00008000,  33,  25, 100, "Green" },
    { 0x002FFFAD,  23,  59, 100, "GreenYellow" },
    { 0x00F0FFF0,  33,  97, 100, "Honeydew" },
    { 0x00B469FF,  91,  71, 100, "HotPink" },
    { 0x005C5CCD,   0,  58,  53, "IndianRed" },
    { 0x0082004B,  76,  25, 100, "Indigo" },
    { 0x00F0FFFF,  16,  97, 100, "Ivory" },
    { 0x008CE6F0,  15,  75,  77, "Khaki" },
    { 0x00FAE6E6,  66,  94,  67, "Lavender" },
    { 0x00F5F0FF,  94,  97, 100, "LavenderBlush" },
    { 0x0000FC7C,  25,  49, 100, "LawnGreen" },
    { 0x00CDFAFF,  15,  90, 100, "LemonChiffon" },
    { 0x00E6D8AD,  54,  79,  53, "LightBlue" },
    { 0x008080F0,   0,  72,  79, "LightCoral" },
    { 0x00FFFFE0,  50,  94, 100, "LightCyan" },
    { 0x00D2FAFA,  16,  90,  80, "LightGoldenrodYellow" },
    { 0x0090EE90,  33,  75,  73, "LightGreen" },
    { 0x00D3D3D3,  -1,  83,   0, "LightGray" },
    { 0x00C1B6FF,  97,  86, 100, "LightPink" },
    { 0x007AA0FF,   4,  74, 100, "LightSalmon" },
    { 0x00AAB220,  49,  41,  70, "LightSeaGreen" },
    { 0x00FACE87,  56,  75,  92, "LightSkyBlue" },
    { 0x00998877,  58,  53,  14, "LightSlateGray" },
    { 0x00DEC4B0,  59,  78,  41, "LightSteelBlue" },
    { 0x00E0FFFF,  16,  94, 100, "LightYellow" },
    { 0x0000FF00,  33,  50, 100, "Lime" },
    { 0x0032CD32,  33,  50,  61, "LimeGreen" },
    { 0x00E6F0FA,   8,  94,  67, "Linen" },
    { 0x00FF00FF,  83,  50, 100, "Magenta" },
    { 0x00000080,   0,  25, 100, "Maroon" },
    { 0x00AACD66,  44,  60,  51, "MediumAquamarine" },
    { 0x00CD0000,  66,  40, 100, "MediumBlue" },
    { 0x00D355BA,  80,  58,  59, "MediumOrchid" },
    { 0x00DB7093,  72,  65,  60, "MediumPurple" },
    { 0x0071B33C,  40,  47,  50, "MediumSeaGreen" },
    { 0x00EE687B,  69,  67,  80, "MediumSlateBlue" },
    { 0x009AFA00,  43,  49, 100, "MediumSpringGreen" },
    { 0x00CCD148,  49,  55,  60, "MediumTurquoise" },
    { 0x008515C7,  89,  43,  81, "MediumVioletRed" },
    { 0x00701919,  66,  27,  64, "MidnightBlue" },
    { 0x00FAFFF5,  41,  98, 100, "MintCream" },
    { 0x00E1E4FF,   1,  94, 100, "MistyRose" },
    { 0x00B5E4FF,  10,  85, 100, "Moccasin" },
    { 0x00ADDEFF,   9,  84, 100, "NavajoWhite" },
    { 0x00800000,  66,  25, 100, "Navy" },
    { 0x00E6F5FD,  10,  95,  85, "OldLace" },
    { 0x00008080,  16,  25, 100, "Olive" },
    { 0x00238E6B,  22,  35,  60, "OliveDrab" },
    { 0x0000A5FF,  10,  50, 100, "Orange" },
    { 0x000045FF,   4,  50, 100, "OrangeRed" },
    { 0x00D670DA,  83,  65,  59, "Orchid" },
    { 0x00AAE8EE,  15,  80,  67, "PaleGoldenrod" },
    { 0x0098FB98,  33,  79,  93, "PaleGreen" },
    { 0x00EEEEAF,  50,  81,  65, "PaleTurquoise" },
    { 0x009370DB,  94,  65,  60, "PaleVioletRed" },
    { 0x00D5EFFF,  10,  92, 100, "PapayaWhip" },
    { 0x00B9DAFF,   7,  86, 100, "PeachPuff" },
    { 0x003F85CD,   8,  53,  59, "Peru" },
    { 0x00CBC0FF,  97,  88, 100, "Pink" },
    { 0x00DDA0DD,  83,  75,  47, "Plum" },
    { 0x00E6E0B0,  51,  80,  52, "PowderBlue" },
    { 0x00800080,  83,  25, 100, "Purple" },
    { 0x000000FF,   0,  50, 100, "Red" },
    { 0x008F8FBC,   0,  65,  25, "RosyBrown" },
    { 0x00E16941,  62,  57,  73, "RoyalBlue" },
    { 0x0013458B,   6,  31,  76, "SaddleBrown" },
    { 0x007280FA,   1,  71,  93, "Salmon" },
    { 0x0060A4F4,   7,  67,  87, "SandyBrown" },
    { 0x00578B2E,  40,  36,  50, "SeaGreen" },
    { 0x00EEF5FF,   6,  97, 100, "SeaShell" },
    { 0x002D52A0,   5,  40,  56, "Sienna" },
    { 0x00C0C0C0,  -1,  75,   0, "Silver" },
    { 0x00EBCE87,  54,  73,  71, "SkyBlue" },
    { 0x00CD5A6A,  68,  58,  53, "SlateBlue" },
    { 0x00908070,  58,  50,  13, "SlateGray" },
    { 0x00FAFAFF,   0,  99, 100, "Snow" },
    { 0x007FFF00,  41,  50, 100, "SpringGreen" },
    { 0x00B48246,  57,  49,  44, "SteelBlue" },
    { 0x008CB4D2,   9,  69,  44, "Tan" },
    { 0x00808000,  50,  25, 100, "Teal" },
    { 0x00D8BFD8,  83,  80,  24, "Thistle" },
    { 0x004763FF,   2,  64, 100, "Tomato" },
    { 0x00D0E040,  48,  56,  72, "Turquoise" },
    { 0x00EE82EE,  83,  72,  76, "Violet" },
    { 0x00B3DEF5,  10,  83,  77, "Wheat" },
    { 0x00FFFFFF,  -1, 100,   0, "White" },
    { 0x00F5F5F5,  -1,  96,   0, "WhiteSmoke" },
    { 0x0000FFFF,  16,  50, 100, "Yellow" },
    { 0x0032CD9A,  22,  50,  61, "YellowGreen" },
};

TEST_CASE("get_hls")
{
    const color_row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(color_testrows),
            std::end(color_testrows)
        )
    );
    CAPTURE(test.name);

    int h = 0;
    int l = 0;
    int s = 0;
    get_hls(test.bgr, h, l, s);

    CAPTURE(h, test.hue);
    CAPTURE(l, test.lightness);
    CAPTURE(s, test.saturation);

    // Note: the existing RGB -> HSL conversion has some slop, so we don't 
    // get exactly the same hue values that .NET's System.Drawing.Color gave
    // for the test table above. The most-off is Orchid at ~2% (~7.2 deg),
    // but everything else is within 1% (3.6 degs).
    //
    // Ultimately, this slop doesn't matter since 1) even the best of human
    // eyes are bad at detecting exact colors, and 2) monitors all show colors
    // different anyway.
    constexpr int tolerance = 2;

    CHECK(std::abs(h - test.hue) <= tolerance);

    CHECK(l == test.lightness);
    CHECK(s == test.saturation);
}

TEST_CASE("get_rgb")
{
    const color_row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(color_testrows),
            std::end(color_testrows)
        )
    );
    CAPTURE(test.name);

    COLORREF bgr = get_rgb(test.hue, test.lightness, test.saturation);

    CAPTURE(bgr, test.bgr);

    // Note: the existing RGB -> HSL conversion has some slop, so we don't 
    // get exactly the same RGB values that .NET's System.Drawing.Color gave
    // for the test table above. The most-off are Gold (green channel) and
    // LightSeaGreen (blue channel) at 9 units off (~3.5%).
    //
    // Same as with the hls slop - this is too small a change to be noticed.
    constexpr int tolerance = 9;

    int expectedR = test.bgr & 0xFF;
    int expectedG = (test.bgr >> 8) & 0xFF;
    int expectedB = (test.bgr >> 16) & 0xFF;

    int actualR = bgr & 0xFF;
    int actualG = (bgr >> 8) & 0xFF;
    int actualB = (bgr >> 16) & 0xFF;

    CHECK(std::abs(expectedR - actualR) <= tolerance);
    CHECK(std::abs(expectedG - actualG) <= tolerance);
    CHECK(std::abs(expectedB - actualB) <= tolerance);
}

TEST_CASE("tone_down")
{
    struct row
    {
        COLORREF baseColor;
        COLORREF targetColor;
        double t;
        COLORREF expected;
    };

    static const row testrows[] = {
        //      base      target      t    expected
        //    BBGGRR      BBGGRR             BBGGRR
        { 0x00000000, 0x00000000,  0.00, 0x00000000 },  // no-op cases
        { 0x00000000, 0x00000000,  1.00, 0x00000000 },
        { 0x00FFFFFF, 0x00FFFFFF,  0.00, 0x00FFFFFF },
        { 0x00FFFFFF, 0x00FFFFFF,  1.00, 0x00FFFFFF },

        { 0x007f7f7f, 0x00000000,  1.00, 0x00000000 },  // --------------------
        { 0x007f7f7f, 0x00000000,  0.75, 0x001F1F1F },
        { 0x007f7f7f, 0x00000000,  0.50, 0x003F3F3F },
        { 0x007f7f7f, 0x00000000,  0.25, 0x005F5F5F },  
        { 0x007f7f7f, 0x00000000,  0.00, 0x007F7F7F },  // ^^^ tone down ^^^
        { 0x007f7f7f, 0x00000000, -0.00, 0x007F7F7F },  // vvv  tone up  vvv
        { 0x007f7f7f, 0x00000000, -0.25, 0x009F9F9F },
        { 0x007f7f7f, 0x00000000, -0.50, 0x00BFBFBF },
        { 0x007f7f7f, 0x00000000, -0.75, 0x00DFDFDF },
        { 0x007f7f7f, 0x00000000, -1.00, 0x00FFFFFF },  //  -------------------

        { 0x00000000, 0x00FFFFFF,  0.00, 0x00000000 },  // tone up (target bright)
        { 0x00000000, 0x00FFFFFF,  0.25, 0x003f3f3f },
        { 0x00000000, 0x00FFFFFF,  0.50, 0x007F7F7F },
        { 0x00000000, 0x00FFFFFF,  0.75, 0x00BFBFBF },
        { 0x00000000, 0x00FFFFFF,  1.00, 0x00FFFFFF },

        { 0x000000FF, 0x00C0C0C0,  0.75, 0x005c5cff },  // hues are preserved
        { 0x0000FF00, 0x00C0C0C0,  0.75, 0x005cff5c },
        { 0x00FF0000, 0x00C0C0C0,  0.75, 0x00ff5c5c },

        { 0x007f7f7f, 0x00000000,  1.50, 0x00000000 },  // results are clamped
        { 0x007f7f7f, 0x00000000, -1.50, 0x00FFFFFF },

        { 0x00BFBFBF, 0x00BFBFBF, -1.00, 0x00BCBCBC },  // tone up will change the color a bit,
        { 0x003F3F3F, 0x003F3F3F, -1.00, 0x00424242 },  // even if base and target match
    };


    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.baseColor, test.targetColor, test.t, test.expected);


    COLORREF actual = tone_down(test.baseColor, test.targetColor, test.t);
    CAPTURE(actual);

    // note: the HSL/RGB conversions have some slop,
    // so only compare to within a tolerance.
    constexpr int tolerance = 2;

    int expectedR = test.expected & 0xFF;
    int expectedG = (test.expected >> 8) & 0xFF;
    int expectedB = (test.expected >> 16) & 0xFF;

    int actualR = actual & 0xFF;
    int actualG = (actual >> 8) & 0xFF;
    int actualB = (actual >> 16) & 0xFF;

    CHECK(std::abs(expectedR - actualR) <= tolerance);
    CHECK(std::abs(expectedG - actualG) <= tolerance);
    CHECK(std::abs(expectedB - actualB) <= tolerance);
}

TEST_CASE("add_contrast")
{
    struct row
    {
        COLORREF baseColor;
        COLORREF backgroundColor;
        COLORREF expected;
    };

    static const row testrows[] = {
        //    BBGGRR      BBGGRR      BBGGRR
        { 0x00000000, 0x00000000, 0x003f3f3f },
        { 0x003f3f3f, 0x00000000, 0x003f3f3f }, // enough contrast already
        { 0x00000000, 0x003f3f3f, 0x00808080 }, // enough contract, but wrong direction

        { 0x00ffffff, 0x00ffffff, 0x00bfbfbf },
        { 0x00bfbfbf, 0x00000000, 0x00bfbfbf }, // enough contrast already
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.baseColor, test.backgroundColor, test.expected);


    COLORREF actual = add_contrast(test.baseColor, test.backgroundColor);
    CAPTURE(actual);

    // note: the HSL/RGB conversions have some slop,
    // so only compare to within a tolerance.
    constexpr int tolerance = 2;

    int expectedR = test.expected & 0xFF;
    int expectedG = (test.expected >> 8) & 0xFF;
    int expectedB = (test.expected >> 16) & 0xFF;

    int actualR = actual & 0xFF;
    int actualG = (actual >> 8) & 0xFF;
    int actualB = (actual >> 16) & 0xFF;

    CHECK(std::abs(expectedR - actualR) <= tolerance);
    CHECK(std::abs(expectedG - actualG) <= tolerance);
    CHECK(std::abs(expectedB - actualB) <= tolerance);
}

TEST_CASE("same_hue")
{
    struct row
    {
        COLORREF baseColor;
        int saturation;
        int luminance;
        COLORREF expected;
    };

    static const row testrows[] = {
        //    BBGGRR   S%   L%      BBGGRR
        { 0x00000000,   0,   0, 0x00000000 },
        { 0x00000000,   0, 100, 0x00ffffff },
        { 0x00ffffff,   0,   0, 0x00000000 },
        { 0x00ffffff,   0, 100, 0x00ffffff },

        // reds
        { 0x000000ff,   0,   0, 0x00000000 },
        { 0x000000ff,  25,  50, 0x0060609f },
        { 0x000000ff,  50,  50, 0x004040bf },
        { 0x000000ff,  75,  50, 0x002020df },
        { 0x000000ff, 100,  50, 0x000000ff },

        { 0x000000ff,   0,   0, 0x00000000 },
        { 0x000000ff, 100,  25, 0x00000080 },
      //{ 0x000000ff, 100,  50, 0x000000ff }, // dups another row
        { 0x000000ff, 100,  75, 0x008080ff },
        { 0x000000ff, 100, 100, 0x00ffffff },

        // greens
        { 0x0000ff00,   0,   0, 0x00000000 },
        { 0x0000ff00,  25,  50, 0x00609f60 },
        { 0x0000ff00,  50,  50, 0x0040bf40 },
        { 0x0000ff00,  75,  50, 0x0020df20 },
        { 0x0000ff00, 100,  50, 0x0000ff00 },

        { 0x0000ff00,   0,   0, 0x00000000 },
        { 0x0000ff00, 100,  25, 0x00008000 },
      //{ 0x0000ff00, 100,  50, 0x0000ff00 }, // dups another row
        { 0x0000ff00, 100,  75, 0x0080ff80 },
        { 0x0000ff00, 100, 100, 0x00ffffff },

        // blues
        { 0x00ff0000,   0,   0, 0x00000000 },
        { 0x00ff0000,  25,  50, 0x009f6060 },
        { 0x00ff0000,  50,  50, 0x00bf4040 },
        { 0x00ff0000,  75,  50, 0x00df2020 },
        { 0x00ff0000, 100,  50, 0x00ff0000 },

        { 0x00ff0000,   0,   0, 0x00000000 },
        { 0x00ff0000, 100,  25, 0x00800000 },
      //{ 0x00ff0000, 100,  50, 0x00ff0000 }, // dups another row
        { 0x00ff0000, 100,  75, 0x00ff8080 },
        { 0x00ff0000, 100, 100, 0x00ffffff },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.baseColor, test.saturation, test.luminance, test.expected);


    COLORREF actual = same_hue(test.baseColor, test.saturation, test.luminance);
    CAPTURE(actual);

    // note: the HSL/RGB conversions have some slop,
    // so only compare to within a tolerance.
    constexpr int tolerance = 2;

    int expectedR = test.expected & 0xFF;
    int expectedG = (test.expected >> 8) & 0xFF;
    int expectedB = (test.expected >> 16) & 0xFF;

    int actualR = actual & 0xFF;
    int actualG = (actual >> 8) & 0xFF;
    int actualB = (actual >> 16) & 0xFF;

    CHECK(std::abs(expectedR - actualR) <= tolerance);
    CHECK(std::abs(expectedG - actualG) <= tolerance);
    CHECK(std::abs(expectedB - actualB) <= tolerance);
}

TEST_CASE("opp_hue")
{
    struct row
    {
        COLORREF baseColor;
        COLORREF expected;
    };

    static const row testrows[] = {
        //      base    expected
        //    BBGGRR      BBGGRR
        { 0x00000000, 0x00000000 },
        { 0x003f3f3f, 0x003f3f3f },
        { 0x00808080, 0x00808080 },
        { 0x00bfbfbf, 0x00bfbfbf },
        { 0x00ffffff, 0x00ffffff },

        { 0x000000ff, 0x00ffff00 },
        { 0x0000ff00, 0x00ff00ff },
        { 0x00ff0000, 0x0000ffff },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.baseColor, test.expected);


    COLORREF actual = opp_hue(test.baseColor);
    CAPTURE(actual);

    // note: the HSL/RGB conversions have some slop,
    // so only compare to within a tolerance.
    constexpr int tolerance = 2;

    int expectedR = test.expected & 0xFF;
    int expectedG = (test.expected >> 8) & 0xFF;
    int expectedB = (test.expected >> 16) & 0xFF;

    int actualR = actual & 0xFF;
    int actualG = (actual >> 8) & 0xFF;
    int actualB = (actual >> 16) & 0xFF;

    CHECK(std::abs(expectedR - actualR) <= tolerance);
    CHECK(std::abs(expectedG - actualG) <= tolerance);
    CHECK(std::abs(expectedB - actualB) <= tolerance);
}


static constexpr double one_second = 1 / (60.0 * 60.0 * 24.0);

TEST_CASE("TZDiff")
{
    double actual = TZDiff();

    // TODO(C++20): <chrono> has timezone capabilities now, and
    // as ugly as chrono is to use, it may be less work than this.
    std::time_t base = std::time(nullptr);

    std::time_t local = std::mktime(std::localtime(&base));
    std::time_t utc = std::mktime(std::gmtime(&base));

    double computedOffsetInSeconds = std::difftime(local, utc);
    double computedOffsetInDays = computedOffsetInSeconds / 60.0 / 60.0 / 24.0;

    CHECK(std::abs(actual - computedOffsetInDays) < one_second);
}

TEST_CASE("FromTime_t")
{
    struct row
    {
        std::int64_t input;
        COleDateTime expected;
    };

    static const row testrows[] = {
        { 0x00000000, COleDateTime{ 1970, 1, 1, 0, 0, 0 } },
        { 0x64161840, COleDateTime{ 2023, 3, 18, 20, 0, 0 } },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.input, test.expected);


    DATE actual = FromTime_t(test.input);

    // undo the timezone adjustment
    actual -= TZDiff();
    CAPTURE(actual);

    CHECK(std::abs(actual - test.expected) < one_second);
}

TEST_CASE("FromTime_t_80")
{
    struct row
    {
        long input;
        COleDateTime expected;
    };

    static const row testrows[] = {
        { 0x00000000, COleDateTime{ 1980, 1, 1, 0, 0, 0 } },
        { 0x51477240, COleDateTime{ 2023, 3, 18, 20, 0, 0 } },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.input, test.expected);


    DATE actual = FromTime_t_80(test.input);

    // undo the timezone adjustment
    actual -= TZDiff();
    CAPTURE(actual);

    CHECK(std::abs(actual - test.expected) < one_second);
}

TEST_CASE("FromTime_t_mins")
{
    struct row
    {
        long input;
        COleDateTime expected;
    };

    static const row testrows[] = {
        { 0x00000000, COleDateTime{ 1970, 1, 1, 0, 0, 0 } },
        { 0x01AB08F0, COleDateTime{ 2023, 3, 18, 20, 0, 0 } },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.input, test.expected);


    DATE actual = FromTime_t_mins(test.input);

    // undo the timezone adjustment
    actual -= TZDiff();
    CAPTURE(actual);

    CHECK(std::abs(actual - test.expected) < one_second);
}

TEST_CASE("FromTime_t_1899")
{
    struct row
    {
        long input;
        COleDateTime expected;
    };

    static const row testrows[] = {
        { 0x00000000, COleDateTime{ 1899, 12, 30, 0, 0, 0 } },
        { 0x2baa2640, COleDateTime{ 1923, 3, 18, 20, 0, 0 } },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.input, test.expected);


    DATE actual = FromTime_t_1899(test.input);

    // undo the timezone adjustment
    actual -= TZDiff();
    CAPTURE(actual);

    CHECK(std::abs(actual - test.expected) < one_second);
}

TEST_CASE("FormatDate")
{
    struct row
    {
        const char* locale;
        COleDateTime input;
        CString expected;
    };

    // note: at least on win7, none of the locales I tested used either
    // two-digit years or an alternative separator, despite the norms listed
    // on wiki: https://en.wikipedia.org/w/index.php?title=Date_format_by_country&oldid=1145395238
    //
    // For example, per that page, the zh-HK (Hong Kong) locale should use
    // characters for year, month, and day, but we're seeing slashes. Other
    // locales that use a '.' also use a slash.
    static const row testrows[] = {
        { "en_US",       COleDateTime{ 2023, 3, 18, 0, 0, 0}, "3/18/2023" },
        { "en_AU",       COleDateTime{ 2023, 3, 18, 0, 0, 0}, "18/03/2023" },
        { "zh",          COleDateTime{ 2023, 3, 18, 0, 0, 0}, "2023/3/18" },
        { "zh_HK",       COleDateTime{ 2023, 3, 18, 0, 0, 0}, "18/3/2023" },

        // also check UTF8 - the app code doesn't use it now, but may in the future.
        { "en_US.UTF-8", COleDateTime{ 2023, 3, 18, 0, 0, 0}, "3/18/2023" },
        { "en_AU.UTF-8", COleDateTime{ 2023, 3, 18, 0, 0, 0}, "18/03/2023" },
        { "zh.UTF-8",    COleDateTime{ 2023, 3, 18, 0, 0, 0}, "2023/3/18" },
        { "zh_HK.UTF-8", COleDateTime{ 2023, 3, 18, 0, 0, 0}, "18/3/2023" },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.input, test.expected);


    const char* originalLocale = std::setlocale(LC_TIME, nullptr);
    char localeBuf[128] = { 0 };
    std::strncpy(localeBuf, originalLocale, sizeof(localeBuf));

    std::setlocale(LC_TIME, test.locale);

    auto cleanup = gsl::finally([&]() {
        std::setlocale(LC_TIME, localeBuf);
    });

    CString actual = FormatDate(test.input);

    CHECK(actual == test.expected);
}


TEST_CASE("NumScale")
{
    struct row
    {
        int abbrevMode;
        double value;
        CString expected;
    };

    static const row testrows[] = {
        // basic increasing magnitude scale
        { 0,                      0.0,   "    0  " },
        { 0,                      1.0,   "    1  " },
        { 0,                     12.0,   "   12  " },
        { 0,                    123.0,   "  123  " },
        { 0,                   1234.0,   " 1.23 K" },
        { 0,                  12345.0,   " 12.3 K" },
        { 0,                 123456.0,   "  123 K" },
        { 0,                1234567.0,   " 1.23 M" },
        { 0,               12345678.0,   " 12.3 M" },
        { 0,              123456789.0,   "  123 M" },
        { 0,             1234567890.0,   " 1.23 G" },
        { 0,            12345678901.0,   " 12.3 G" },
        { 0,           123456789012.0,   "  123 G" },
        { 0,          1234567890123.0,   " 1.23 T" },
        { 0,         12345678901234.0,   " 12.3 T" },
        { 0,        123456789012345.0,   "  123 T" },
        { 0,       1234567890123456.0,   " 1.23 P" },
        { 0,      12345678901234567.0,   " 12.3 P" },
        { 0,     123456789012345678.0,   "  123 P" },
        { 0,    1234567890123456789.0,   " 1.23 E" },
        { 0,   12345678901234567890.0,   " 12.3 E" },
        { 0,  123456789012345678901.0,   "  123 E" },
        { 0, 1234567890123456789012.0,   " 1e+21 " },

        // rounding after 3 digits
        { 0,                   123.4,   "  123  " },
        { 0,                   123.5,   "  124  " },    // midpoint rounds up
        { 0,                   124.5,   "  125  " },    //   not to nearest even
        { 0,                   124.6,   "  125  " },

        // negatives
        { 0,                      -0.0,   "    0  " },
        { 0,                      -1.0,   "   -1  " },
        { 0,  -123456789012345678901.0,   " -123 E" },
        { 0, -1234567890123456789012.0,   "-1e+21 " },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.abbrevMode, test.value, test.expected);


    int origAbbrevMode = theApp.k_abbrev_;
    theApp.k_abbrev_ = test.abbrevMode;
    auto cleanup = gsl::finally([=]() {
        theApp.k_abbrev_ = origAbbrevMode;
    });

    CString actual = NumScale(test.value);
    CAPTURE(actual);

    CHECK(actual == test.expected);
}

TEST_CASE("int2str")
{
    struct row
    {
        std::int64_t value;
        int radix;
        int separator_digits;
        char separator;
        bool uppercase;
        int minDigits;
        int maxDigits;

        bool success;
        CString expected;
    };

    static const row testrows[] = {
        //  value  radix    sep    upper?   digs            results
        {       0,  10,   3, ',',  false,   0, 5,    true,               "0" },
        {       1,  10,   3, ',',  false,   0, 5,    true,               "1" },
        {   12345,  10,   3, ',',  false,   0, 5,    true,          "12,345" },
        {   12345,  10,   2, '_',  false,   0, 5,    true,         "1_23_45" },

        {   65535,  16,   2, '+',  false,   0, 5,    true,           "ff+ff" },
        {   65535,  16,   2, '+',   true,   0, 5,    true,           "FF+FF" },

        {    1234,  10,   0, '.',  false,   5, 5,    true,           "01234" },
        {    1234,  10,   0, '.',  false,   1, 1,    true,               "4" },

        {    0xFF,   2,   4, '.',  false,   0, 0,    true,       "1111.1111" },
        {  0x7FFF,   2,   0, '.',  false,   0, 0,    true, "111111111111111" },

        {    1367,  37,   0, '.',  false,   0, 0,    true,              "?z" },
        {    1367,  37,   0, '.',   true,   0, 0,    true,              "?Z" },

        {  0xFFFF,   2,   0, '.',  false,   0, 0,   false,                "" }, // value too long
        {    1234,   1,   0, '.',  false,   0, 0,   false,                "" }, // radix too small
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.value, test.radix, test.expected);


    char buffer[16]{};

    bool actualSuccess = int2str(
        buffer, sizeof(buffer),
        test.value,
        test.radix,
        test.separator_digits, test.separator,
        test.uppercase,
        test.minDigits,
        test.maxDigits
    );
    CAPTURE(buffer);

    CHECK(actualSuccess == test.success);
    if (actualSuccess)
    {
        CHECK(buffer == test.expected);
    }
}

TEST_CASE("int2str - buffer len 0")
{
    char buffer[16]{};

    bool success = int2str(
        buffer, 0,
        12345,
        10,
        0, ',',
        true,
        0, 100
    );

    CHECK(!success);
}

TEST_CASE("int2str - not enough space on separator")
{
    char buffer[16]{};

    //{ 0xFFFF, 2, 0, '.', false, 0, 0, false, "" }, // value too long
    bool success = int2str(
        buffer, 3,
        7,
        2,
        2, ',',
        true,
        0, 100
    );

    CHECK(!success);
}

TEST_CASE("bin_str")
{
    struct row
    {
        std::int64_t value;
        int bits;
        CString expected;
    };

    static const row testrows[] = {
        {      0,  1,                 "0" },
        {      1,  1,                 "1" },
        { 0xAA55, 16, "10101010 01010101" },
        {
            0x55AA55AA55AA55AA,
            64,
            "01010101 10101010 01010101 10101010 01010101 10101010 01010101 10101010"
        },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.value, test.bits, test.expected);

    CString actual = bin_str(test.value, test.bits);
    CAPTURE(actual);

    CHECK(test.expected == actual);
}

TEST_CASE("AddCommas")
{
    struct row
    {
        CString input;
        int groupSize;
        char groupSeparator;

        CString expected;
    };

    static const row testrows[] = {
        {      "0", 3, ',',        "0" },
        {    "123", 3, ',',      "123" },
        { "123456", 3, ',',  "123,456" },
        { "123456", 2, '/', "12/34/56" },
        { "123456", 4, '+',  "12+3456" },

        // leading whitespace trimmed
        { "   123456", 3, ',',  "123,456" },

        // trailing whitespace preserved
        { "123456   ", 3, ',',  "123,456   " },

        // leading signs are preserved
        { "+123456", 3, '_',  "+123_456" },
        { "-123456", 3, '_',  "-123_456" },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.input, test.groupSize, test.groupSeparator, test.expected);

    int origGroupSize = theApp.dec_group_;
    int origSepChar = theApp.dec_sep_char_;
    auto cleanup = gsl::finally([=]() {
        theApp.dec_sep_char_ = origSepChar;
        theApp.dec_group_ = origGroupSize;
    });

    theApp.dec_group_ = test.groupSize;
    theApp.dec_sep_char_ = test.groupSeparator;

    CString actual = test.input;
    AddCommas(actual);
    CAPTURE(actual);

    CHECK(test.expected == actual);
}

TEST_CASE("AddSpaces")
{
    struct row
    {
        CString input;
        CString expected;
    };

    static const row testrows[] = {
        {         "0",           "0" },
        {         "1",           "1" },
        {        "12",          "12" },
        {       "123",         "123" },
        {      "1234",        "1234" },
        {     "12345",      "1 2345" },
        {    "123456",     "12 3456" },
        {   "1234567",    "123 4567" },
        {  "12345678",   "1234 5678" },
        { "123456789", "1 2345 6789" },

        // leading whitespace trimmed
        { "     123456789", "1 2345 6789" },

        // trailing whitespace preserved
        { "123456789     ", "1 2345 6789     " },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.input, test.expected);

    CString actual = test.input;
    AddSpaces(actual);
    CAPTURE(actual);

    CHECK(test.expected == actual);
}

TEST_CASE("MakePlural")
{
    struct row
    {
        CStringW input;
        CStringW expected;
    };

    static const row testrows[] = {
        { L"X", L"Xs" },
        { L"child", L"children" },
        { L"digit", L"digits" },

        // *ch, *sh, *ss, *x -> add 'es'
        { L"batch", L"batches" },
        { L"sash", L"sashes" },
        { L"glass", L"glasses" },
        { L"box", L"boxes" },

        // *(vowel)y -> add 's'
        { L"monday", L"mondays" },

        // *(consonant)y -> remove y, add 'ies'
        { L"memory", L"memories" },

        // *f -> *ves
        { L"half", L"halves" },

        // *fe -> *ves
        { L"knife", L"knives" },

        // *is -> *es
        { L"thesis", L"theses" },

        // cases *not* handled
        { L"children", L"childrens" },  // already plural

        { L"man", L"mans" },            // explicitly not handled
        { L"woman", L"womans" },
        { L"foot", L"foots" },
        { L"goose", L"gooses" },
        { L"mouse", L"mouses" },

        // some additional cases inspired from Humanizer's handling:
        // https://github.com/Humanizr/Humanizer/blob/1bdd7a1bad03fd001d04fc0e252d279b4e785818/src/Humanizer/Inflections/Vocabularies.cs
        { L"octopus", L"octopuss" },    // should be octopi
        { L"datum", L"datums" },        // should be data
        { L"salmon", L"salmons" },      // should be salmon (uncountable word)
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.input, test.expected);

    CStringW actual = MakePlural(test.input);
    CAPTURE(actual);

    CHECK(test.expected == actual);
}


TEST_CASE("make_real48")
{
    struct row
    {
        double value;
        bool bigEndian;

        bool expectedSuccess;
        std::array<std::uint8_t, 6> expectedBytes;
    };

    static const row testrows[] = {
        //            value      BE?    OK?   output
        {             0.0,     false,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        {             0.0,      true,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        {            -1.0,     false,  true, { 0x81, 0x00, 0x00, 0x00, 0x00, 0x80 } },
        {            -1.0,      true,  true, { 0x80, 0x00, 0x00, 0x00, 0x00, 0x81 } },
        {             1.0,     false,  true, { 0x81, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        {             1.0,      true,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x81 } },

        {          1e-129,      true,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },

        { 1099511627775.0,     false,  true, { 0xA8, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F } },
        { 1099511627775.0,      true,  true, { 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xA8 } },

        { INFINITY, false, false, {} },
        { -INFINITY, false, false, {} },
        { NAN, false, false, {} },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.value, test.bigEndian);


    std::uint8_t actual[6]{};

    bool success = make_real48(actual, test.value, test.bigEndian);

    CHECK(success == test.expectedSuccess);

    for (std::size_t i = 0; i < test.expectedBytes.size(); i++)
    {
        // promote these to ints so they print correctly on failure
        int actualVal = actual[i];
        int expectedVal = test.expectedBytes[i];
        CAPTURE(i);
        CHECK(actualVal == expectedVal);
    }
}

TEST_CASE("real48")
{
    struct row
    {
        std::array<std::uint8_t, 6> bytes;
        bool bigEndian;

        double expectedValue;
        int expectedExponent;
        long double expectedMantissa;
    };

    static const row testrows[] = {
        //                 bytes                    BE?          value
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, false,                0.0, -128,  0.0 },
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  true,                0.0, -128,  0.0 },
        { { 0x81, 0x00, 0x00, 0x00, 0x00, 0x80 }, false,               -1.0,    1, -1.0 },
        { { 0x80, 0x00, 0x00, 0x00, 0x00, 0x81 },  true,               -1.0,    1, -1.0 },
        { { 0x81, 0x00, 0x00, 0x00, 0x00, 0x00 }, false,                1.0,    1,  1.0 },
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x81 },  true,                1.0,    1,  1.0 },

        { { 0xA8, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F }, false,    1099511627775.0,   40,  1.999999999998L  },
        { { 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xA8 },  true,    1099511627775.0,   40,  1.999999999998L  },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.expectedValue, test.bigEndian);


    int exp{};
    long double mantissa{};
    double actual = real48(test.bytes.data(), &exp, &mantissa, test.bigEndian);

    const double valTolerance = std::abs(std::min(test.expectedValue, actual)) * DBL_EPSILON;
    CHECK(std::abs(test.expectedValue - actual) <= valTolerance);

    CHECK(exp == test.expectedExponent);
    
    static constexpr double r48_epsilon = 1.0 / (1024.0 * 1024.0 * 1024.0 * 512.0);
    const double mantTolerance = std::max(std::abs(test.expectedMantissa), std::abs(mantissa)) * r48_epsilon;
    CHECK(std::abs(test.expectedMantissa - mantissa) <= mantTolerance);
}


TEST_CASE("make_ibm_fp32")
{
    struct row
    {
        double value;
        bool littleEndian;

        bool expectedSuccess;
        std::array<std::uint8_t, 4> expectedBytes;
    };

    static const row testrows[] = {
        //               value        LE?    OK?   output
        {                  0.0,     false,  true, { 0x00, 0x00, 0x00, 0x00 } },
        {                  0.0,      true,  true, { 0x00, 0x00, 0x00, 0x00 } },
        {                 -1.0,     false,  true, { 0xC1, 0x10, 0x00, 0x00 } },
        {                 -1.0,      true,  true, { 0x00, 0x00, 0x10, 0xC1 } },
        {                  1.0,     false,  true, { 0x41, 0x10, 0x00, 0x00 } },
        {                  1.0,      true,  true, { 0x00, 0x00, 0x10, 0x41 } },

        // below normal range
        {                1e-80,     false,  true, { 0x00, 0x00, 0x00, 0x00 } },

        // smallest normal value
        { std::pow(16, -65),        false,  true, { 0x00, 0x10, 0x00, 0x00 } },

        // largest normal value
        { (1 - std::pow(16, -6)) * std::pow(16, 63),
                                    false,  true, { 0x7F, 0xFF, 0xFF, 0xFF } },

        // above normal range
        { std::pow(16, 64),         false, false, {  } },

        // examples from wikipedia:
        // https://en.wikipedia.org/wiki/IBM_hexadecimal_floating-point
        {               -118.625,   false,  true, { 0xC2, 0x76, 0xA0, 0x00 } },
        {               -118.625,    true,  true, { 0x00, 0xA0, 0x76, 0xC2 } },

        { INFINITY, false, false, {} },
        { -INFINITY, false, false, {} },
        { NAN, false, false, {} },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.value, test.littleEndian);


    std::uint8_t actual[4]{};

    bool success = make_ibm_fp32(actual, test.value, test.littleEndian);

    CHECK(success == test.expectedSuccess);

    for (std::size_t i = 0; i < test.expectedBytes.size(); i++)
    {
        // promote these to ints so they print correctly on failure
        int actualVal = actual[i];
        int expectedVal = test.expectedBytes[i];
        CAPTURE(i);
        CHECK(actualVal == expectedVal);
    }
}

TEST_CASE("ibm_fp32")
{
    struct row
    {
        std::array<std::uint8_t, 4> bytes;
        bool littleEndian;

        double expectedValue;
        int expectedExponent;
        long double expectedMantissa;
    };

    static const row testrows[] = {
        //          bytes               LE?              value  expr   mant
        { { 0x00, 0x00, 0x00, 0x00 }, false,               0.0,  -64,  0.0L },
        { { 0x00, 0x00, 0x00, 0x00 },  true,               0.0,  -64,  0.0L },
        { { 0xC1, 0x10, 0x00, 0x00 }, false,              -1.0,    1, -0.0625L },
        { { 0x00, 0x00, 0x10, 0xC1 },  true,              -1.0,    1, -0.0625L },
        { { 0x41, 0x10, 0x00, 0x00 }, false,               1.0,    1,  0.0625L },
        { { 0x00, 0x00, 0x10, 0x41 },  true,               1.0,    1,  0.0625L },

        // smallest normal value
        { { 0x00, 0x10, 0x00, 0x00 }, false, std::pow(16, -65),  -64,  0.0625L },

        // largest normal value
        { { 0x7F, 0xFF, 0xFF, 0xFF }, false, (1 - std::pow(16, -6)) * std::pow(16, 63),
                                                                  63,  1.0L },

        // examples from wikipedia:
        // https://en.wikipedia.org/wiki/IBM_hexadecimal_floating-point
        { { 0xC2, 0x76, 0xA0, 0x00 },   false,        -118.625,    2, -0x0.76A000p+0L },
        { { 0x00, 0xA0, 0x76, 0xC2 },    true,        -118.625,    2, -0x0.76A000p+0L },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.expectedValue, test.littleEndian);


    int exp{};
    long double mantissa{};
    double actual = ibm_fp32(test.bytes.data(), &exp, &mantissa, test.littleEndian);

    const double valTolerance = std::abs(std::min(test.expectedValue, actual)) * DBL_EPSILON;
    CHECK(std::abs(test.expectedValue - actual) <= valTolerance);

    CHECK(exp == test.expectedExponent);

    static constexpr double ibm32_epsilon = 0x1.0p-24;
    const double mantTolerance = std::max(std::abs(test.expectedMantissa), std::abs(mantissa)) * ibm32_epsilon;
    CHECK(std::abs(test.expectedMantissa - mantissa) <= mantTolerance);
}


TEST_CASE("make_ibm_fp64")
{
    struct row
    {
        double value;
        bool littleEndian;

        bool expectedSuccess;
        std::array<std::uint8_t, 8> expectedBytes;
    };

    static const row testrows[] = {
        //               value        LE?    OK?   output
        {                    0.0,   false,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        {                    0.0,    true,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        {                   -1.0,   false,  true, { 0xC1, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        {                   -1.0,    true,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xC1 } },
        {                    1.0,   false,  true, { 0x41, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        {                    1.0,    true,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x41 } },

        // below normal range
        {                  1e-80,   false,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },

        // smallest normal value
        {      std::pow(16, -65),   false,  true, { 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },

        // largest normal value that is also representable in IEEE binary64
        // HFP64 has 56 bits of mantissa, while IEEE only has 53, so there is
        // necessarily some imprecision in the conversion at it's extremes.
        { 0x1.FFFFFFFFFFFFF0p+251,   false,  true, { 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8 } },

        // above normal range
        { 0x1.FFFFFFFFFFFFF8p+251,   false, false, {  } },

        // examples from wikipedia:
        // https://en.wikipedia.org/wiki/IBM_hexadecimal_floating-point
        {               -118.625,   false,  true, { 0xC2, 0x76, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        {               -118.625,    true,  true, { 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x76, 0xC2 } },

        { INFINITY, false, false, {} },
        { -INFINITY, false, false, {} },
        { NAN, false, false, {} },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.value, test.littleEndian);


    std::uint8_t actual[8]{};

    bool success = make_ibm_fp64(actual, test.value, test.littleEndian);

    CHECK(success == test.expectedSuccess);

    for (std::size_t i = 0; i < test.expectedBytes.size(); i++)
    {
        // promote these to ints so they print correctly on failure
        int actualVal = actual[i];
        int expectedVal = test.expectedBytes[i];
        CAPTURE(i);
        CHECK(actualVal == expectedVal);
    }
}

TEST_CASE("ibm_fp64")
{
    struct row
    {
        std::array<std::uint8_t, 8> bytes;
        bool littleEndian;

        double expectedValue;
        int expectedExponent;
        long double expectedMantissa;
    };

    static const row testrows[] = {
        //                       bytes                          LE?              value  expr   mant
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, false,               0.0,  -64,  0.0L },
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  true,               0.0,  -64,  0.0L },
        { { 0xC1, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, false,              -1.0,    1, -0.0625L },
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xC1 },  true,              -1.0,    1, -0.0625L },
        { { 0x41, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, false,               1.0,    1,  0.0625L },
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x41 },  true,               1.0,    1,  0.0625L },

        // smallest normal value
        { { 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, false, std::pow(16, -65),  -64,  0.0625L },

        // largest normal value
        { { 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, false, 0x1.FFFFFFFFFFFFF0p+251,
                                                                                          63,  1.0L },

        // examples from wikipedia:
        // https://en.wikipedia.org/wiki/IBM_hexadecimal_floating-point
        { { 0xC2, 0x76, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00 },   false,        -118.625,    2, -0x0.76A000p+0L },
        { { 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x76, 0xC2 },    true,        -118.625,    2, -0x0.76A000p+0L },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.expectedValue, test.littleEndian, test.expectedExponent, test.expectedMantissa);


    int exp{};
    long double mantissa{};
    double actual = ibm_fp64(test.bytes.data(), &exp, &mantissa, test.littleEndian);

    char actualBits[64]{};
    std::sprintf(actualBits, "%a", actual);
    CAPTURE(actualBits);

    const double valTolerance = std::abs(std::min(test.expectedValue, actual)) * DBL_EPSILON;
    CHECK(std::abs(test.expectedValue - actual) <= valTolerance);

    CHECK(exp == test.expectedExponent);

    static constexpr double ibm64_epsilon = 0x1.0p-56;
    const double mantTolerance = std::max(std::abs(test.expectedMantissa), std::abs(mantissa)) * ibm64_epsilon;
    CHECK(std::abs(test.expectedMantissa - mantissa) <= mantTolerance);
}


TEST_CASE("strtoi64")
{
    struct row
    {
        const char* input;
        int radix;
        std::int64_t expectedValue;
        int expectedEndIndex;
    };

    static const row testrows[] = {
        // single digit tests
        { "0",  2,  0, 1 },
        { "1",  2,  1, 1 },
        { "2",  8,  2, 1 },
        { "3",  8,  3, 1 },
        { "4",  8,  4, 1 },
        { "5",  8,  5, 1 },
        { "6",  8,  6, 1 },
        { "7",  8,  7, 1 },
        { "8", 10,  8, 1 },
        { "9", 10,  9, 1 },
        { "a", 16, 10, 1 },     // Hex, lowercase
        { "b", 16, 11, 1 },
        { "c", 16, 12, 1 },
        { "d", 16, 13, 1 },
        { "e", 16, 14, 1 },
        { "f", 16, 15, 1 },
        { "A", 16, 10, 1 },     // Hex, uppercase
        { "B", 16, 11, 1 },
        { "C", 16, 12, 1 },
        { "D", 16, 13, 1 },
        { "E", 16, 14, 1 },
        { "F", 16, 15, 1 },

        // min value for each radix
        //{ "0",  2,  0, 1 }, // dup row
        { "0",  3,  0, 1 },
        { "0",  4,  0, 1 },
        { "0",  5,  0, 1 },
        { "0",  6,  0, 1 },
        { "0",  7,  0, 1 },
        { "0",  8,  0, 1 },
        { "0",  9,  0, 1 },
        { "0", 10,  0, 1 },
        { "0", 11,  0, 1 },
        { "0", 12,  0, 1 },
        { "0", 13,  0, 1 },
        { "0", 14,  0, 1 },
        { "0", 15,  0, 1 },
        { "0", 16,  0, 1 },

        // radix defaults to 10 if 0 (default value)
        { "0",  0,  0, 1 },
        { "9",  0,  9, 1 },
        { "A",  0,  0, 0 },

        // max value for each radix
        {
            "111111111111111111111111111111111111111111111111111111111111111",
            2,
            INT64_MAX,
            63
        },
        {
            "2021110011022210012102010021220101220221",
            3,
            INT64_MAX,
            40
        },
        {
            "13333333333333333333333333333333",
            4,
            INT64_MAX,
            32
        },
        {
            "1104332401304422434310311212",
            5,
            INT64_MAX,
            28
        },
        {
            "1540241003031030222122211",
            6,
            INT64_MAX,
            25
        },
        {
            "22341010611245052052300",
            7,
            INT64_MAX,
            23
        },
        {
            "777777777777777777777",
            8,
            INT64_MAX,
            21
        },
        {
            "67404283172107811827",
            9,
            INT64_MAX,
            20
        },
        {
            "9223372036854775807",
            10,
            INT64_MAX,
            19
        },
        {
            "1728002635214590697",
            11,
            INT64_MAX,
            19
        },
        {
            "41A792678515120367",
            12,
            INT64_MAX,
            18
        },
        {
            "10B269549075433C37",
            13,
            INT64_MAX,
            18
        },
        {
            "4340724C6C71DC7A7",
            14,
            INT64_MAX,
            17
        },
        {
            "160E2AD3246366807",
            15,
            INT64_MAX,
            17
        },
        {
            "7FFFFFFFFFFFFFFF",
            16,
            INT64_MAX,
            16
        },
    
        // digit too large - terminate parsing (no preceding digits)
        { "2",  2, 0, 0 },
        { "3",  3, 0, 0 },
        { "4",  4, 0, 0 },
        { "5",  5, 0, 0 },
        { "6",  6, 0, 0 },
        { "7",  7, 0, 0 },
        { "8",  8, 0, 0 },
        { "9",  9, 0, 0 },
        { "A", 10, 0, 0 },
        { "B", 11, 0, 0 },
        { "C", 12, 0, 0 },
        { "D", 13, 0, 0 },
        { "E", 14, 0, 0 },
        { "F", 15, 0, 0 },
    
        // digit too large - terminate parsing (yes preceding digits)
        { "12",  2,  1, 1 },
        { "23",  3,  2, 1 },
        { "34",  4,  3, 1 },
        { "45",  5,  4, 1 },
        { "56",  6,  5, 1 },
        { "67",  7,  6, 1 },
        { "78",  8,  7, 1 },
        { "89",  9,  8, 1 },
        { "9A", 10,  9, 1 },
        { "AB", 11, 10, 1 },
        { "BC", 12, 11, 1 },
        { "CD", 13, 12, 1 },
        { "DE", 14, 13, 1 },
        { "EF", 15, 14, 1 },
    };

    const row test = GENERATE(
        Catch::Generators::from_range(
            std::begin(testrows),
            std::end(testrows)
        )
    );
    CAPTURE(test.input, test.radix, test.expectedValue, test.expectedEndIndex);
    const char* expectedEndPtr = &(test.input[test.expectedEndIndex]);

    std::int64_t result = strtoi64(test.input, test.radix);
    CAPTURE(result);

    CHECK(test.expectedValue == result);
}

TEST_CASE("strtoi64 - garbage character handling")
{
    // also hit the heap allocation branch
    static constexpr const char input[] = "1"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "-*/+;'[]\=,._"
        "2";

    std::int64_t result = strtoi64(input, 10);
    CHECK(result == 12);
}

