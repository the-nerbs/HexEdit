#pragma once
#include "../ICryptographyProvider.h"

namespace hex
{
    /// \brief  Cryptography algorithm provider for the Windows Crypt* APIs
    class AdvapiCryptographyProvider : public ICryptographyProvider
    {
        static const CString ProviderName;

    public:
        CString Name() const override { return ProviderName; }

        std::size_t AlgorithmCount() const override;

        ICryptographyAlgorithm& GetAlgorithm(std::size_t index) const override;
    };
}
