//
// The Hook Programming Language
// hk_iterable.c
//

#include "hk_iterable.h"
#include "hk_range.h"
#include "hk_array.h"

hk_iterator_t *hk_new_iterator(hk_value_t val)
{
  if (!hk_is_iterable(val))
    return NULL;
  if (hk_is_range(val))
    return hk_range_iterator_new(hk_as_range(val));
  return hk_array_iterator_new(hk_as_array(val));
}
