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
							   size_t reclen /*= 32*/) :
	file_{ std::make_unique<CStdioFile>() },
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
	}
}

CWriteIntelHex::CWriteIntelHex(std::unique_ptr<CFile> stream,
							   unsigned long base_addr,
							   size_t reclen) :
	file_{ std::move(stream) },
	addr_{ base_addr },
	reclen_{ reclen },
	error_{},
	recs_out_{ 0 }
{ }

CWriteIntelHex::~CWriteIntelHex()
{
	// for some reason, MFC's CFile streams have a discrete Open method, but
	// no way to query if the stream is open from a CFile reference... This
	// means we have to do a bit of ugly runtime type checking to see if we're
	// in a valid state to write out the S5 record here. On the off chance an
	// unrecognized stream type is used, assume it is open.
	bool isStreamOpen = true;

	if (file_->IsKindOf(RUNTIME_CLASS(CStdioFile)))
	{
		isStreamOpen = static_cast<CStdioFile*>(file_.get())->m_pStream;
	}

	// since we don't have a prologue record, file_->GetLength can still be 0 here,
	// so the check that SREC writer makes for CMemFile cannot be done here.

	if (isStreamOpen)
	{
		put_rec(1, 0, "", 0);       // Output the EOF (01) record
		try
		{
			file_->Close();
		}
		catch (CFileException* pfe)
		{
			error_ = ::FileErrorMessage(pfe, CFile::modeWrite);
			pfe->Delete();
		}
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
