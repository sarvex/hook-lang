//
// The Hook Programming Language
// utf8.c
//

#include "utf8.h"
#include <hook/check.h>
#include <hook/status.h>

static inline int32_t decode_char(unsigned char c);
static int32_t len_call(hk_state_t *state, hk_value_t *args);
static int32_t sub_call(hk_state_t *state, hk_value_t *args);

static inline int32_t decode_char(unsigned char c)
{
  if ((c & 0xc0) == 0x80)
    return 0;
  if ((c & 0xf8) == 0xf0)
    return 4;
  if ((c & 0xf0) == 0xe0)
    return 3;
  if ((c & 0xe0) == 0xc0)
    return 2;
  return 1;
}

static int32_t len_call(hk_state_t *state, hk_value_t *args)
{
  if (hk_check_argument_string(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_string_t *str = hk_as_string(args[1]);
  int32_t result = 0;
  for (int32_t i = 0; i < str->length;)
  {
    int32_t length = decode_char((unsigned char) str->chars[i]);
    if (!length)
      break;
    i += length;
    ++result;
  }
  return hk_state_push_number(state, result);
}

static int32_t sub_call(hk_state_t *state, hk_value_t *args)
{
  if (hk_check_argument_string(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_check_argument_number(args, 2) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_check_argument_number(args, 3) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_string_t *str = hk_as_string(args[1]);
  int32_t start = (int32_t) hk_as_number(args[2]);
  int32_t end = (int32_t) hk_as_number(args[3]);
  int32_t length = 0;
  int32_t i = 0;
  while (i < str->length)
  {
    int32_t n = decode_char((unsigned char) str->chars[i]);
    if (!n || length == start)
      break;
    i += n;
    ++length;
  }
  start = i;
  while (i < str->length)
  {
    int32_t n = decode_char((unsigned char) str->chars[i]);
    if (!n || length == end)
      break;
    i += n;
    ++length;
  }
  end = i;
  length = end - start;
  char *chars = &str->chars[start];
  return hk_state_push_string_from_chars(state, length, chars);
}

HK_LOAD_FN(utf8)
{
  if (hk_state_push_string_from_chars(state, -1, "utf8") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "len") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "len", 1, &len_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "sub") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "sub", 3, &sub_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  return hk_state_construct(state, 2);
}
