//
// The Hook Programming Language
// hk_parser.c
//

#include "hk_parser.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include "hk_scanner.h"
#include "hk_builtin.h"

#define MAX_VARIABLES UINT8_MAX
#define MAX_BREAKS    UINT8_MAX

#define SYNTAX_NONE      0x00
#define SYNTAX_ASSIGN    0x01
#define SYNTAX_CALL      0x02
#define SYNTAX_SUBSCRIPT 0x03

#define match(s, t) ((s)->token.type == (t))

#define consume(p, t) do \
  { \
    scanner_t *scan = (p)->scan; \
    if (!match(scan, t)) \
      syntax_error_unexpected(p); \
    scanner_next_token(scan); \
  } while(0)

#define add_placeholder(c) do \
  { \
    ++(c)->next_index; \
  } while (0)

typedef struct
{
  bool is_local;
  int32_t depth;
  uint8_t index;
  int32_t length;
  char *start;
  bool is_mutable;
} variable_t;

typedef struct loop
{
  struct loop *parent;
  int32_t scope_depth;
  uint16_t jump;
  int32_t num_offsets;
  int32_t offsets[MAX_BREAKS];
} loop_t;

typedef struct parser
{
  struct parser *parent;
  scanner_t *scan;
  int32_t scope_depth;
  int32_t num_variables;
  uint8_t next_index;
  variable_t variables[MAX_VARIABLES];
  loop_t *loop;
  hk_function_t *fn;
} parser_t;

static inline void syntax_error(const char *function, const char *file, int32_t line,
  int32_t col, const char *fmt, ...);
static inline void syntax_error_unexpected(parser_t *parser);
static inline void push_scope(parser_t *parser);
static inline void pop_scope(parser_t *parser);
static inline int32_t discard_variables(parser_t *parser);
static inline bool variable_match(token_t *tk, variable_t *var);
static inline void add_local(parser_t *parser, token_t *tk, bool is_mutable);
static inline void add_variable(parser_t *parser, bool is_local, uint8_t index, token_t *tk,
  bool is_mutable);
static inline void define_local(parser_t *parser, token_t *tk, bool is_mutable);
static inline variable_t resolve_variable(parser_t *parser, token_t *tk);
static inline variable_t *lookup_variable(parser_t *parser, token_t *tk);
static inline bool nonlocal_exists(parser_t *parser, token_t *tk);
static inline void start_loop(parser_t *parser, loop_t *loop);
static inline void end_loop(parser_t *parser);
static inline void parser_init(parser_t *parser, parser_t *parent, scanner_t *scan,
  hk_string_t *name);
static inline void parser_free(parser_t *parser);
static void parse_statement(parser_t *parser);
static void parse_load_module(parser_t *parser);
static void parse_constant_declaration(parser_t *parser);
static void parse_variable_declaration(parser_t *parser);
static void parse_assign_statement(parser_t *parser, token_t *tk);
static int32_t parse_assign(parser_t *parser, int32_t syntax);
static void parse_struct_declaration(parser_t *parser, bool is_anonymous);
static void parse_function_declaration(parser_t *parser, bool is_anonymous);
static void parse_del_statement(parser_t *parser);
static void parse_delete(parser_t *parser);
static void parse_if_statement(parser_t *parser, bool not);
static void parse_match_statement(parser_t *parser);
static void parse_match_statement_member(parser_t *parser);
static void parse_loop_statement(parser_t *parser);
static void parse_while_statement(parser_t *parser, bool not);
static void parse_do_statement(parser_t *parser);
static void parse_for_statement(parser_t *parser);
static void parse_foreach_statement(parser_t *parser);
static void parse_continue_statement(parser_t *parser);
static void parse_break_statement(parser_t *parser);
static void parse_return_statement(parser_t *parser);
static void parse_block(parser_t *parser);
static void parse_expression(parser_t *parser);
static void parse_and_expression(parser_t *parser);
static void parse_equal_expression(parser_t *parser);
static void parse_comp_expression(parser_t *parser);
static void parse_bitwise_or_expression(parser_t *parser);
static void parse_bitwise_xor_expression(parser_t *parser);
static void parse_bitwise_and_expression(parser_t *parser);
static void parse_left_shift_expression(parser_t *parser);
static void parse_right_shift_expression(parser_t *parser);
static void parse_range_expression(parser_t *parser);
static void parse_add_expression(parser_t *parser);
static void parse_mul_expression(parser_t *parser);
static void parse_unary_expression(parser_t *parser);
static void parse_prim_expression(parser_t *parser);
static void parse_array_constructor(parser_t *parser);
static void parse_struct_constructor(parser_t *parser);
static void parse_if_expression(parser_t *parser, bool not);
static void parse_match_expression(parser_t *parser);
static void parse_match_expression_member(parser_t *parser);
static void parse_subscript(parser_t *parser);
static variable_t parse_variable(parser_t *parser, token_t *tk);
static variable_t *parse_nonlocal(parser_t *parser, token_t *tk);

static inline void syntax_error(const char *function, const char *file, int32_t line,
  int32_t col, const char *fmt, ...)
{
  fprintf(stderr, "syntax error: ");
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n  at %s() in %s:%d,%d\n", function, file, line, col);
  exit(EXIT_FAILURE);
}

