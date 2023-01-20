//
// The Hook Programming Language
// hk_ast.h
//

#ifndef HK_AST_H
#define HK_AST_H

#include <stdint.h>

typedef struct
{
  int32_t type;
} hk_ast_node_t;

void hk_ast_free(hk_ast_node_t *ast);
void hk_ast_print(hk_ast_node_t *ast);

#endif // HK_AST_H
