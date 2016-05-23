#ifndef __FS_LIBRARY_H
#define __FS_LIBRARY_H

#include "cc_string.h"

class FileWriter {
protected:
	CC_STRING filename;
	int id;
public:
	virtual ssize_t Write(const void *buf, size_t count) = 0;
	virtual ~FileWriter();
};

/* A File Writer via local os file api functions */
class LocalFileWriter : public FileWriter {
public:
	virtual ssize_t Write(const void *buf, size_t count);
	LocalFileWriter(int mtype, const CC_STRING&);
	LocalFileWriter(int mtype, const char *);
};

/* A Write Writer via talking to a domain socket server */
class IpcFileWriter : public FileWriter {
	static const char *svr_addr;
	static const char *cli_addr;
	static int svr_fd;
	static unsigned int ref_count;
public:
	int Connect(const char *rtdir, const char *saddr);
	virtual ssize_t Write(const void *buf, size_t count);
	IpcFileWriter(int mtype);
	virtual ~IpcFileWriter();
};

#endif