static inline void syntax_error_unexpected(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  token_t *tk = &scan->token;
  char *function = parser->fn->name->chars;
  char *file = scan->file->chars;
  if (tk->type == TOKEN_EOF)
    syntax_error(function, file, tk->line, tk->col, "unexpected end of file");
  syntax_error(function, file, tk->line, tk->col, "unexpected token `%.*s`",
    tk->length, tk->start);
}

static inline void push_scope(parser_t *parser)
{
  ++parser->scope_depth;
}

static inline void pop_scope(parser_t *parser)
{
  parser->num_variables -= discard_variables(parser);
  --parser->scope_depth;
  int32_t index = parser->num_variables - 1;
  parser->next_index = parser->variables[index].index + 1;
}

static inline int32_t discard_variables(parser_t *parser)
{
  int32_t index = parser->num_variables - 1;
  return parser->num_variables - index - 1;
}

static inline bool variable_match(token_t *tk, variable_t *var)
{
  return tk->length == var->length
    && !memcmp(tk->start, var->start, tk->length);
}

static inline void add_local(parser_t *parser, token_t *tk, bool is_mutable)
{
  uint8_t index = parser->next_index++;
  add_variable(parser, true, index, tk, is_mutable);
}

static inline void add_variable(parser_t *parser, bool is_local, uint8_t index, token_t *tk,
  bool is_mutable)
{
  if (parser->num_variables == MAX_VARIABLES)
    syntax_error(parser->fn->name->chars, parser->scan->file->chars, tk->line, tk->col,
      "a function may only contain %d unique variables", MAX_VARIABLES);
  variable_t *var = &parser->variables[parser->num_variables];
  var->is_local = is_local;
  var->depth = parser->scope_depth;
  var->index = index;
  var->length = tk->length;
  var->start = tk->start;
  var->is_mutable = is_mutable;
  ++parser->num_variables;
}

static inline void define_local(parser_t *parser, token_t *tk, bool is_mutable)
{
  for (int32_t i = parser->num_variables - 1; i > -1; --i)
  {
    variable_t *var = &parser->variables[i];
    if (var->depth < parser->scope_depth)
      break;
    if (variable_match(tk, var))
      syntax_error(parser->fn->name->chars, parser->scan->file->chars, tk->line, tk->col,
        "variable `%.*s` is already defined in this scope", tk->length, tk->start);
  }
  add_local(parser, tk, is_mutable);
}

static inline variable_t resolve_variable(parser_t *parser, token_t *tk)
{
  variable_t *var = lookup_variable(parser, tk);
  if (var)
    return *var;
  if (!nonlocal_exists(parser->parent, tk) && lookup_global(tk->length, tk->start) == -1)
    syntax_error(parser->fn->name->chars, parser->scan->file->chars, tk->line, tk->col,
      "variable `%.*s` is used but not defined", tk->length, tk->start);
  return (variable_t) {.is_mutable = false};
}

static inline variable_t *lookup_variable(parser_t *parser, token_t *tk)
{
  for (int32_t i = parser->num_variables - 1; i > -1; --i)
  {
    variable_t *var = &parser->variables[i];
    if (variable_match(tk, var))
      return var;
  }
  return NULL;
}

static inline bool nonlocal_exists(parser_t *parser, token_t *tk)
{
  if (!parser)
    return false;
  return lookup_variable(parser, tk) || nonlocal_exists(parser->parent, tk);
}

static inline void start_loop(parser_t *parser, loop_t *loop)
{
  loop->parent = parser->loop;
  loop->scope_depth = parser->scope_depth;
  loop->jump = (uint16_t) parser->fn->chunk.code_length;
  loop->num_offsets = 0;
  parser->loop = loop;
}

static inline void end_loop(parser_t *parser)
{
  parser->loop = parser->loop->parent;
}

static inline void parser_init(parser_t *parser, parser_t *parent, scanner_t *scan,
  hk_string_t *name)
{
  parser->parent = parent;
  parser->scan = scan;
  parser->scope_depth = 0;
  parser->num_variables = 0;
  parser->next_index = 1;
  parser->loop = NULL;
  parser->fn = hk_function_new(0, name, scan->file);
}

static inline void parser_free(parser_t *parser)
{
  hk_function_free(parser->fn);
}

