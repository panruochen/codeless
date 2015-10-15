#ifndef __GFILE_H
#define __GFILE_H

#include <stdio.h>
#include "cc_string.h"

class File {
	friend class Parser;
protected:
	CC_STRING name;
	size_t line;
	int offset;
public:
	virtual int  ReadChar() = 0;
	virtual bool Open() = 0;
	virtual void Close() = 0;
	virtual ~File() {} ;
};

class RealFile : public File {
protected:
	FILE *fp;

	virtual inline int ReadChar();

public:
	virtual inline bool Open();
	virtual inline void Close();
	virtual inline ~RealFile();
	inline RealFile();

	inline void SetFileName(const CC_STRING& name);
	inline bool Open(const CC_STRING& filename);
};

class MemFile : public File {
protected:
	size_t     pos;
//	CC_STRING  contents;
	virtual inline int ReadChar();

public:
	CC_STRING  contents;
	inline MemFile();
	virtual inline ~MemFile();

	inline void SetFileName(const CC_STRING& name);
	virtual inline bool Open();
	virtual inline void Close();

	inline MemFile& operator << (const char *s);
	inline MemFile& operator << (const CC_STRING& s);
	inline MemFile& operator << (const char c);

	inline size_t Size();
};

#include "File.inl"

#endif


