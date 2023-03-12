#pragma once
#include "../ICryptographyProvider.h"

namespace hex
{
    class AdvapiCryptographyProvider : ICryptographyProvider
    {
        static const CString ProviderName;

    public:
        CString Name() const override { return ProviderName; }

        std::size_t AlgorithmCount() const override;

        ICryptographyAlgorithm& GetAlgorithm(std::size_t index) const override;
    };
}
