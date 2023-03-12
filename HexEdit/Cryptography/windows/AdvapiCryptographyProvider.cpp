#include "Stdafx.h"
#include "AdvapiCryptographyProvider.h"

#include <wincrypt.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include <cstring>

#pragma comment(lib, "advapi32.lib")

#define TRACE_TAG "[crypt/advapi] "

//TODO(6.0): MSDocs says the Crypto APIs are deprecated, and to use the BCrypt functions instead. Do they support the use here?

namespace hex
{
    static constexpr HCRYPTKEY NullCryptKey = HCRYPTKEY{ 0 };

    const CString AdvapiCryptographyProvider::ProviderName{ "ADVAPI32" };


    class AdvapiAlgorithm : public ICryptographyAlgorithm
    {
    public:
        AdvapiAlgorithm(const CString& name, HCRYPTPROV hProvider, ALG_ID id, int blockBits, int keyBits) :
            _name{ name },
            _hProvider{},
            _algorithmId{ id },
            _key{ NullCryptKey },
            _blockBitLength{ blockBits },
            _keyBitLength{ keyBits }
        {
            CryptContextAddRef(_hProvider, nullptr, 0);
        }

        ~AdvapiAlgorithm()
        {
            CryptReleaseContext(_hProvider, 0);
        }


        const char* Name() const override { return static_cast<const char*>(_name); }
        int BlockLength() const override { return _blockBitLength; }
        int KeyLength() const override { return _keyBitLength; }

        bool NeedsPassword() const override { return _key == NullCryptKey; }

        void SetPassword(const char* password) override
        {
            if (_key == NullCryptKey)
            {
                CryptDestroyKey(_key);
                _key = NullCryptKey;
            }

            if (password)
            {
                HCRYPTHASH hHash;

                //TODO(6.0): Let the user select an algorithm? Allow MD5 for compatibility, but mark it not secure!
                if (!CryptCreateHash(_hProvider, CALG_SHA, 0, 0, &hHash)
                    && !CryptCreateHash(_hProvider, CALG_MD5, 0, 0, &hHash))
                {
                    throw std::exception{ static_cast<const char*>("Could not create hash using\n" + _name) };
                }

                if (!CryptHashData(hHash, (const BYTE*)password, std::strlen(password), 0)
                    || !CryptDeriveKey(_hProvider, _algorithmId, hHash, 0, &_key))
                {
                    CryptDestroyHash(hHash);
                    throw std::exception{ static_cast<const char*>("Could not create key from password using\n" + _name) };
                }

                CryptDestroyHash(hHash);
            }
        }

        std::size_t EncryptedSize(
            std::size_t length,
            bool finalBlock = true) override
        {
            assert(_key != NullCryptKey);
            DWORD size = length;

            if (!CryptEncrypt(_key, HCRYPTHASH{ 0 }, finalBlock, 0, nullptr, &size, size))
            {
                throw std::exception{ static_cast<const char*>("Could not get encryption length needed using\n" + _name) };
            }

            return static_cast<std::size_t>(size);
        }

        std::size_t Encrypt(
            std::uint8_t* buffer,
            std::size_t dataLen,
            std::size_t bufferLen,
            bool finalBlock = true) override
        {
            assert(_key != NullCryptKey);
            DWORD size = dataLen;

            if (!CryptEncrypt(_key, HCRYPTHASH{ 0 }, finalBlock, 0, buffer, &size, bufferLen))
            {
                assert(false);
                return ~0u;
            }

            assert(size < bufferLen);
            return size;
        }

        std::size_t Decrypt(
            std::uint8_t* buffer,
            std::size_t dataLen,
            bool finalBlock = true) override
        {
            assert(_key != NullCryptKey);
            DWORD size = dataLen;

            if (!CryptDecrypt(_key, HCRYPTHASH{ 0 }, finalBlock, 0, buffer, &size))
            {
                return ~0u;
            }

            return size;
        }