static void parse_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  if (match(scan, TOKEN_USE))
  {
    parse_load_module(parser);
    return;
  }
  if (match(scan, TOKEN_VAL))
  {
    parse_constant_declaration(parser);
    consume(parser, TOKEN_SEMICOLON);
    return;
  }
  if (match(scan, TOKEN_MUT))
  {
    parse_variable_declaration(parser);
    consume(parser, TOKEN_SEMICOLON);
    return;
  }
  if (match(scan, TOKEN_NAME))
  {
    token_t tk = scan->token;
    scanner_next_token(scan);
    parse_assign_statement(parser, &tk);
    consume(parser, TOKEN_SEMICOLON);
    return;
  }
  if (match(scan, TOKEN_STRUCT))
  {
    parse_struct_declaration(parser, false);
    return;
  }
  if (match(scan, TOKEN_FN))
  {
    parse_function_declaration(parser, false);
    return;
  }
  if (match(scan, TOKEN_DEL))
  {
    parse_del_statement(parser);
    return;
  }
  if (match(scan, TOKEN_IF))
  {
    parse_if_statement(parser, false);
    return;
  }
  if (match(scan, TOKEN_IFBANG))
  {
    parse_if_statement(parser, true);
    return;
  }
  if (match(scan, TOKEN_MATCH))
  {
    parse_match_statement(parser);
    return;
  }
  if (match(scan, TOKEN_LOOP))
  {
    parse_loop_statement(parser);
    return;
  }
  if (match(scan, TOKEN_WHILE))
  {
    parse_while_statement(parser, false);
    return;
  }
  if (match(scan, TOKEN_WHILEBANG))
  {
    parse_while_statement(parser, true);
    return;
  }
  if (match(scan, TOKEN_DO))
  {
    parse_do_statement(parser);
    return;
  }
  if (match(scan, TOKEN_FOR))
  {
    parse_for_statement(parser);
    return;
  }
  if (match(scan, TOKEN_FOREACH))
  {
    parse_foreach_statement(parser);
    return;
  }
  if (match(scan, TOKEN_CONTINUE))
  {
    parse_continue_statement(parser);
    return;
  }
  if (match(scan, TOKEN_BREAK))
  {
    parse_break_statement(parser);
    return;
  }
  if (match(scan, TOKEN_RETURN))
  {
    parse_return_statement(parser);
    return;
  }
  if (match(scan, TOKEN_LBRACE))
  {
    parse_block(parser);
    return;
  }
  syntax_error_unexpected(parser);
}

static void parse_load_module(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  if (match(scan, TOKEN_NAME))
  {
    token_t tk = scan->token;
    scanner_next_token(scan);
    if (match(scan, TOKEN_AS))
    {
      scanner_next_token(scan);
      if (!match(scan, TOKEN_NAME))
        syntax_error_unexpected(parser);
      tk = scan->token;
      scanner_next_token(scan);
    }
    define_local(parser, &tk, false);
    consume(parser, TOKEN_SEMICOLON);
    return;
  }
  if (match(scan, TOKEN_LBRACE))
  {
    scanner_next_token(scan);
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    token_t tk = scan->token;
    scanner_next_token(scan);
    define_local(parser, &tk, false);
    uint8_t n = 1;
    while (match(scan, TOKEN_COMMA))
    {
      scanner_next_token(scan);
      if (!match(scan, TOKEN_NAME))
        syntax_error_unexpected(parser);
      token_t tk = scan->token;
      scanner_next_token(scan);
      define_local(parser, &tk, false);
      ++n;
    }
    consume(parser, TOKEN_RBRACE);
    consume(parser, TOKEN_FROM);
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    tk = scan->token;
    scanner_next_token(scan);
    consume(parser, TOKEN_SEMICOLON);
    return;
  }
  syntax_error_unexpected(parser);
}

static void parse_constant_declaration(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  if (match(scan, TOKEN_NAME))
  {
    define_local(parser, &scan->token, false);
    scanner_next_token(scan);
    consume(parser, TOKEN_EQ);
    parse_expression(parser);
    return;
  }
  if (match(scan, TOKEN_LBRACKET))
  {
    scanner_next_token(scan);
    if (match(scan, TOKEN_UNDERSCORE))
      add_placeholder(parser);
    else
    {
      if (!match(scan, TOKEN_NAME))
        syntax_error_unexpected(parser);
      define_local(parser, &scan->token, false);
    }
    scanner_next_token(scan);
    uint8_t n = 1;
    while (match(scan, TOKEN_COMMA))
    {
      scanner_next_token(scan);
      if (match(scan, TOKEN_UNDERSCORE))
        add_placeholder(parser);
      else
      {
        if (!match(scan, TOKEN_NAME))
          syntax_error_unexpected(parser);
        define_local(parser, &scan->token, false);
      }
      scanner_next_token(scan);
      ++n;
    }
    consume(parser, TOKEN_RBRACKET);
    consume(parser, TOKEN_EQ);
    parse_expression(parser);
    return;
  }
  if (match(scan, TOKEN_LBRACE))
  {
    scanner_next_token(scan);
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    token_t tk = scan->token;
    scanner_next_token(scan);
    define_local(parser, &tk, false);
    uint8_t n = 1;
    while (match(scan, TOKEN_COMMA))
    {
      scanner_next_token(scan);
      if (!match(scan, TOKEN_NAME))
        syntax_error_unexpected(parser);
      token_t tk = scan->token;
      scanner_next_token(scan);
      define_local(parser, &tk, false);
      ++n;
    }
    consume(parser, TOKEN_RBRACE);
    consume(parser, TOKEN_EQ);
    parse_expression(parser);
    return;
  }
  syntax_error_unexpected(parser);
}

