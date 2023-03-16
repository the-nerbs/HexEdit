#include "Stdafx.h"
#include "Cryptography/windows/AdvapiCryptographyProvider.h"
#include "Cryptography/cryptography_error.h"

#include <cryptopp/cryptlib.h>
#include <cryptopp/aes.h>
#include <cryptopp/files.h>
#include <cryptopp/modes.h>

#include <catch.hpp>

#include <array>
#include <string>

// AES128 *should* be a common enough algorithm that it'll exist everywhere.
// There are a few instances of it from separate ADVAPI providers though, so
// the one chosen here looks like the most generic one. The others were for 
// SChannel (docs sound like a server/client thing) or smart cards.
//
// The ADVAPI32.dll this was observed with was on windows 7:
//  - Version: 6.1.7601.24499
//  - SHA256:  DF8B735C1A2CAF45DE4520F6DB069487853A35496387F55AB7931126C380ACC9
static constexpr const char* TestAlgorithmName = "AES 128:Microsoft Enhanced RSA and AES Cryptographic Provider";

static constexpr const char* TestPassword = "p@ssw0rd1";


static hex::ICryptographyAlgorithm& findTestAlgorithm(hex::AdvapiCryptographyProvider& provider)
{
    const std::size_t count = provider.AlgorithmCount();

    for (size_t id = 0; id < count; id++)
    {
        hex::ICryptographyAlgorithm& alg = provider.GetAlgorithm(id);

        if (std::strcmp(alg.Name(), TestAlgorithmName) == 0)
        {
            return alg;
        }
    }

    FAIL("Failed to find test algorithm.");
    throw std::exception{ "test algorithm not found."};
}


TEST_CASE("AdvapiCryptographyProvider::Name")
{
    hex::AdvapiCryptographyProvider provider;

    CHECK(provider.Name() == "ADVAPI32");
}

TEST_CASE("AdvapiCryptographyProvider::GetAlgorithm - out of range")
{
    hex::AdvapiCryptographyProvider provider;

    CHECK_THROWS_AS(provider.GetAlgorithm(~0u), std::range_error);
}

TEST_CASE("AdvapiCryptographyProvider - test algorithm parameters")
{
    hex::AdvapiCryptographyProvider provider;

    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);

    CHECK(algorithm.BlockLength() == 128 /* bits */);
    CHECK(algorithm.KeyLength() == 128 /* bits */);
}

TEST_CASE("AdvapiAlgorithm::NeedsPassword")
{
    hex::AdvapiCryptographyProvider provider;

    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);

    algorithm.SetPassword(nullptr);
    CHECK(algorithm.NeedsPassword());

    algorithm.SetPassword(TestPassword);
    CHECK(!algorithm.NeedsPassword());
}

TEST_CASE("AdvapiAlgorithm::EncryptedSize")
{
    struct row
    {
        std::size_t length;
        bool isFinalBlock;
        std::size_t expectedSize;
    };

    static const row testrows[] =
    {
        // note: < block size requires finalBlock to be true
        row{ 0, true, 16, },
        row{ 1, true, 16, },
        row{ 2, true, 16, },
        row{ 3, true, 16, },
        row{ 4, true, 16, },
        row{ 5, true, 16, },
        row{ 6, true, 16, },
        row{ 7, true, 16, },
        row{ 8, true, 16, },
        row{ 9, true, 16, },
        row{ 10, true, 16, },
        row{ 11, true, 16, },
        row{ 12, true, 16, },
        row{ 13, true, 16, },
        row{ 14, true, 16, },
        row{ 15, true, 16, },

        row { 16, false, 16, },
        row { 16, true, 16 + 16, },

        row { 1024, false, 1024, },
        row { 1024, true,  1024 + 16, },

        row { 1024 * 1024, false, 1024 * 1024, },
        row { 1024 * 1024, true,  1024 * 1024 + 16, },
    };

    row test = GENERATE(Catch::Generators::from_range(std::begin(testrows), std::end(testrows)));

    hex::AdvapiCryptographyProvider provider;
    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);
    algorithm.SetPassword(TestPassword);

    std::size_t actual = algorithm.EncryptedSize(test.length, test.isFinalBlock);

    CAPTURE(test.length);
    CAPTURE(test.isFinalBlock);
    CAPTURE(test.expectedSize);

    CHECK(actual == test.expectedSize);
}

TEST_CASE("AdvapiAlgorithm::EncryptedSize - size error")
{
    hex::AdvapiCryptographyProvider provider;
    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);
    algorithm.SetPassword(TestPassword);

    // finalBlock == false requires a full block.
    CHECK_THROWS_AS(algorithm.EncryptedSize(4, false), hex::cryptography_error);
}

