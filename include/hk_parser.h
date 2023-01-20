//
// The Hook Programming Language
// hk_parser.h
//

#ifndef HK_PARSER_H
#define HK_PARSER_H

#include "hk_ast.h"
#include "hk_string.h"

hk_ast_node_t *hk_parse(hk_string_t *file, hk_string_t *source);

#endif // HK_PARSER_H
