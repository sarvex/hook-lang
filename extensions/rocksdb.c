//
// The Hook Programming Language
// rocksdb.c
//

#include "rocksdb.h"
#include <rocksdb/c.h>
#include "hk_status.h"

static int32_t dummy_call(hk_vm_t *vm, hk_value_t *args);

static int32_t dummy_call(hk_vm_t *vm, hk_value_t *args)
{
  // TODO implement
  (void) args;
  return hk_vm_push_nil(vm);
}

HK_LOAD_FN(rocksdb)
{
  if (hk_vm_push_string_from_chars(vm, -1, "rocksdb") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "dummy") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "dummy", 0, &dummy_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  return hk_vm_construct(vm, 1);
}