static void parse_variable_declaration(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  if (match(scan, TOKEN_NAME))
  {
    define_local(parser, &scan->token, true);
    scanner_next_token(scan);
    if (match(scan, TOKEN_EQ))
    {
      scanner_next_token(scan);
      parse_expression(parser);
      return;  
    }
    return;
  }
  if (match(scan, TOKEN_LBRACKET))
  {
    scanner_next_token(scan);
    if (match(scan, TOKEN_UNDERSCORE))
      add_placeholder(parser);
    else
    {
      if (!match(scan, TOKEN_NAME))
        syntax_error_unexpected(parser);
      define_local(parser, &scan->token, false);
    }
    scanner_next_token(scan);
    // TODO
    //uint8_t n = 1;
    while (match(scan, TOKEN_COMMA))
    {
      scanner_next_token(scan);
      if (match(scan, TOKEN_UNDERSCORE))
        add_placeholder(parser);
      else
      {
        if (!match(scan, TOKEN_NAME))
          syntax_error_unexpected(parser);
        define_local(parser, &scan->token, false);
      }
      scanner_next_token(scan);
      //++n;
    }
    consume(parser, TOKEN_RBRACKET);
    consume(parser, TOKEN_EQ);
    parse_expression(parser);
    return;
  }
  if (match(scan, TOKEN_LBRACE))
  {
    scanner_next_token(scan);
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    token_t tk = scan->token;
    scanner_next_token(scan);
    define_local(parser, &tk, true);
    // TODO
    //uint8_t n = 1;
    while (match(scan, TOKEN_COMMA))
    {
      scanner_next_token(scan);
      if (!match(scan, TOKEN_NAME))
        syntax_error_unexpected(parser);
      token_t tk = scan->token;
      scanner_next_token(scan);
      define_local(parser, &tk, true);
      //++n;
    }
    consume(parser, TOKEN_RBRACE);
    consume(parser, TOKEN_EQ);
    parse_expression(parser);
    return;
  }
  syntax_error_unexpected(parser);
}

static void parse_assign_statement(parser_t *parser, token_t *tk)
{
  scanner_t *scan = parser->scan;
  hk_function_t *fn = parser->fn;
  variable_t var;
  if (match(scan, TOKEN_EQ))
  {
    var = parse_variable(parser, tk);
    scanner_next_token(scan);
    parse_expression(parser);
    goto end;
  }
  var = parse_variable(parser, tk);
  if (parse_assign(parser, SYNTAX_NONE) == SYNTAX_CALL)
    return;
end:
  if (!var.is_mutable)
    syntax_error(fn->name->chars, scan->file->chars, tk->line, tk->col,
      "cannot assign to immutable variable `%.*s`", tk->length, tk->start);
}

static int32_t parse_assign(parser_t *parser, int32_t syntax)
{
  scanner_t *scan = parser->scan;
  if (match(scan, TOKEN_PIPEEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_CARETEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_AMPEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_LTLTEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_GTGTEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_PLUSEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_MINUSEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_STAREQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_SLASHEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_TILDESLASHEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_PERCENTEQ))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_PLUSPLUS))
  {
    scanner_next_token(scan);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_MINUSMINUS))
  {
    scanner_next_token(scan);
    return SYNTAX_ASSIGN;
  }
  if (match(scan, TOKEN_LBRACKET))
  {
    scanner_next_token(scan);
    if (match(scan, TOKEN_RBRACKET))
    {
      scanner_next_token(scan);
      consume(parser, TOKEN_EQ);
      parse_expression(parser);
      return SYNTAX_ASSIGN;
    }
    parse_expression(parser);
    consume(parser, TOKEN_RBRACKET);
    if (match(scan, TOKEN_EQ))
    {
      scanner_next_token(scan);
      parse_expression(parser);
      return SYNTAX_ASSIGN;
    }
    return parse_assign(parser, SYNTAX_SUBSCRIPT);
  }
  if (match(scan, TOKEN_DOT))
  {
    scanner_next_token(scan);
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    scanner_next_token(scan);
    if (match(scan, TOKEN_EQ))
    {
      scanner_next_token(scan);
      parse_expression(parser);
      return SYNTAX_ASSIGN;
    }
    return parse_assign(parser, SYNTAX_SUBSCRIPT);
  }
  if (match(scan, TOKEN_LPAREN))
  {
    scanner_next_token(scan);
    if (match(scan, TOKEN_RPAREN))
    {
      scanner_next_token(scan);
      return parse_assign(parser, SYNTAX_CALL);
    }
    parse_expression(parser);
    uint8_t num_args = 1;
    while (match(scan, TOKEN_COMMA))
    {
      scanner_next_token(scan);
      parse_expression(parser);
      ++num_args;
    }
    consume(parser, TOKEN_RPAREN);
    return parse_assign(parser, SYNTAX_CALL);
  }
  if (syntax == SYNTAX_NONE || syntax == SYNTAX_SUBSCRIPT)
    syntax_error_unexpected(parser);
  return syntax;
}