TEST_CASE("AdvapiAlgorithm::Encrypt")
{
    struct row
    {
        std::vector<std::uint8_t> data;
        bool isFinalBlock;
        std::vector<std::uint8_t> expectedData;
    };

    // note: these are reversing the rows in the decrypt tests
    static const row testrows[] =
    {
        row
        {
            {
                0x00
            },
            true,
            {
                0x08, 0xF5, 0x93, 0x76, 0x14, 0x81, 0xB5, 0xDF, 0x9D, 0x2A, 0x84, 0x34, 0x94, 0xF7, 0x82, 0x65,
            }
        },
        row
        {
            {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            },
            true,
            {
                0x13, 0x1F, 0xCF, 0x92, 0x56, 0x9A, 0xD8, 0x22, 0x2A, 0x6E, 0x06, 0xDD, 0x5A, 0xAE, 0x9C, 0x11,
                0x9D, 0xA5, 0x97, 0xED, 0x41, 0xA3, 0x68, 0x5A, 0x0C, 0xAE, 0xCD, 0x10, 0xA8, 0xFE, 0xFB, 0xF6,
            }
        },
    };

    row test = GENERATE(Catch::Generators::from_range(std::begin(testrows), std::end(testrows)));

    hex::AdvapiCryptographyProvider provider;
    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);
    algorithm.SetPassword(TestPassword);

    std::vector<std::uint8_t> buffer;
    buffer.resize(1024);

    std::copy(test.data.cbegin(), test.data.cend(), buffer.begin());

    std::size_t actualLen = algorithm.Encrypt(
        (BYTE*)buffer.data(),
        test.data.size(),
        buffer.size(),
        test.isFinalBlock
    );

    CAPTURE(test.data.size());
    CAPTURE(test.isFinalBlock);
    CAPTURE(test.expectedData.size());

    CHECK(actualLen == test.expectedData.size());


    bool equal = std::equal(
        buffer.begin(), buffer.begin() + actualLen,
        test.expectedData.begin(), test.expectedData.end()
    );
    CHECK(equal);
}

TEST_CASE("AdvapiAlgorithm::Encrypt - size error")
{
    hex::AdvapiCryptographyProvider provider;
    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);
    algorithm.SetPassword(TestPassword);

    std::vector<std::uint8_t> buffer;
    buffer.resize(15);

    CHECK_THROWS_AS(algorithm.Encrypt(
        (BYTE*)buffer.data(),
        15,
        15,
        false
    ), hex::cryptography_error);
}

TEST_CASE("AdvapiAlgorithm::Encrypt - setting password twice does not effect output")
{
    hex::AdvapiCryptographyProvider provider;
    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);

    algorithm.SetPassword("not the test password");
    algorithm.SetPassword(TestPassword);

    // same block as one row in the CCrypto::encrypt case
    std::array<std::uint8_t, 16> buffer = { 0x00 };

    std::size_t actualLen = algorithm.Encrypt(
        (BYTE*)buffer.data(),
        1,
        buffer.size(),
        true
    );

    const std::array<std::uint8_t, 16> expectedData = {
        0x08, 0xF5, 0x93, 0x76, 0x14, 0x81, 0xB5, 0xDF,
        0x9D, 0x2A, 0x84, 0x34, 0x94, 0xF7, 0x82, 0x65
    };

    bool equal = std::equal(
        buffer.begin(), buffer.begin() + actualLen,
        expectedData.begin(), expectedData.end()
    );
    CHECK(equal);
}

TEST_CASE("AdvapiAlgorithm::Encrypt - matches Crypto++ results")
{
    hex::AdvapiCryptographyProvider provider;
    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);
    algorithm.SetPassword(TestPassword);

    std::vector<std::uint8_t> data;
    data.resize(1024 * 1024);

    std::size_t dataLen = data.size();

    for (size_t i = 0; i < dataLen; i++)
    {
        data[i] = static_cast<std::uint8_t>(i & 0xFF);
    }


    std::vector<std::uint8_t> hexOutput = data;
    hexOutput.resize(hexOutput.size() + 16);

    std::size_t actualLen = algorithm.Encrypt(
        (BYTE*)hexOutput.data(),
        dataLen,
        hexOutput.size(),
        true
    );

    // exported from inside the SetPassword call above. See comments in there for how.
    CryptoPP::byte iv[] =
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    CryptoPP::byte key[] =
    {
        0xb0, 0x2e, 0x77, 0x56, 0x45, 0x7c, 0x61, 0xf8, 0x7f, 0xb2, 0x61, 0x5b, 0x8b, 0x53, 0x6c, 0x29,
    };

    CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption cipher{};
    cipher.SetKeyWithIV(key, sizeof(key), iv, sizeof(iv));

    std::vector<std::uint8_t> cryptoppOutput;

    CryptoPP::VectorSource s{
        data,
        true,
        new CryptoPP::StreamTransformationFilter{
            cipher,
            new CryptoPP::VectorSink{ cryptoppOutput }
        }
    };

    CHECK(actualLen == cryptoppOutput.size());

    for (size_t i = 0; i < actualLen; i++)
    {
        if (hexOutput[i] != cryptoppOutput[i])
        {
            CAPTURE(i, hexOutput[i], cryptoppOutput[i]);
            FAIL("Output does not match.");
        }
    }
}


