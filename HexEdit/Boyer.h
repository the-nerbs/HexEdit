// boyer.h : boyer class for fast searching
//
// Copyright (c) 2016 by Andrew W. Phillips
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it and I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

/////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <memory>

class boyer
{
public:
	// Construction
	boyer(const std::uint8_t* pat, std::size_t len, const std::uint8_t* mask);
	boyer(const boyer &);
	boyer &operator=(const boyer &);

	// Attributes
	std::size_t length() { return pattern_len_; }
	const std::uint8_t* pattern() { return pattern_.get(); }
	const std::uint8_t* mask() { return mask_.get(); }

	// Operations
	std::uint8_t* findforw(std::uint8_t* pp, std::size_t len,
						BOOL icase, int tt,
						BOOL wholeword, BOOL alpha_before, BOOL alpha_after,
						int alignment, int offset, __int64 base_addr, __int64 address) const;
	std::uint8_t* findback(std::uint8_t* pp, std::size_t len,
							BOOL icase, int tt,
							BOOL wholeword, BOOL alpha_before, BOOL alpha_after,
							int alignment, int offset, __int64 base_addr, __int64 address) const;

private:
	std::uint8_t* mask_find(std::uint8_t* pp, std::size_t len,
							 BOOL icase, int tt,
							 BOOL wholeword, BOOL alpha_before, BOOL alpha_after,
							 int alignment, int offset, __int64 base_addr, __int64 address) const;
	std::uint8_t* mask_findback(std::uint8_t* pp, std::size_t len,
								 BOOL icase, int tt,
								 BOOL wholeword, BOOL alpha_before, BOOL alpha_after,
								 int alignment, int offset, __int64 base_addr, __int64 address) const;

	std::unique_ptr<std::uint8_t[]> pattern_;	// Current search bytes
	std::unique_ptr<std::uint8_t[]> mask_;		// Which bits are used (all if NULL)
	std::size_t pattern_len_;					// Length of search bytes
	std::size_t fskip_[256];					// Use internally in forward searches
	std::size_t bskip_[256];					// Use internally in backward searches
};
