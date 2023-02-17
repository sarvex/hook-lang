//
// The Hook Programming Language
// hk_generator.c
//

#include "hk_generator.h"

static void generator_deinit(hk_iterator_t *it);
static bool generator_is_valid(hk_iterator_t *it);
static hk_value_t generator_get_current(hk_iterator_t *it);
static hk_iterator_t *generator_next(hk_iterator_t *it);
static void generator_inplace_next(hk_iterator_t *it);

static void generator_deinit(hk_iterator_t *it)
{
  // TODO: Implement this function
  (void) it;
}

static bool generator_is_valid(hk_iterator_t *it)
{
  // TODO: Implement this function
  (void) it;
  return false;
}

static hk_value_t generator_get_current(hk_iterator_t *it)
{
  // TODO: Implement this function
  (void) it;
  return HK_NIL_VALUE;
}

static hk_iterator_t *generator_next(hk_iterator_t *it)
{
  // TODO: Implement this function
  (void) it;
  return NULL;
}

static void generator_inplace_next(hk_iterator_t *it)
{
  // TODO: Implement this function
  (void) it;
}

hk_generator_t *hk_generator_new(hk_closure_t *cl)
{
  // TODO: Implement this function
  (void) generator_deinit;
  (void) generator_is_valid;
  (void) generator_get_current;
  (void) generator_next;
  (void) generator_inplace_next;
  (void) cl;
  return NULL;
}