TEST_CASE("AdvapiAlgorithm::Decrypt")
{
    struct row
    {
        std::vector<std::uint8_t> encryptedData;
        bool isFinalBlock;
        std::vector<std::uint8_t> expectedData;
    };

    // note: these are reversing the rows in the encrypt tests
    static const row testrows[] =
    {
        row
        {
            {
                0x08, 0xF5, 0x93, 0x76, 0x14, 0x81, 0xB5, 0xDF, 0x9D, 0x2A, 0x84, 0x34, 0x94, 0xF7, 0x82, 0x65,
            },
            true,
            {
                0x00
            }
        },
        row
        {
            {
                0x13, 0x1F, 0xCF, 0x92, 0x56, 0x9A, 0xD8, 0x22, 0x2A, 0x6E, 0x06, 0xDD, 0x5A, 0xAE, 0x9C, 0x11,
                0x9D, 0xA5, 0x97, 0xED, 0x41, 0xA3, 0x68, 0x5A, 0x0C, 0xAE, 0xCD, 0x10, 0xA8, 0xFE, 0xFB, 0xF6,
            },
            true,
            {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            }
        },
    };

    row test = GENERATE(Catch::Generators::from_range(std::begin(testrows), std::end(testrows)));

    hex::AdvapiCryptographyProvider provider;
    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);
    algorithm.SetPassword(TestPassword);

    std::vector<std::uint8_t> buffer;

    buffer.resize(1024);
    std::copy(test.encryptedData.cbegin(), test.encryptedData.cend(), buffer.begin());

    std::size_t actualLen = algorithm.Decrypt(
        (BYTE*)buffer.data(),
        test.encryptedData.size(),
        test.isFinalBlock
    );

    CAPTURE(test.encryptedData.size());
    CAPTURE(test.isFinalBlock);
    CAPTURE(test.expectedData.size());

    CHECK(actualLen == test.expectedData.size());

    bool equal = std::equal(
        buffer.begin(), buffer.begin() + actualLen,
        test.expectedData.begin(), test.expectedData.end()
    );
    CHECK(equal);
}

TEST_CASE("AdvapiAlgorithm::Decrypt - size error")
{
    hex::AdvapiCryptographyProvider provider;
    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);
    algorithm.SetPassword(TestPassword);

    std::vector<std::uint8_t> buffer;
    buffer.resize(15);

    CHECK_THROWS_AS(algorithm.Decrypt(
        (BYTE*)buffer.data(),
        15,
        false
    ), hex::cryptography_error);
}

TEST_CASE("AdvapiAlgorithm::Decrypt - matches Crypto++ results")
{
    hex::AdvapiCryptographyProvider provider;
    hex::ICryptographyAlgorithm& algorithm = findTestAlgorithm(provider);
    algorithm.SetPassword(TestPassword);

    std::vector<std::uint8_t> data{
        0x13, 0x1F, 0xCF, 0x92, 0x56, 0x9A, 0xD8, 0x22, 0x2A, 0x6E, 0x06, 0xDD, 0x5A, 0xAE, 0x9C, 0x11,
        0x9D, 0xA5, 0x97, 0xED, 0x41, 0xA3, 0x68, 0x5A, 0x0C, 0xAE, 0xCD, 0x10, 0xA8, 0xFE, 0xFB, 0xF6,
    };

    std::vector<std::uint8_t> hexOutput = data;

    std::size_t actualLen = algorithm.Decrypt(
        (BYTE*)hexOutput.data(),
        hexOutput.size(),
        true
    );

    // exported from inside the SetPassword call above. See comments in there for how.
    CryptoPP::byte iv[] =
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    CryptoPP::byte key[] =
    {
        0xb0, 0x2e, 0x77, 0x56, 0x45, 0x7c, 0x61, 0xf8, 0x7f, 0xb2, 0x61, 0x5b, 0x8b, 0x53, 0x6c, 0x29,
    };

    CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption cipher{};
    cipher.SetKeyWithIV(key, sizeof(key), iv, sizeof(iv));

    std::vector<std::uint8_t> cryptoppOutput;

    CryptoPP::VectorSource s{
        data,
        true,
        new CryptoPP::StreamTransformationFilter{
            cipher,
            new CryptoPP::VectorSink{ cryptoppOutput }
        }
    };

    CHECK(actualLen == cryptoppOutput.size());

    for (size_t i = 0; i < actualLen; i++)
    {
        if (hexOutput[i] != cryptoppOutput[i])
        {
            CAPTURE(i, hexOutput[i], cryptoppOutput[i]);
            FAIL("Output does not match.");
        }
    }
}