static void parse_struct_declaration(parser_t *parser, bool is_anonymous)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  token_t tk;
  if (!is_anonymous)
  {
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    tk = scan->token;
    scanner_next_token(scan);
    define_local(parser, &tk, false);
  }
  consume(parser, TOKEN_LBRACE);
  if (match(scan, TOKEN_RBRACE))
  {
    scanner_next_token(scan);
    return;
  }
  if (!match(scan, TOKEN_STRING) && !match(scan, TOKEN_NAME))
    syntax_error_unexpected(parser);
  tk = scan->token;
  scanner_next_token(scan);
  // TODO
  //uint8_t length = 1;
  while (match(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    if (!match(scan, TOKEN_STRING) && !match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    tk = scan->token;
    scanner_next_token(scan);
    //++length;
  }
  consume(parser, TOKEN_RBRACE);
}

static void parse_function_declaration(parser_t *parser, bool is_anonymous)
{
  scanner_t *scan = parser->scan;
  hk_function_t *fn = parser->fn;
  scanner_next_token(scan);
  parser_t child_parser;
  if (!is_anonymous)
  {
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    token_t tk = scan->token;
    scanner_next_token(scan);
    define_local(parser, &tk, false);
    hk_string_t *name = hk_string_from_chars(tk.length, tk.start);
    parser_init(&child_parser, parser, scan, name);
    add_variable(&child_parser, true, 0, &tk, false);
  }
  else
    parser_init(&child_parser, parser, scan, NULL);
  consume(parser, TOKEN_LPAREN);
  if (match(scan, TOKEN_RPAREN))
  {
    scanner_next_token(scan);
    if (match(scan, TOKEN_ARROW))
    {
      scanner_next_token(scan);
      parse_expression(&child_parser);
      goto end;
    }
    if (!match(scan, TOKEN_LBRACE))
      syntax_error_unexpected(parser);
    parse_block(&child_parser);
    goto end;
  }
  bool is_mutable = false;
  if (match(scan, TOKEN_MUT))
  {
    scanner_next_token(scan);
    is_mutable = true;
  }
  if (!match(scan, TOKEN_NAME))
    syntax_error_unexpected(parser);
  define_local(&child_parser, &scan->token, is_mutable);
  scanner_next_token(scan);
  int32_t arity = 1;
  while (match(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    bool is_mutable = false;
    if (match(scan, TOKEN_MUT))
    {
      scanner_next_token(scan);
      is_mutable = true;
    }
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    define_local(&child_parser, &scan->token, is_mutable);
    scanner_next_token(scan);
    ++arity;
  }
  child_parser.fn->arity = arity;
  consume(parser, TOKEN_RPAREN);
  if (match(scan, TOKEN_ARROW))
  {
    scanner_next_token(scan);
    parse_expression(&child_parser);
    goto end;
  }
  if (!match(scan, TOKEN_LBRACE))
    syntax_error_unexpected(parser);
  parse_block(&child_parser);
end:
  hk_function_add_child(fn, child_parser.fn);
}

static void parse_del_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  hk_function_t *fn = parser->fn;
  scanner_next_token(scan);
  if (!match(scan, TOKEN_NAME))
    syntax_error_unexpected(parser);
  token_t tk = scan->token;
  scanner_next_token(scan);
  variable_t var = resolve_variable(parser, &tk);
  if (!var.is_mutable)
    syntax_error(fn->name->chars, scan->file->chars, tk.line, tk.col,
      "cannot delete element from immutable variable `%.*s`", tk.length, tk.start);
  parse_delete(parser);
}

static void parse_delete(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  if (match(scan, TOKEN_LBRACKET))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    consume(parser, TOKEN_RBRACKET);
    if (match(scan, TOKEN_SEMICOLON))
    {
      scanner_next_token(scan);
      return;
    }
    parse_delete(parser);
    return;
  }
  if (match(scan, TOKEN_DOT))
  {
    scanner_next_token(scan);
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    scanner_next_token(scan);
    parse_delete(parser);
    return;
  }
  syntax_error_unexpected(parser); 
}

static void parse_if_statement(parser_t *parser, bool not)
{
  // TODO
  (void) not;
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  consume(parser, TOKEN_LPAREN);
  parse_expression(parser);
  consume(parser, TOKEN_RPAREN);
  parse_statement(parser);
  if (match(scan, TOKEN_ELSE))
  {
    scanner_next_token(scan);
    parse_statement(parser);
  }
}

static void parse_match_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  consume(parser, TOKEN_LPAREN);
  parse_expression(parser);
  consume(parser, TOKEN_RPAREN);
  consume(parser, TOKEN_LBRACE);
  parse_expression(parser);
  consume(parser, TOKEN_ARROW);
  parse_statement(parser);
  parse_match_statement_member(parser);
}

static void parse_match_statement_member(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  if (match(scan, TOKEN_RBRACE))
  {
    scanner_next_token(scan);
    return;
  }
  if (match(scan, TOKEN_UNDERSCORE))
  {
    scanner_next_token(scan);
    consume(parser, TOKEN_ARROW);
    parse_statement(parser);
    consume(parser, TOKEN_RBRACE);
    return;
  }
  parse_expression(parser);
  consume(parser, TOKEN_ARROW);
  parse_statement(parser);
  parse_match_statement_member(parser);
}

static void parse_loop_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  if (!match(scan, TOKEN_LBRACE))
    syntax_error_unexpected(parser);
  loop_t loop;
  start_loop(parser, &loop);
  parse_statement(parser);
  end_loop(parser);
}

static void parse_while_statement(parser_t *parser, bool not)
{
  // TODO
  (void) not;
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  consume(parser, TOKEN_LPAREN);
  loop_t loop;
  start_loop(parser, &loop);
  parse_expression(parser);
  consume(parser, TOKEN_RPAREN);
  parse_statement(parser);
  end_loop(parser);
}

static void parse_do_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  loop_t loop;
  start_loop(parser, &loop);
  parse_statement(parser);  
  if (match(scan, TOKEN_WHILEBANG))
  {
    scanner_next_token(scan);
  }
  else
    consume(parser, TOKEN_WHILE);
  consume(parser, TOKEN_LPAREN);
  parse_expression(parser);
  consume(parser, TOKEN_RPAREN);
  consume(parser, TOKEN_SEMICOLON);
  end_loop(parser);
}

