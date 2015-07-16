#ifndef __GFILE_H
#define __GFILE_H

#include <stdio.h>
#include "cc_string.h"

class CFile {
	friend class CYcpp;
protected:
	CC_STRING  name;
	size_t     line;
public:
	virtual int  ReadChar() = 0;
	virtual bool Open() = 0;
	virtual void Close() = 0;
	virtual ~CFile() {} ;
};

class CRealFile : public CFile {
protected:
	FILE *fp;

	virtual inline int ReadChar();
	
public:
	virtual inline bool Open();
	virtual inline void Close();
	virtual inline ~CRealFile();
	inline CRealFile(); 

	inline void SetFileName(const CC_STRING& name);
	inline bool Open(const CC_STRING& filename);
};

class CMemFile : public CFile {
protected:
	size_t     pos;
//	CC_STRING  contents;
	virtual inline int ReadChar();

public:
	CC_STRING  contents;
	inline CMemFile(); 
	virtual inline ~CMemFile();

	inline void SetFileName(const CC_STRING& name); 
	virtual inline bool Open();
	virtual inline void Close();

	inline CMemFile& operator << (const char *s);
	inline CMemFile& operator << (const CC_STRING& s);
	inline CMemFile& operator << (const char c);

	inline size_t Size();
};

#include "file.inl"

#endif


