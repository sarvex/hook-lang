//
// The Hook Programming Language
// regex.c
//

#include "regex.h"
#include <regex.h>
#include <hook/memory.h>
#include <hook/check.h>
#include <hook/status.h>
#include <hook/error.h>

typedef struct
{
  HK_USERDATA_HEADER
  regex_t regex;
} regex_wrapper_t;

static inline regex_wrapper_t *regex_wrapper_new(regex_t regex);
static void regex_wrapper_deinit(hk_userdata_t *udata);
static int32_t new_call(hk_state_t *state, hk_value_t *args);
static int32_t find_call(hk_state_t *state, hk_value_t *args);
static int32_t is_match_call(hk_state_t *state, hk_value_t *args);

static inline regex_wrapper_t *regex_wrapper_new(regex_t regex)
{
  regex_wrapper_t *wrapper = (regex_wrapper_t *) hk_allocate(sizeof(*wrapper));
  hk_userdata_init((hk_userdata_t *) wrapper, &regex_wrapper_deinit);
  wrapper->regex = regex;
  return wrapper;
}

static void regex_wrapper_deinit(hk_userdata_t *udata)
{
  regfree(&((regex_wrapper_t *) udata)->regex);
}

static int32_t new_call(hk_state_t *state, hk_value_t *args)
{
  if (hk_check_argument_string(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_string_t *pattern = hk_as_string(args[1]);
  regex_t regex;
  int32_t errcode = regcomp(&regex, pattern->chars, REG_EXTENDED);
  if (errcode)
  {
    char errbuf[1024];
    regerror(errcode, &regex, errbuf, sizeof(errbuf));
    hk_runtime_error("cannot compile regex: %s", errbuf);
    return HK_STATUS_ERROR;
  }
  return hk_state_push_userdata(state, (hk_userdata_t *) regex_wrapper_new(regex));
}

static int32_t find_call(hk_state_t *state, hk_value_t *args)
{
  if (hk_check_argument_userdata(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_check_argument_string(args, 2) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  regex_wrapper_t *wrapper = (regex_wrapper_t *) hk_as_userdata(args[1]);
  hk_string_t *str = hk_as_string(args[2]);
  regmatch_t match;
  int32_t errcode = regexec(&wrapper->regex, str->chars, 1, &match, 0);
  if (errcode)
  {
    if (errcode == REG_NOMATCH)
      return hk_state_push_nil(state);
    char errbuf[1024];
    regerror(errcode, &wrapper->regex, errbuf, sizeof(errbuf));
    hk_runtime_error("cannot match regex: %s", errbuf);
    return HK_STATUS_ERROR;
  }
  int32_t start = match.rm_so;
  int32_t end = match.rm_eo;
  if (start == -1 || end == -1)
    return hk_state_push_nil(state);
  hk_array_t *result = hk_array_new_with_capacity(2);
  hk_array_inplace_add_element(result, hk_number_value(start));
  hk_array_inplace_add_element(result, hk_number_value(end));
  return hk_state_push_array(state, result);
}

static int32_t is_match_call(hk_state_t *state, hk_value_t *args)
{
  if (hk_check_argument_userdata(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_check_argument_string(args, 2) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  regex_wrapper_t *wrapper = (regex_wrapper_t *) hk_as_userdata(args[1]);
  hk_string_t *str = hk_as_string(args[2]);
  int32_t errcode = regexec(&wrapper->regex, str->chars, 0, NULL, 0);
  if (errcode)
  {
    if (errcode == REG_NOMATCH)
      return hk_state_push_bool(state, false);
    char errbuf[1024];
    regerror(errcode, &wrapper->regex, errbuf, sizeof(errbuf));
    hk_runtime_error("cannot match regex: %s", errbuf);
    return HK_STATUS_ERROR;
  }
  return hk_state_push_bool(state, true);
}

HK_LOAD_FN(regex)
{
  if (hk_state_push_string_from_chars(state, -1, "regex") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "new") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "new", 1, &new_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "find") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "find", 2, &find_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "is_match") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "is_match", 2, &is_match_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  return hk_state_construct(state, 3);
}
