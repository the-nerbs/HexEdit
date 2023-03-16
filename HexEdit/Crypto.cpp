// Crypto.cpp : implements the cryptography class
//
// Copyright (c) 2015 by Andrew W. Phillips
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it and I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include "stdafx.h"
#include "crypto.h"

#include "Cryptography/ICryptographyProvider.h"
#include "Cryptography/windows/AdvapiCryptographyProvider.h"
#include "Services/DialogProvider.h"

#include <numeric>
#include <stdexcept>

static hex::DialogProvider DefaultProvider;


CCrypto::CCrypto() :
	CCrypto{ DefaultProvider }
{ }

CCrypto::CCrypto(hex::IDialogProvider& dialogProvider) :
	_dialogProvider{ dialogProvider }
{
	// TODO(6.0+): push this up to another level? (...)
	// If I can manage it, I'd like any platform-specific initialization to be in one-ish place
	// rather than spread all across the code base. This is probably a good idea if/when I setup
	// a provider for the Crypto++ algorithm implementations.
	//
	// If this is done, the tests against CCrypto should also be updated to use a test
	// provider/algorithm so it doesn't rely on the Windows-specific provider which can
	// change from under us here.
	RegisterProvider<hex::AdvapiCryptographyProvider>();
}


void CCrypto::RegisterProvider(std::unique_ptr<hex::ICryptographyProvider> provider)
{
	if (provider)
	{
		_providers.emplace_back(std::move(provider));
	}
}


std::size_t CCrypto::GetNum() const
{
	return std::accumulate(
		begin(_providers), end(_providers),
		0u,
		[&](std::size_t x, const auto& p) { return x + p->AlgorithmCount(); }
	);
}

const char *CCrypto::GetName(std::size_t alg) const
{
	hex::ICryptographyAlgorithm& algorithm = GetAlgorithm(alg);
	return algorithm.Name();
}

void CCrypto::SetPassword(std::size_t alg, const char *password /*=nullptr*/)
{
	hex::ICryptographyAlgorithm& algorithm = GetAlgorithm(alg);
	try
	{
		algorithm.SetPassword(password);
	}
	catch (const std::exception& ex)
	{
		_dialogProvider.ShowMessageBox(ex.what());
	}
}

bool CCrypto::NeedsPassword(std::size_t alg) const
{
	hex::ICryptographyAlgorithm& algorithm = GetAlgorithm(alg);
	return algorithm.NeedsPassword();
}

int CCrypto::GetBlockLength(std::size_t alg) const
{
	hex::ICryptographyAlgorithm& algorithm = GetAlgorithm(alg);
	return algorithm.BlockLength();
}

int CCrypto::GetKeyLength(std::size_t alg) const
{
	hex::ICryptographyAlgorithm& algorithm = GetAlgorithm(alg);
	return algorithm.KeyLength();
}

std::size_t CCrypto::needed(std::size_t alg, std::size_t len, bool finalBlock /*=true*/) const
{
	hex::ICryptographyAlgorithm& algorithm = GetAlgorithm(alg);

	try
	{
		return algorithm.EncryptedSize(len, finalBlock);
	}
	catch (const std::exception& ex)
	{
		_dialogProvider.ShowMessageBox(ex.what());
		return ~0u;
	}
}

std::size_t CCrypto::encrypt(std::size_t alg, BYTE *buf, std::size_t len, std::size_t buflen, bool finalBlock /*=true*/) const
{
	hex::ICryptographyAlgorithm& algorithm = GetAlgorithm(alg);
	try
	{
		return algorithm.Encrypt(buf, len, buflen, finalBlock);
	}
	catch (const std::exception& ex)
	{
		_dialogProvider.ShowMessageBox(ex.what());
		return ~0u;
	}
}

std::size_t CCrypto::decrypt(std::size_t alg, BYTE *buf, std::size_t len, bool finalBlock /*=true*/) const
{
	hex::ICryptographyAlgorithm& algorithm = GetAlgorithm(alg);
	try
	{
		return algorithm.Decrypt(buf, len, finalBlock);
	}
	catch (const std::exception& ex)
	{
		_dialogProvider.ShowMessageBox(ex.what());
		return ~0u;
	}
}

hex::ICryptographyAlgorithm& CCrypto::GetAlgorithm(std::size_t alg) const
{
	for (const auto& provider : _providers)
	{
		std::size_t count = provider->AlgorithmCount();

		if (alg < count)
		{
			return provider->GetAlgorithm(alg);
		}

		alg -= count;
	}

	throw std::range_error{ "Algorithm ID is not valid." };
}
