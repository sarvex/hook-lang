//
// The Hook Programming Language
// olive.c
//

#include "olive.h"
#define OLIVEC_IMPLEMENTATION
#include "deps/olive.c"
#include "hk_memory.h"
#include "hk_status.h"

typedef struct
{
  HK_USERDATA_HEADER
  Olivec_Canvas oc;
} canvas_t;

static inline canvas_t *canvas_new(Olivec_Canvas oc);
static void canvas_deinit(hk_userdata_t *udata);
static int32_t canvas_call(hk_vm_t *vm, hk_value_t *args);

static inline canvas_t *canvas_new(Olivec_Canvas oc)
{
  canvas_t *canvas = (canvas_t *) hk_allocate(sizeof(*canvas));
  hk_userdata_init((hk_userdata_t *) canvas, NULL);
  canvas->oc = oc;
  return canvas;
}

static int32_t canvas_call(hk_vm_t *vm, hk_value_t *args)
{
  // TODO: Implement
  (void) args;
  return hk_vm_push_nil(vm);
}

HK_LOAD_FN(olive)
{
  if (hk_vm_push_string_from_chars(vm, -1, "olive") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "canvas") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "canvas", 1, &canvas_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  return hk_vm_construct(vm, 1);
}