static void parse_for_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  consume(parser, TOKEN_LPAREN);
  push_scope(parser);
  if (match(scan, TOKEN_SEMICOLON))
    scanner_next_token(scan);
  else
  {
    if (match(scan, TOKEN_VAL))
    {
      parse_constant_declaration(parser);
      consume(parser, TOKEN_SEMICOLON);
    }
    else if (match(scan, TOKEN_MUT))
    {
      parse_variable_declaration(parser);
      consume(parser, TOKEN_SEMICOLON);
    }
    else if (match(scan, TOKEN_NAME))
    {
      token_t tk = scan->token;
      scanner_next_token(scan);
      parse_assign_statement(parser, &tk);
      consume(parser, TOKEN_SEMICOLON);
    }
    else
      syntax_error_unexpected(parser);
  }
  bool missing = match(scan, TOKEN_SEMICOLON);
  if (missing)
    scanner_next_token(scan);
  else
  {
    parse_expression(parser);
    consume(parser, TOKEN_SEMICOLON);
  }
  loop_t loop;
  start_loop(parser, &loop);
  if (match(scan, TOKEN_RPAREN))
    scanner_next_token(scan);
  else
  {
    if (!match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    token_t tk = scan->token;
    scanner_next_token(scan);
    parse_assign_statement(parser, &tk);
    consume(parser, TOKEN_RPAREN);
  }
  parse_statement(parser);
  end_loop(parser);
  pop_scope(parser);
}

static void parse_foreach_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  consume(parser, TOKEN_LPAREN);
  push_scope(parser);
  if (!match(scan, TOKEN_NAME))
    syntax_error_unexpected(parser);
  token_t tk = scan->token;
  scanner_next_token(scan);
  add_local(parser, &tk, false);
  consume(parser, TOKEN_IN);
  parse_expression(parser);
  consume(parser, TOKEN_RPAREN);
  loop_t loop;
  start_loop(parser, &loop);
  parse_statement(parser);
  end_loop(parser);
  pop_scope(parser);
}

static void parse_continue_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  hk_function_t *fn = parser->fn;
  token_t tk = scan->token;
  scanner_next_token(scan);
  if (!parser->loop)
    syntax_error(fn->name->chars, scan->file->chars, tk.line, tk.col,
      "cannot use continue outside of a loop");
  consume(parser, TOKEN_SEMICOLON);
  discard_variables(parser);
}

static void parse_break_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  char *function = parser->fn->name->chars;
  char *file = scan->file->chars;
  token_t tk = scan->token;
  scanner_next_token(scan);
  if (!parser->loop)
    syntax_error(function, file, tk.line, tk.col,
      "cannot use break outside of a loop");
  consume(parser, TOKEN_SEMICOLON);
  discard_variables(parser);
  loop_t *loop = parser->loop;
  if (loop->num_offsets == MAX_BREAKS)
    syntax_error(function, file, tk.line, tk.col,
      "cannot use more than %d breaks", MAX_BREAKS);
}

static void parse_return_statement(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  if (match(scan, TOKEN_SEMICOLON))
  {
    scanner_next_token(scan);
    return;
  }
  parse_expression(parser);
  consume(parser, TOKEN_SEMICOLON);
}

static void parse_block(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  push_scope(parser);
  while (!match(scan, TOKEN_RBRACE))
    parse_statement(parser);
  scanner_next_token(scan);
  pop_scope(parser);
}

static void parse_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_and_expression(parser);
  while (match(scan, TOKEN_PIPEPIPE))
  {
    scanner_next_token(scan);
    parse_and_expression(parser);
  }
}

static void parse_and_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_equal_expression(parser);
  while (match(scan, TOKEN_AMPAMP))
  {
    scanner_next_token(scan);
    parse_equal_expression(parser);
  }
}

static void parse_equal_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_comp_expression(parser);
  for (;;)
  {
    if (match(scan, TOKEN_EQEQ))
    {
      scanner_next_token(scan);
      parse_comp_expression(parser);
      continue;
    }
    if (match(scan, TOKEN_BANGEQ))
    {
      scanner_next_token(scan);
      parse_comp_expression(parser);
      continue;
    }
    break;
  }
}

static void parse_comp_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_bitwise_or_expression(parser);
  for (;;)
  {
    if (match(scan, TOKEN_GT))
    {
      scanner_next_token(scan);
      parse_bitwise_or_expression(parser);
      continue;
    }
    if (match(scan, TOKEN_GTEQ))
    {
      scanner_next_token(scan);
      parse_bitwise_or_expression(parser);
      continue;
    }
    if (match(scan, TOKEN_LT))
    {
      scanner_next_token(scan);
      parse_bitwise_or_expression(parser);
      continue;
    }
    if (match(scan, TOKEN_LTEQ))
    {
      scanner_next_token(scan);
      parse_bitwise_or_expression(parser);
      continue;
    }
    break;
  }
}

static void parse_bitwise_or_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_bitwise_xor_expression(parser);
  while (match(scan, TOKEN_PIPE))
  {
    scanner_next_token(scan);
    parse_bitwise_xor_expression(parser);
  }
}

