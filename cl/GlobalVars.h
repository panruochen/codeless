#ifndef __GLOBAL_VAR_H
#define __GLOBAL_VAR_H

#include "datagram.h"
#include "FileWriter.h"
#include "ip_sc.h"

extern uint8_t       gvar_preprocess_mode;
extern bool          gvar_expand_macros;
extern const char*   dv_current_file;
extern size_t        dv_current_line;
extern FileWriter   *gvar_file_writers[VCH_MAX];
extern InterProcessSharedCounter    *gvar_sm;

extern uint64_t gvar_write_time, gvar_write_length;
extern uint32_t gvar_write_count;

struct WriteStat {
	uint64_t duration;
	uint64_t length;
};
extern WriteStat gvar_maxw;

#endif

