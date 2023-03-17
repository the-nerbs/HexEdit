#pragma once
#include "ICryptographyAlgorithm.h"

#include <afxstr.h>

#include <cstddef>

namespace hex
{
    /// \brief  Provides cryptography services.
    ///
    /// \remarks
    /// Providers are intended to encapsulate and provide access to all
    /// algorithms exposed by a library or API set. For example, the Windows
    /// ADVAPI32 Crypt APIs have their own provider, but the newer BCrypt APIs
    /// have a separate one.
    class ICryptographyProvider
    {
    public:
        virtual ~ICryptographyProvider() = default;


        /// \brief  Gets the name of this provider.
        virtual CString Name() const = 0;

        /// \brief  Gets the number of algorithms available from this provider.
        ///
        /// \remarks
        /// Implementations are required to return the same count any time it
        /// is called during a single run of the application. However, the
        /// count may change between executions.
        virtual std::size_t AlgorithmCount() const = 0;

        /// \brief  Gets an algorithm.
        ///
        /// \exception  std::range_error  \p index is out of range.
        /// 
        /// \remarks
        /// Implementations are required to return the same algorithm for any
        /// specific index, though it may be a reference to a different object.
        virtual ICryptographyAlgorithm& GetAlgorithm(std::size_t index) const = 0;
    };
}
