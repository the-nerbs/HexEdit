// srecord.cpp - implements CReadSRecord and CWriteSRecord classes that
//               handle files containing Motorola S-Records
//
// Copyright (c) 2015 by Andrew W. Phillips
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it and I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include "stdafx.h"
#include "hexedit.h"
#include "misc.h"
#include "srecord.h"

// Sizes of address fields for S-Records (S0 to S9)
static size_t addr_size[10] = { 2, 2, 3, 4, -1, 2, -1, 4, 3, 2 };

CWriteSRecord::CWriteSRecord(const char *filename,
							 unsigned long base_addr /*= 0L*/,
							 int stype /*= 3*/,
							 size_t reclen /*= 32*/) :
	file_{ std::make_unique<CStdioFile>()},
	stype_{ stype },
	addr_{ base_addr },
	reclen_{ reclen },
	error_{},
	recs_out_{ 0 }
{
	CFileException fe;                      // Stores file exception info

	// Open the file
	if (!file_->Open(filename,
		CFile::modeCreate|CFile::modeWrite|CFile::shareExclusive|CFile::typeText,
					  &fe))
	{
		error_ = ::FileErrorMessage(&fe, CFile::modeWrite);
		return;
	}

	// Write S0 record
	put_rec(0, 0L, "HDR", 3);
}

CWriteSRecord::CWriteSRecord(std::unique_ptr<CFile> stream, unsigned long base_addr, int stype, size_t reclen) :
	file_{ std::move(stream) },
	stype_{ stype },
	addr_{ base_addr },
	reclen_{ reclen },
	error_{},
	recs_out_{ 0 }
{
	// Write S0 record
	put_rec(0, 0L, "HDR", 3);
}

CWriteSRecord::~CWriteSRecord()
{
	put_rec(5, recs_out_, "", 0);       // For S5 the number of records read is stored in address field
	try
	{
		file_->Close();
	}
	catch (CFileException *pfe)
	{
		error_ = ::FileErrorMessage(pfe, CFile::modeWrite);
		pfe->Delete();
	}
}

void CWriteSRecord::Put(const void *data, size_t len, unsigned long address /* = UINT_MAX */)
{
	if (address == UINT_MAX)
		address = addr_;
	addr_ = address + len;   // Save next address after these record(s)

	char *pp = (char *)data;

	while (len > 0)
	{
		if (len > reclen_)
		{
			put_rec(stype_, address, pp, reclen_);
			address += reclen_;
			pp += reclen_;
			len -= reclen_;
		}
		else
		{
			put_rec(stype_, address, pp, size_t(len));
			len = 0;
		}
	}
}

// Output an S-Record, where stype is the S record type (0 to 9), addr is the 16/24/32-bit address
// data points to the data bytes and len is the number of data bytes.  It calculates the address
// size based on the record type and also works out and stores the byte count and checksum.
void CWriteSRecord::put_rec(int stype, unsigned long addr, void *data, size_t len)
{
	ASSERT(stype >= 0 && stype <= 9);

	char buffer[520];
	int checksum;
	int byte_count = addr_size[stype] + len + 1;    // size of address, data, checksum
	char *pp = buffer;

	*pp++ = 'S';
	*pp++ = stype + '0';
	checksum = put_hex(pp, byte_count, 1);
	pp += 2;
	checksum += put_hex(pp, addr, addr_size[stype]);
	pp += addr_size[stype]*2;

	for (size_t ii = 0; ii < len; ++ii)
	{
		checksum += put_hex(pp, *((char*)data+ii), 1);
		pp += 2;
	}
	put_hex(pp, 255-(checksum&0xFF), 1);
	pp += 2;
	*pp++ = '\n';
	*pp = '\0';

	if (stype >= 1 && stype <= 3)
		++recs_out_;

	try
	{
		UINT writeLen = pp - buffer;
		file_->Write(buffer, writeLen);
	}
	catch (CFileException *pfe)
	{
		error_ = ::FileErrorMessage(pfe, CFile::modeWrite);
		pfe->Delete();
	}
}

// Output from 1 to 4 bytes converting into (from 2 to 8) hex digits
// Also calculates the checksum of digits output and returns it.
// Note that 'bytes' is the number of data bytes 1 byte=2 hex digits
int CWriteSRecord::put_hex(char *pstart, unsigned long val, int bytes)
{
	static char hex_digit[18] = "0123456789ABCDEF?";

	unsigned long tt = val;

	for (char *pp = pstart + bytes*2; pp > pstart; pp--)
	{
		*(pp-1) = hex_digit[tt&0x0F];
		tt >>= 4;
	}

	// Work out sum of bytes
	int retval = 0;
	tt = val;
	while (bytes--)
	{
		retval += tt&0xFF;
		tt >>= 8;
	}
	return retval;
}

