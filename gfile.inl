#include "cc_string.h"
#include "gfile.h"

////////////////////////////////////////////////////////////////
//  Real File Class
////////////////////////////////////////////////////////////////

CRealFile::CRealFile()
{
	fp = NULL;
}

CRealFile::~CRealFile()
{
	this->Close();
}

int CRealFile::ReadChar()
{
	return ::fgetc(fp);
}

bool CRealFile::Open()
{
	fp   = fopen(name.c_str(), "rb");
	line = 0;
	return fp != NULL;
}

void CRealFile::Close()
{
	if(fp) {
		fclose(fp);
		fp = NULL;
	}
}

void CRealFile::SetFileName(const CC_STRING& name)
{
	this->name = name;
}

bool CRealFile::Open(const CC_STRING& filename)
{
	fp    = fopen(filename.c_str(), "rb");
	name  = filename;
	line  = 0;
	return fp != NULL;
}

////////////////////////////////////////////////////////////////
//  Memory File Class
////////////////////////////////////////////////////////////////


int CMemFile::ReadChar()
{
	if(pos < contents.size())
		return contents[pos++];
	return EOF;
}

CMemFile::CMemFile()
{
	pos = 0;
}

void CMemFile::SetFileName(const CC_STRING& name)
{
	this->name = name;
	this->line = 0;
}

bool CMemFile::Open()
{
	pos = 0;
	return true;
}

void CMemFile::Close()
{
}

CMemFile::~CMemFile()
{
}

CMemFile& CMemFile::operator << (const char *s)
{
	contents += s;
	return *this;
}

CMemFile& CMemFile::operator << (const CC_STRING& s)
{
	contents += s;
	return *this;
}

CMemFile& CMemFile::operator << (const char c)
{
	contents += c;
	return *this;
}

size_t CMemFile::Size()
{
	return contents.size();
}


