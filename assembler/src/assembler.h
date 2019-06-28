#ifndef _ASSEMBLER_H_
#define _ASSEMBLER_H_

extern char err_str[1000];
_Bool assemble(const char* fn_in, const char* fn_out, const char* fn_dmp);
#endif