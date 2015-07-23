#ifndef __GLOBAL_VAR_H
#define __GLOBAL_VAR_H

extern CC_ARRAY<CC_STRING>  dx_traced_macros, dx_traced_lines;

extern uint8_t       gv_preprocess_mode;
extern bool          rtm_expand_macros;
extern const char*   dv_current_file;
extern size_t        dv_current_line;

#endif

