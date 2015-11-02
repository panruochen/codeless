#include "cc_string.h"
#include "File.h"

////////////////////////////////////////////////////////////////
//  Real File Class
////////////////////////////////////////////////////////////////

RealFile::RealFile()
{
	fp = NULL;
}

RealFile::~RealFile()
{
	this->Close();
}

int RealFile::ReadChar()
{
	return ::fgetc(fp);
}

bool RealFile::Open()
{
	fp   = fopen(name.c_str(), "rb");
	line = 0;
	return fp != NULL;
}

void RealFile::Close()
{
	if(fp) {
		fclose(fp);
		fp = NULL;
	}
}

void RealFile::SetFileName(const CC_STRING& name)
{
	this->name = name;
}

bool RealFile::Open(const CC_STRING& filename)
{
	fp    = fopen(filename.c_str(), "rb");
	name  = filename;
	line  = 0;
	return fp != NULL;
}

////////////////////////////////////////////////////////////////
//  Memory File Class
////////////////////////////////////////////////////////////////


int MemFile::ReadChar()
{
	if(pos < contents.size())
		return contents[pos++];
	return EOF;
}

MemFile::MemFile()
{
	pos = 0;
}

void MemFile::SetFileName(const CC_STRING& name)
{
	this->name = name;
	this->line = 0;
}

bool MemFile::Open()
{
	pos = 0;
	return true;
}

void MemFile::Close()
{
}

MemFile::~MemFile()
{
}

MemFile& MemFile::operator << (const char *s)
{
	contents += s;
	return *this;
}

MemFile& MemFile::operator << (const CC_STRING& s)
{
	contents += s;
	return *this;
}

MemFile& MemFile::operator << (const char c)
{
	contents += c;
	return *this;
}

size_t MemFile::Size()
{
	return contents.size();
}