static void parse_bitwise_xor_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_bitwise_and_expression(parser);
  while (match(scan, TOKEN_CARET))
  {
    scanner_next_token(scan);
    parse_bitwise_and_expression(parser);
  }
}

static void parse_bitwise_and_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_left_shift_expression(parser);
  while (match(scan, TOKEN_AMP))
  {
    scanner_next_token(scan);
    parse_left_shift_expression(parser);
  }
}

static void parse_left_shift_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_right_shift_expression(parser);
  while (match(scan, TOKEN_LTLT))
  {
    scanner_next_token(scan);
    parse_right_shift_expression(parser);
  }
}

static void parse_right_shift_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_range_expression(parser);
  while (match(scan, TOKEN_GTGT))
  {
    scanner_next_token(scan);
    parse_range_expression(parser);
  }
}

static void parse_range_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_add_expression(parser);
  if (match(scan, TOKEN_DOTDOT))
  {
    scanner_next_token(scan);
    parse_add_expression(parser);
  }
}

static void parse_add_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_mul_expression(parser);
  for (;;)
  {
    if (match(scan, TOKEN_PLUS))
    {
      scanner_next_token(scan);
      parse_mul_expression(parser);
      continue;
    }
    if (match(scan, TOKEN_MINUS))
    {
      scanner_next_token(scan);
      parse_mul_expression(parser);
      continue;
    }
    break;
  }
}

static void parse_mul_expression(parser_t *parser)
{
  parse_unary_expression(parser);
  scanner_t *scan = parser->scan;
  for (;;)
  {
    if (match(scan, TOKEN_STAR))
    {
      scanner_next_token(scan);
      parse_unary_expression(parser);
      continue;
    }
    if (match(scan, TOKEN_SLASH))
    {
      scanner_next_token(scan);
      parse_unary_expression(parser);
      continue;
    }
    if (match(scan, TOKEN_TILDESLASH))
    {
      scanner_next_token(scan);
      parse_unary_expression(parser);
      continue;
    }
    if (match(scan, TOKEN_PERCENT))
    {
      scanner_next_token(scan);
      parse_unary_expression(parser);
      continue;
    }
    break;
  }
}

static void parse_unary_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  if (match(scan, TOKEN_MINUS))
  {
    scanner_next_token(scan);
    parse_unary_expression(parser);
    return;
  }
  if (match(scan, TOKEN_BANG))
  {
    scanner_next_token(scan);
    parse_unary_expression(parser);
    return;
  }
  if (match(scan, TOKEN_TILDE))
  {
    scanner_next_token(scan);
    parse_unary_expression(parser);
    return;
  }
  parse_prim_expression(parser);
}

static void parse_prim_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  if (match(scan, TOKEN_NIL))
  {
    scanner_next_token(scan);
    return;
  }
  if (match(scan, TOKEN_FALSE))
  {
    scanner_next_token(scan);
    return;
  }
  if (match(scan, TOKEN_TRUE))
  {
    scanner_next_token(scan);
    return;
  }
  if (match(scan, TOKEN_INT))
  {
    scanner_next_token(scan);
    return;
  }
  if (match(scan, TOKEN_FLOAT))
  {
    scanner_next_token(scan);
    return;
  }
  if (match(scan, TOKEN_STRING))
  {
    scanner_next_token(scan);
    return;
  }
  if (match(scan, TOKEN_LBRACKET))
  {
    parse_array_constructor(parser);
    return;
  }
  if (match(scan, TOKEN_LBRACE))
  {
    parse_struct_constructor(parser);
    return;
  }
  if (match(scan, TOKEN_STRUCT))
  {
    parse_struct_declaration(parser, true);
    return;
  }
  if (match(scan, TOKEN_FN))
  {
    parse_function_declaration(parser, true);
    return;
  }
  if (match(scan, TOKEN_IF))
  {
    parse_if_expression(parser, false);
    return;
  }
  if (match(scan, TOKEN_IFBANG))
  {
    parse_if_expression(parser, true);
    return;
  }
  if (match(scan, TOKEN_MATCH))
  {
    parse_match_expression(parser);
    return;
  }
  if (match(scan, TOKEN_NAME))
  {
    parse_subscript(parser);
    return;
  }
  if (match(scan, TOKEN_LPAREN))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    consume(parser, TOKEN_RPAREN);
    return;
  }
  syntax_error_unexpected(parser);
}

