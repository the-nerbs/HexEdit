// srecord.h - classes to read/write Motorola S-record files

#ifndef SRECORD_INCLUDED
#define SRECORD_INCLUDED

#include <memory>

// Writes Motorola S-records to a file from data in memory
class CWriteSRecord
{
public:
	CWriteSRecord(const char *filename, unsigned long base_addr = 0L, int stype = 3, size_t reclen = 32);
	CWriteSRecord(std::unique_ptr<CFile> stream, unsigned long base_addr = 0L, int stype = 3, size_t reclen = 32);
	~CWriteSRecord();
	void Put(const void *data, size_t len, unsigned long address = UINT_MAX);
	CString Error() const { return error_; }

private:
	void put_rec(int stype, unsigned long addr, void *data, size_t len);
	int put_hex(char *pstart, unsigned long val, int bytes);

	std::unique_ptr<CFile> file_;
	int stype_;                         // preferred record type: 1 = S1, 2 = S2, 3 = S3
	unsigned long addr_;                // Address of next S record to write
	size_t reclen_;                     // Preferred output record size
	CString error_;                     // Error message for last error (if any)
	int recs_out_;                      // Number of S1/S2/S3 records output so far
};

#endif
