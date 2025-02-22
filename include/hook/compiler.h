//
// The Hook Programming Language
// compiler.h
//

#ifndef HK_COMPILER_H
#define HK_COMPILER_H

#include <hook/callable.h>

hk_closure_t *hk_compile(hk_string_t *file, hk_string_t *source);

#endif // HK_COMPILER_H
