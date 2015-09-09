#include "codeless.h"

const char*  dv_current_file;
size_t       dv_current_line;
uint8_t      gv_preprocess_mode = true;
bool         rtm_expand_macros; //= true;
CC_ARRAY<CC_STRING>  dx_traced_macros;
CC_ARRAY<CC_STRING>  dx_traced_lines;

