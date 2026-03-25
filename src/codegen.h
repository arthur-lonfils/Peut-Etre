#ifndef PE_CODEGEN_H
#define PE_CODEGEN_H

#include "parser.h"
#include <stdio.h>

/* Emit generated code for the given AST to the output stream.
 * runtime_src: the content of the chaos runtime to embed (C header or TS module).
 * no_chaos: if nonzero, disable chaos engine in generated code. */
void codegen_c_emit(FILE *out, AstNode *program, const char *runtime_src,
                    int no_chaos, unsigned int seed, int has_seed);

void codegen_ts_emit(FILE *out, AstNode *program, const char *runtime_src,
                     int no_chaos, unsigned int seed, int has_seed);

#endif
