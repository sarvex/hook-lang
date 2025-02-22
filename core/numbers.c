//
// The Hook Programming Language
// numbers.c
//

#include "numbers.h"
#include <stdlib.h>
#include <float.h>
#include <hook/check.h>
#include <hook/status.h>

#define PI  3.14159265358979323846264338327950288
#define TAU 6.28318530717958647692528676655900577

#define LARGEST  DBL_MAX
#define SMALLEST DBL_MIN

#define MAX_INTEGER 9007199254740991.0
#define MIN_INTEGER -9007199254740991.0

static int32_t srand_call(hk_state_t *state, hk_value_t *args);
static int32_t rand_call(hk_state_t *state, hk_value_t *args);

static int32_t srand_call(hk_state_t *state, hk_value_t *args)
{
  if (hk_check_argument_number(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  srand((uint32_t) hk_as_number(args[1]));
  return hk_state_push_nil(state);
}

static int32_t rand_call(hk_state_t *state, hk_value_t *args)
{
  (void) args;
  double result = (double) rand() / RAND_MAX;
  return hk_state_push_number(state, result);
}

HK_LOAD_FN(numbers)
{
  if (hk_state_push_string_from_chars(state, -1, "numbers") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "PI") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, PI) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "TAU") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, TAU) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "LARGEST") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, LARGEST) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "SMALLEST") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, SMALLEST) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "MAX_INTEGER") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, MAX_INTEGER) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "MIN_INTEGER") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_number(state, MIN_INTEGER) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "srand") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "srand", 1, &srand_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_string_from_chars(state, -1, "rand") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_state_push_new_native(state, "rand", 0, &rand_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  return hk_state_construct(state, 8);
}
