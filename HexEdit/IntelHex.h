// IntelHex.h - classes to read/write Intel hex files

#ifndef INTELHEX_INCLUDED
#define INTELHEX_INCLUDED  1

#include <memory>

// Can be used to create an Intel hex file
class CWriteIntelHex
{
public:
	CWriteIntelHex(const char *filename, unsigned long base_addr = 0L, size_t reclen = 32);
	~CWriteIntelHex();
	void Put(const void *data, size_t len);
	CString Error() const { return error_; }

private:
	void put_rec(int stype, unsigned long addr, void *data, size_t len);
	int put_hex(char *pstart, unsigned long val, int bytes);

	CStdioFile file_;
	unsigned long addr_;                // Address of next record to write
	size_t reclen_;                     // Preferred output record size
	CString error_;                     // Error message for last error (if any)
	int recs_out_;                      // Number of records output so far
};

#endif
