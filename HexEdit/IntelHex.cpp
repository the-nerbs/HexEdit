// IntelHex.cpp - implements CReadIntelHex and CWriteIntelHex classes that
// can be used to read and write Intel hex format files.
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
#include "IntelHex.h"

CWriteIntelHex::CWriteIntelHex(const char *filename,
							 unsigned long base_addr /*= 0L*/,
							 size_t reclen /*= 32*/)
{
	addr_ = base_addr;                      // Init addr_ to base address
	reclen_ = reclen;
	recs_out_ = 0;                          // Init output record count

	CFileException fe;                      // Stores file exception info

	// Open the file
	if (!file_.Open(filename,
		CFile::modeCreate|CFile::modeWrite|CFile::shareExclusive|CFile::typeText,
					  &fe))
	{
		error_ = ::FileErrorMessage(&fe, CFile::modeWrite);
		return;
	}
}

CWriteIntelHex::~CWriteIntelHex()
{
	put_rec(1, 0, "", 0);       // Ouput the EOF (01) record
	try
	{
		file_.Close();
	}
	catch (CFileException *pfe)
	{
		error_ = ::FileErrorMessage(pfe, CFile::modeWrite);
		pfe->Delete();
	}
}

void CWriteIntelHex::Put(const void *data, size_t len)
{
	char *pp = (char *)data;

	while (len > 0)
	{
		if (len > reclen_)
		{
			put_rec(0, addr_, pp, reclen_);
			addr_ += reclen_;
			pp += reclen_;
			len -= reclen_;
		}
		else
		{
			put_rec(0, addr_, pp, size_t(len));
			addr_ += len;
			len = 0;
		}
	}
}

// Output a record, where stype is 0 for data or 1 for EOF record, addr is the 16-bit address
// data points to the data bytes and len is the number of data bytes.
// It works out and stores the byte count and checksum.
void CWriteIntelHex::put_rec(int stype, unsigned long addr, void *data, size_t len)
{
	ASSERT(stype == 0 || stype == 1);  // We only know how to do data and EOF records
	ASSERT(len < 256);

	char buffer[520];
	int checksum = 0;
	char *pp = buffer;

	*pp++ = ':';
	checksum += put_hex(pp, len, 1);
	pp += 2;
	checksum += put_hex(pp, addr, 2);
	pp += 4;
	checksum += put_hex(pp, stype, 1);
	pp += 2;

	for (size_t ii = 0; ii < len; ++ii)
	{
		checksum += put_hex(pp, *((char*)data+ii), 1);
		pp += 2;
	}
	put_hex(pp, (256-checksum)&0xFF, 1);
	pp += 2;
	*pp++ = '\n';
	*pp = '\0';

	if (stype == 0)
		++recs_out_;

	try
	{
		file_.WriteString(buffer);
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
int CWriteIntelHex::put_hex(char *pstart, unsigned long val, int bytes)
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