static void parse_array_constructor(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  uint8_t length = 0;
  if (match(scan, TOKEN_RBRACKET))
  {
    scanner_next_token(scan);
    return;
  }
  parse_expression(parser);
  ++length;
  while (match(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    parse_expression(parser);
    ++length;
  }
  consume(parser, TOKEN_RBRACKET);
}

static void parse_struct_constructor(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  if (match(scan, TOKEN_RBRACE))
  {
    scanner_next_token(scan);
    return;
  }
  if (!match(scan, TOKEN_STRING) && !match(scan, TOKEN_NAME))
    syntax_error_unexpected(parser);
  scanner_next_token(scan);
  consume(parser, TOKEN_COLON);
  parse_expression(parser);
  uint8_t length = 1;
  while (match(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    if (!match(scan, TOKEN_STRING) && !match(scan, TOKEN_NAME))
      syntax_error_unexpected(parser);
    scanner_next_token(scan);
    consume(parser, TOKEN_COLON);
    parse_expression(parser);
    ++length;
  }
  consume(parser, TOKEN_RBRACE);
}

static void parse_if_expression(parser_t *parser, bool not)
{
  // TODO
  (void) not;
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  consume(parser, TOKEN_LPAREN);
  parse_expression(parser);
  consume(parser, TOKEN_RPAREN);
  parse_expression(parser);
  consume(parser, TOKEN_ELSE);
  parse_expression(parser);
}

static void parse_match_expression(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  scanner_next_token(scan);
  consume(parser, TOKEN_LPAREN);
  parse_expression(parser);
  consume(parser, TOKEN_RPAREN);
  consume(parser, TOKEN_LBRACE);
  parse_expression(parser);
  consume(parser, TOKEN_ARROW);
  parse_expression(parser);
  if (match(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    if (match(scan, TOKEN_UNDERSCORE))
    {
      scanner_next_token(scan);
      consume(parser, TOKEN_ARROW);
      parse_expression(parser);
      consume(parser, TOKEN_RBRACE);
      return;
    }
    parse_match_expression_member(parser);
    return;
  }
  syntax_error_unexpected(parser);
}

static void parse_match_expression_member(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_expression(parser);
  consume(parser, TOKEN_ARROW);
  parse_expression(parser);
  if (match(scan, TOKEN_COMMA))
  {
    scanner_next_token(scan);
    if (match(scan, TOKEN_UNDERSCORE))
    {
      scanner_next_token(scan);
      consume(parser, TOKEN_ARROW);
      parse_expression(parser);
      consume(parser, TOKEN_RBRACE);
      return;
    }
    parse_match_expression_member(parser);
    return;
  }
  syntax_error_unexpected(parser);
}

static void parse_subscript(parser_t *parser)
{
  scanner_t *scan = parser->scan;
  parse_variable(parser, &scan->token);
  scanner_next_token(scan);
  for (;;)
  {
    if (match(scan, TOKEN_LBRACKET))
    {
      scanner_next_token(scan);
      parse_expression(parser);
      consume(parser, TOKEN_RBRACKET);
      continue;
    }
    if (match(scan, TOKEN_DOT))
    {
      scanner_next_token(scan);
      if (!match(scan, TOKEN_NAME))
        syntax_error_unexpected(parser);
      scanner_next_token(scan);
      continue;
    }
    if (match(scan, TOKEN_LPAREN))
    {
      scanner_next_token(scan);
      if (match(scan, TOKEN_RPAREN))
      {
        scanner_next_token(scan);
        return;
      }
      parse_expression(parser);
      uint8_t num_args = 1;
      while (match(scan, TOKEN_COMMA))
      {
        scanner_next_token(scan);
        parse_expression(parser);
        ++num_args;
      }
      consume(parser, TOKEN_RPAREN);
      continue;
    }
    break;
  }
  if (match(scan, TOKEN_LBRACE))
  {
    scanner_next_token(scan);
    if (match(scan, TOKEN_RBRACE))
    {
      scanner_next_token(scan);
      return;
    }
    parse_expression(parser);
    uint8_t num_args = 1;
    while (match(scan, TOKEN_COMMA))
    {
      scanner_next_token(scan);
      parse_expression(parser);
      ++num_args;
    }
    consume(parser, TOKEN_RBRACE);
  }
}

static variable_t parse_variable(parser_t *parser, token_t *tk)
{
  hk_function_t *fn = parser->fn;
  variable_t *var = lookup_variable(parser, tk);
  if (var)
    return *var;
  var = parse_nonlocal(parser->parent, tk);
  if (var)
    return *var;
  int32_t index = lookup_global(tk->length, tk->start);
  if (index == -1)
    syntax_error(fn->name->chars, parser->scan->file->chars, tk->line, tk->col,
      "variable `%.*s` is used but not defined", tk->length, tk->start);
  return (variable_t) {.is_local = false, .depth = -1, .index = index, .length = tk->length,
    .start = tk->start, .is_mutable = false};
}

static variable_t *parse_nonlocal(parser_t *parser, token_t *tk)
{
  if (!parser)
    return NULL;
  hk_function_t *fn = parser->fn;
  variable_t *var = lookup_variable(parser, tk);
  if (var)
  {
    if (var->is_local)
    {
      if (var->is_mutable)
        syntax_error(fn->name->chars, parser->scan->file->chars, tk->line, tk->col,
          "cannot capture mutable variable `%.*s`", tk->length, tk->start);
    }
    return var;
  }
  return parse_nonlocal(parser->parent, tk);
}

hk_ast_node_t *hk_parse(hk_string_t *file, hk_string_t *source)
{
  scanner_t scan;
  scanner_init(&scan, file, source);
  parser_t parser;
  parser_init(&parser, NULL, &scan, hk_string_from_chars(-1, "main"));
  char args_name[] = "args";
  token_t tk = {.length = sizeof(args_name) - 1, .start = args_name};
  add_local(&parser, &tk, false);
  while (!match(parser.scan, TOKEN_EOF))
    parse_statement(&parser);
  scanner_free(&scan);
  parser_free(&parser);
  return NULL;
}
