// Crypto.h : header file for cryptography class
//
// Copyright (c) 2015 by Andrew W. Phillips
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it and I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#pragma once

#include "Cryptography/ICryptographyProvider.h"
#include "Cryptography/ICryptographyAlgorithm.h"
#include "Services/IDialogProvider.h"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#if _MSC_VER < 1300
#define _WIN32_WINNT  0x0400  // To get stuff in wincrypt.h
#endif
#include <wincrypt.h>
#if _MSC_VER < 1300
#undef _WIN32_WINNT
#endif

class CCrypto
{
public:
	CCrypto();
	explicit CCrypto(hex::IDialogProvider& dialogProvider);


	/// \brief  Registers a new cryptography provider.
	///
	/// \param  provider  The provider. If null, no action is taken.
	void RegisterProvider(std::unique_ptr<hex::ICryptographyProvider> provider);

	/// \brief  Registers a new cryptography provider.
	///
	/// \tparam  T  The provider type. Must implement ICryptographyProvider and be default constructible.
	template <class T>
	void RegisterProvider()
	{
		static_assert(std::is_base_of_v<hex::ICryptographyProvider, T>, "Provider must implement ICryptographyProvider.");
		static_assert(std::is_default_constructible_v<T>, "Provider must implement ICryptographyProvider.");
		std::unique_ptr<hex::ICryptographyProvider> prov = std::make_unique<T>();
		RegisterProvider(std::move(prov));
	}


	/// \brief  Gets the number of algorithms that are available.
	std::size_t GetNum() const;

	/// \brief  Gets algorithm name, including the CSP name.
	/// 
	/// \param  alg  The algorithm ID.
	const char *GetName(std::size_t alg) const;

	/// \brief  Set an algorithm's encryption/decryption password.
	/// 
	/// \param  alg       The algorithm ID.
	/// \param  password  The password string, or null to clear a previously set password.
	void SetPassword(std::size_t alg, const char *password = nullptr);

	/// \brief  Determines if an algorithm requires a password.
	/// 
	/// \param  alg       The algorithm ID.
	bool NeedsPassword(std::size_t alg) const;

	/// \brief  Gets the cipher block length.
	/// 
	/// \param  alg  The algorithm ID.
	/// 
	/// \returns The block length in bits, or zero for stream ciphers.
	int GetBlockLength(std::size_t alg) const;

	/// \brief  Gets the key length.
	/// 
	/// \param  alg  The algorithm ID.
	/// 
	/// \returns The key length in bits.
	/// 
	/// \remarks
	/// Higher values usually indicate stronger encryption.
	int GetKeyLength(std::size_t alg) const;


	/// \brief  Computes the space needed to encrypt \p len bytes.
	///
	/// \param  alg         The algorithm ID.
	/// \param  len         The length of data to encrypt.
	/// \param  finalBlock  True if this is the final block of data, or false if not.
	/// 
	/// \returns  The size of the buffer in bytes needed to encrypt \p len bytes.
	std::size_t needed(std::size_t alg, std::size_t len, bool finalBlock = true) const;

	/// \brief  Encrypts a block of memory.
	///
	/// \param  alg         The algorithm ID.
	/// \param  buf         The buffer to encrypt.
	/// \param  len         The length of data to encrypt.
	/// \param  buflen      The size of the buffer.
	/// \param  finalBlock  True if this is the final block of data, or false if not.
	/// 
	/// \returns  The size of the encrypted data in bytes.
	std::size_t encrypt(std::size_t alg, BYTE *buf, std::size_t len, std::size_t buflen, bool finalBlock = true) const;

	/// \brief  Decrypts a block of memory.
	///
	/// \param  alg         The algorithm ID.
	/// \param  buf         The buffer to decrypt.
	/// \param  len         The length of data to decrypt.
	/// \param  finalBlock  True if this is the final block of data, or false if not.
	/// 
	/// \returns  The size of the decrypted data in bytes.
	std::size_t decrypt(std::size_t alg, BYTE *buf, std::size_t len, bool finalBlock = true) const;


private:
	hex::ICryptographyAlgorithm& GetAlgorithm(std::size_t alg) const;

	hex::IDialogProvider& _dialogProvider;
	std::vector<std::unique_ptr<hex::ICryptographyProvider>> _providers;
};
