#ifndef __GLOBAL_VAR_H
#define __GLOBAL_VAR_H

#include "msgfmt.h"
#include "FileWriter.h"

extern uint8_t       gv_preprocess_mode;
extern bool          rtm_expand_macros;
extern const char*   dv_current_file;
extern size_t        dv_current_line;
extern FileWriter   *gvar_file_writers[MSGT_MAX];

#endif