    private:
        CString _name;
        HCRYPTPROV _hProvider;
        ALG_ID _algorithmId;
        HCRYPTKEY _key;
        int _blockBitLength;
        int _keyBitLength;
    };


    // ADVAPI32 algorithms shouldn't change over the course of one execution,
    // so we only need to compute them once and we can throw them in a global.
    static bool g_initialized = false;
    static std::vector<AdvapiAlgorithm> g_algorithms;


    static void ensure_advapi_init()
    {
        if (g_initialized)
        {
            // we've already init'd, so don't do it again
            return;
        }

        DWORD index = 0;
        DWORD type = 0;
        DWORD nameLen = 0;

        std::vector<AdvapiAlgorithm> algorithms;

        // read the algorithm providers
        for (index = 0;
            CryptEnumProvidersA(index, nullptr, 0, &type, nullptr, &nameLen);
            index++)
        {
            std::unique_ptr<char[]> providerName = std::make_unique<char[]>(nameLen);
            std::memset(providerName.get(), 0, nameLen);

            if (!CryptEnumProvidersA(index, nullptr, 0, &type, providerName.get(), &nameLen))
            {
                TRACE(TRACE_TAG "Failed to retrieve algorithm provider %d. Skipping.", index);
                continue;
            }

            HCRYPTPROV hProvider;
            if (!CryptAcquireContextA(&hProvider, nullptr, providerName.get(), type, CRYPT_VERIFYCONTEXT))
            {
                TRACE(TRACE_TAG "Failed to acquire context for algorithm provider %s. Skipping", providerName.get());
                continue;
            }

            PROV_ENUMALGS alginfo;
            DWORD alginfoLen = sizeof(alginfo);
            DWORD flags = CRYPT_FIRST;
            while (CryptGetProvParam(hProvider, PP_ENUMALGS, (BYTE*)&alginfo, &alginfoLen, flags))
            {
                if (GET_ALG_CLASS(alginfo.aiAlgid) == ALG_CLASS_DATA_ENCRYPT)
                {
                    CString algorithmName = CString{ alginfo.szName, (int)alginfo.dwNameLen } + ":" + providerName.get();

                    // get the algorithm block/length sizes via a temporary encryption key.
                    HCRYPTKEY hKey;
                    DWORD blockLen, keyLen;
                    DWORD dataLen = sizeof(blockLen);
                    if (!CryptGenKey(hProvider, alginfo.aiAlgid, 0, &hKey)
                        || !CryptGetKeyParam(hKey, KP_BLOCKLEN, (BYTE*)&blockLen, &dataLen, 0)
                        || dataLen != sizeof(blockLen))
                    {
                        TRACE(TRACE_TAG "Failed to acquire block length for algorithm %s. Skipping", static_cast<const char*>(algorithmName));

                        CryptDestroyKey(hKey);
                        continue;
                    }

                    // note: key length isn't valid for all algorithms, so don't fail when getting that does.
                    dataLen = sizeof(keyLen);
                    if (!CryptGetKeyParam(hProvider, KP_KEYLEN, (BYTE*)&keyLen, &dataLen, 0))
                    {
                        keyLen = 0;
                    }
                    CryptDestroyKey(hKey);

                    // add the algorithm.
                    algorithms.emplace_back(algorithmName, hProvider, alginfo.aiAlgid, blockLen, keyLen);
                }
            }
        }

        g_algorithms.swap(algorithms);
        g_initialized = true;
    }


    std::size_t AdvapiCryptographyProvider::AlgorithmCount() const
    {
        ensure_advapi_init();

        return g_algorithms.size();
    }

    ICryptographyAlgorithm& AdvapiCryptographyProvider::GetAlgorithm(std::size_t index) const
    {
        ensure_advapi_init();

        if (!(index < g_algorithms.size()))
        {
            throw std::range_error{ "Algorithm index out of range." };
        }

        return g_algorithms[index];
    }
}
