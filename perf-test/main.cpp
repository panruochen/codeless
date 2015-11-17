#include "msgfmt.h"
#include "FileWriter.h"
#include "GlobalVars.h"
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#define ALOG(fmt,args...)  printf(fmt,##args)

#define ROUND_UP(a,b)  (((a) + (b) - 1) & ~((b) - 1))

int main(int argc, const char *argv[])
{
	char *__runtime_dir, *runtime_dir, *server_addr;
	const char *datfile = argv[1];
	FILE *fp;
	char *buf;
	size_t bufsz = 8192;
	DsFileWriter *fw;

	if(argc < 3) {
		ALOG("require 3 args\n");
		exit(-EINVAL);
	}

	gvar_sm = ipsc_open("/var/tmp/SM0");
	if(!gvar_sm) {
		ALOG("Cannot open shared mem\n");
		exit(-ENOMEM);
	}

	__runtime_dir = strdup(argv[2]);
	server_addr = strdup(argv[2]);
	runtime_dir = dirname(__runtime_dir);

	fw = new DsFileWriter(MSGT_CV);
    if( fw->Connect(runtime_dir, server_addr) < 0 ) {
		ALOG("Can not connect to %s, errno=%d (%s)\n", server_addr, errno, strerror(errno));
		exit(EINVAL);
	}
	buf = (char *) malloc(bufsz);
	fp = fopen(datfile, "rb");
	while(1) {
		uint32_t msgsz;
		ssize_t ret;

		ret = fread(&msgsz, 1, sizeof(msgsz), fp);
		if(ret < (ssize_t)sizeof(msgsz))
			break;

		if(msgsz > bufsz) {
			buf = (char *)realloc(buf, ROUND_UP(msgsz,4096));
			bufsz = ROUND_UP(msgsz,4096);
		}

		fread(buf, 1, msgsz, fp);
		if( fw->Write(buf, msgsz) < 0 ) {
			ALOG("Write failed\n");
			exit(-EINVAL);
		}

	}

	ipsc_add_uint64(gvar_sm, 0, gvar_write_time);
	ipsc_add_uint64(gvar_sm, 1, gvar_write_length);
	ipsc_add_uint32(gvar_sm, 0, gvar_write_count);
	ipsc_close(gvar_sm);

	ALOG("Max write time %lu for %lu bytes\n", gvar_maxw.duration, gvar_maxw.length);
	ALOG("%06u exits\n", getpid());
	return 0;
}
