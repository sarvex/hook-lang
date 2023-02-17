//
// The Hook Programming Language
// hk_generator.h
//

#ifndef HK_GENERATOR_H
#define HK_GENERATOR_H

#include "hk_state.h"

typedef struct
{
  HK_ITERATOR_HEADER
  hk_closure_t *cl;
  uint8_t *pc;
  hk_state_t state;
} hk_generator_t ;

hk_generator_t *hk_generator_new(hk_closure_t *cl);

#endif // HK_GENERATOR_H
