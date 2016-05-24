#include "base.h"
#include "cc_string.h"
#include "cc_array.h"
#include "GlobalVars.h"

const char*  dv_current_file;
size_t       dv_current_line;
uint8_t      gv_preprocess_mode = true;
bool         rtm_expand_macros; //= true;

FileWriter  *gvar_file_writers[VCH_MAX];

InterProcessSharedCounter   *gvar_sm;
uint64_t gvar_write_time, gvar_write_length;
uint32_t gvar_write_count;

WriteStat gvar_maxw;
