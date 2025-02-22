//
// The Hook Programming Language
// state.c
//

#include <hook/state.h>
#include <stdlib.h>
#include <math.h>
#include <hook/struct.h>
#include <hook/iterable.h>
#include <hook/memory.h>
#include <hook/status.h>
#include <hook/error.h>
#include <hook/utils.h>
#include "module.h"
#include "builtin.h"

static inline int32_t push(hk_state_t *state, hk_value_t val);
static inline void pop(hk_state_t *state);
static inline int32_t read_byte(uint8_t **pc);
static inline int32_t read_word(uint8_t **pc);
static inline int32_t do_range(hk_state_t *state);
static inline int32_t do_array(hk_state_t *state, int32_t length);
static inline int32_t do_struct(hk_state_t *state, int32_t length);
static inline int32_t do_instance(hk_state_t *state, int32_t num_args);
static inline int32_t adjust_instance_args(hk_state_t *state, int32_t length, int32_t num_args);
static inline int32_t do_construct(hk_state_t *state, int32_t length);
static inline int32_t do_iterator(hk_state_t *state);
static inline int32_t do_closure(hk_state_t *state, hk_function_t *fn);
static inline int32_t do_unpack_array(hk_state_t *state, int32_t n);
static inline int32_t do_unpack_struct(hk_state_t *state, int32_t n);
static inline int32_t do_add_element(hk_state_t *state);
static inline int32_t do_get_element(hk_state_t *state);
static inline void slice_string(hk_state_t *state, hk_value_t *slot, hk_string_t *str, hk_range_t *range);
static inline void slice_array(hk_state_t *state, hk_value_t *slot, hk_array_t *arr, hk_range_t *range);
static inline int32_t do_fetch_element(hk_state_t *state);
static inline void do_set_element(hk_state_t *state);
static inline int32_t do_put_element(hk_state_t *state);
static inline int32_t do_delete_element(hk_state_t *state);
static inline int32_t do_inplace_add_element(hk_state_t *state);
static inline int32_t do_inplace_put_element(hk_state_t *state);
static inline int32_t do_inplace_delete_element(hk_state_t *state);
static inline int32_t do_get_field(hk_state_t *state, hk_string_t *name);
static inline int32_t do_fetch_field(hk_state_t *state, hk_string_t *name);
static inline void do_set_field(hk_state_t *state);
static inline int32_t do_put_field(hk_state_t *state, hk_string_t *name);
static inline int32_t do_inplace_put_field(hk_state_t *state, hk_string_t *name);
static inline void do_current(hk_state_t *state);
static inline void do_next(hk_state_t *state);
static inline void do_equal(hk_state_t *state);
static inline int32_t do_greater(hk_state_t *state);
static inline int32_t do_less(hk_state_t *state);
static inline void do_not_equal(hk_state_t *state);
static inline int32_t do_not_greater(hk_state_t *state);
static inline int32_t do_not_less(hk_state_t *state);
static inline int32_t do_bitwise_or(hk_state_t *state);
static inline int32_t do_bitwise_xor(hk_state_t *state);
static inline int32_t do_bitwise_and(hk_state_t *state);
static inline int32_t do_left_shift(hk_state_t *state);
static inline int32_t do_right_shift(hk_state_t *state);
static inline int32_t do_add(hk_state_t *state);
static inline int32_t concat_strings(hk_state_t *state, hk_value_t *slots, hk_value_t val1, hk_value_t val2);
static inline int32_t concat_arrays(hk_state_t *state, hk_value_t *slots, hk_value_t val1, hk_value_t val2);
static inline int32_t do_subtract(hk_state_t *state);
static inline int32_t diff_arrays(hk_state_t *state, hk_value_t *slots, hk_value_t val1, hk_value_t val2);
static inline int32_t do_multiply(hk_state_t *state);
static inline int32_t do_divide(hk_state_t *state);
static inline int32_t do_quotient(hk_state_t *state);
static inline int32_t do_remainder(hk_state_t *state);
static inline int32_t do_negate(hk_state_t *state);
static inline void do_not(hk_state_t *state);
static inline int32_t do_bitwise_not(hk_state_t *state);
static inline int32_t do_increment(hk_state_t *state);
static inline int32_t do_decrement(hk_state_t *state);
static inline int32_t do_call(hk_state_t *state, int32_t num_args);
static inline int32_t adjust_call_args(hk_state_t *state, int32_t arity, int32_t num_args);
static inline void print_trace(hk_string_t *name, hk_string_t *file, int32_t line);
static inline int32_t call_function(hk_state_t *state, hk_value_t *locals, hk_closure_t *cl, int32_t *line);
static inline void discard_frame(hk_state_t *state, hk_value_t *slots);
static inline void move_result(hk_state_t *state, hk_value_t *slots);

static inline int32_t push(hk_state_t *state, hk_value_t val)
{
  if (state->stack_top == state->stack_end)
  {
    hk_runtime_error("stack overflow");
    return HK_STATUS_ERROR;
  }
  ++state->stack_top;
  state->stack[state->stack_top] = val;
  return HK_STATUS_OK;
}

static inline void pop(hk_state_t *state)
{
  hk_assert(state->stack_top > -1, "stack underflow");
  hk_value_t val = state->stack[state->stack_top];
  --state->stack_top;
  hk_value_release(val);
}

static inline int32_t read_byte(uint8_t **pc)
{
  int32_t byte = **pc;
  ++(*pc);
  return byte;
}

static inline int32_t read_word(uint8_t **pc)
{
  int32_t word = *((uint16_t *) *pc);
  *pc += 2;
  return word;
}

static inline int32_t do_range(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: range must be of type number");
    return HK_STATUS_ERROR;
  }
  hk_range_t *range = hk_range_new(hk_as_number(val1), hk_as_number(val2));
  hk_incr_ref(range);
  slots[0] = hk_range_value(range);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_array(hk_state_t *state, int32_t length)
{
  hk_value_t *slots = &state->stack[state->stack_top - length + 1];
  hk_array_t *arr = hk_array_new_with_capacity(length);
  arr->length = length;
  for (int32_t i = 0; i < length; ++i)
    arr->elements[i] = slots[i];
  state->stack_top -= length;
  if (push(state, hk_array_value(arr)) == HK_STATUS_ERROR)
  {
    hk_array_free(arr);
    return HK_STATUS_ERROR;
  }
  hk_incr_ref(arr);
  return HK_STATUS_OK;
}

static inline int32_t do_struct(hk_state_t *state, int32_t length)
{
  hk_value_t *slots = &state->stack[state->stack_top - length];
  hk_value_t val = slots[0];
  hk_string_t *struct_name = hk_is_nil(val) ? NULL : hk_as_string(val);
  hk_struct_t *ztruct = hk_struct_new(struct_name);
  for (int32_t i = 1; i <= length; ++i)
  {
    hk_string_t *field_name = hk_as_string(slots[i]);
    if (!hk_struct_define_field(ztruct, field_name))
    {
      hk_runtime_error("field %.*s is already defined", field_name->length,
        field_name->chars);
      hk_struct_free(ztruct);
      return HK_STATUS_ERROR;
    }
  }
  for (int32_t i = 1; i <= length; ++i)
    hk_decr_ref(hk_as_object(slots[i]));
  state->stack_top -= length;
  hk_incr_ref(ztruct);
  slots[0] = hk_struct_value(ztruct);
  if (struct_name)
    hk_decr_ref(struct_name);
  return HK_STATUS_OK;
}

static inline int32_t do_instance(hk_state_t *state, int32_t num_args)
{
  hk_value_t *slots = &state->stack[state->stack_top - num_args];
  hk_value_t val = slots[0];
  if (!hk_is_struct(val))
  {
    hk_runtime_error("type error: cannot use %s as a struct", hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  hk_struct_t *ztruct = hk_as_struct(val);
  int32_t length = ztruct->length;
  adjust_instance_args(state, length, num_args);
  hk_instance_t *inst = hk_instance_new(ztruct);
  for (int32_t i = 0; i < length; ++i)
    inst->values[i] = slots[i + 1];
  state->stack_top -= length;
  hk_incr_ref(inst);
  slots[0] = hk_instance_value(inst);
  hk_struct_release(ztruct);
  return HK_STATUS_OK;
}

static inline int32_t adjust_instance_args(hk_state_t *state, int32_t length, int32_t num_args)
{
  if (num_args > length)
  {
    do
    {
      pop(state);
      --num_args;
    }
    while (num_args > length);
    return HK_STATUS_OK;
  }
  while (num_args < length)
  {
    if (push(state, HK_NIL_VALUE) == HK_STATUS_ERROR)
      return HK_STATUS_ERROR;
    ++num_args;
  }
  return HK_STATUS_OK;
}

static inline int32_t do_construct(hk_state_t *state, int32_t length)
{
  int32_t n = length << 1;
  hk_value_t *slots = &state->stack[state->stack_top - n];
  hk_value_t val = slots[0];
  hk_string_t *struct_name = hk_is_nil(val) ? NULL : hk_as_string(val);
  hk_struct_t *ztruct = hk_struct_new(struct_name);
  for (int32_t i = 1; i <= n; i += 2)
  {
    hk_string_t *field_name = hk_as_string(slots[i]);
    if (hk_struct_define_field(ztruct, field_name))
      continue;
    hk_runtime_error("field %.*s is already defined", field_name->length,
      field_name->chars);
    hk_struct_free(ztruct);
    return HK_STATUS_ERROR;
  }
  for (int32_t i = 1; i <= n; i += 2)
    hk_decr_ref(hk_as_object(slots[i]));
  hk_instance_t *inst = hk_instance_new(ztruct);
  for (int32_t i = 2, j = 0; i <= n + 1; i += 2, ++j)
    inst->values[j] = slots[i];
  state->stack_top -= n;
  hk_incr_ref(inst);
  slots[0] = hk_instance_value(inst);
  if (struct_name)
    hk_decr_ref(struct_name);
  return HK_STATUS_OK;
}

static inline int32_t do_iterator(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top];
  hk_value_t val = slots[0];
  if (hk_is_iterator(val))
    return HK_STATUS_OK;
  hk_iterator_t *it = hk_new_iterator(val);
  if (!it)
  {
    hk_runtime_error("type error: value of type %s is not iterable", hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  hk_incr_ref(it);
  slots[0] = hk_iterator_value(it);
  hk_value_release(val);
  return HK_STATUS_OK;
}

static inline int32_t do_closure(hk_state_t *state, hk_function_t *fn)
{
  int32_t num_nonlocals = fn->num_nonlocals;
  hk_value_t *slots = &state->stack[state->stack_top - num_nonlocals + 1];
  hk_closure_t *cl = hk_closure_new(fn);
  for (int32_t i = 0; i < num_nonlocals; ++i)
    cl->nonlocals[i] = slots[i];
  state->stack_top -= num_nonlocals;
  if (push(state, hk_closure_value(cl)) == HK_STATUS_ERROR)
  {
    hk_closure_free(cl);
    return HK_STATUS_ERROR;
  }
  hk_incr_ref(cl);
  return HK_STATUS_OK;
}

static inline int32_t do_unpack_array(hk_state_t *state, int32_t n)
{
  hk_value_t val = state->stack[state->stack_top];
  if (!hk_is_array(val))
  {
    hk_runtime_error("type error: value of type %s is not an array",
      hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  hk_array_t *arr = hk_as_array(val);
  --state->stack_top;
  int32_t status = HK_STATUS_OK;
  for (int32_t i = 0; i < n && i < arr->length; ++i)
  {
    hk_value_t elem = hk_array_get_element(arr, i);
    if ((status = push(state, elem)) == HK_STATUS_ERROR)
      goto end;
    hk_value_incr_ref(elem);
  }
  for (int32_t i = arr->length; i < n; ++i)
    if ((status = push(state, HK_NIL_VALUE)) == HK_STATUS_ERROR)
      break;
end:
  hk_array_release(arr);
  return status;
}

static inline int32_t do_unpack_struct(hk_state_t *state, int32_t n)
{
  hk_value_t val = state->stack[state->stack_top];
  if (!hk_is_instance(val))
  {
    hk_runtime_error("type error: value of type %s is not an instance of struct",
      hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  hk_instance_t *inst = hk_as_instance(val);
  hk_struct_t *ztruct = inst->ztruct;
  hk_value_t *slots = &state->stack[state->stack_top - n];
  for (int32_t i = 0; i < n; ++i)
  {
    hk_string_t *name = hk_as_string(slots[i]);
    int32_t index = hk_struct_index_of(ztruct, name);
    hk_value_t value = index == -1 ? HK_NIL_VALUE :
      hk_instance_get_field(inst, index);
    hk_value_incr_ref(value);
    hk_decr_ref(name);
    slots[i] = value;
  }
  --state->stack_top;
  hk_instance_release(inst);
  return HK_STATUS_OK;
}

static inline int32_t do_add_element(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_array(val1))
  {
    hk_runtime_error("type error: cannot use %s as an array", hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  hk_array_t *arr = hk_as_array(val1);
  hk_array_t *result = hk_array_add_element(arr, val2);
  hk_incr_ref(result);
  slots[0] = hk_array_value(result);
  --state->stack_top;
  hk_array_release(arr);  
  hk_value_decr_ref(val2);
  return HK_STATUS_OK;
}

static inline int32_t do_get_element(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (hk_is_string(val1))
  {
    hk_string_t *str = hk_as_string(val1);
    if (hk_is_int(val2))
    {
      int64_t index = (int64_t) hk_as_number(val2);
      if (index < 0 || index >= str->length)
      {
        hk_runtime_error("range error: index %d is out of bounds for string of length %d",
          index, str->length);
        return HK_STATUS_ERROR;
      }
      hk_value_t result = hk_string_value(hk_string_from_chars(1, &str->chars[(int32_t) index]));
      hk_value_incr_ref(result);
      slots[0] = result;
      --state->stack_top;
      hk_string_release(str);
      return HK_STATUS_OK;
    }
    if (!hk_is_range(val2))
    {
      hk_runtime_error("type error: string cannot be indexed by %s", hk_type_name(val2.type));
      return HK_STATUS_ERROR;
    }
    slice_string(state, slots, str, hk_as_range(val2));
    return HK_STATUS_OK;
  }
  if (!hk_is_array(val1))
  {
    hk_runtime_error("type error: %s cannot be indexed", hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  hk_array_t *arr = hk_as_array(val1);
  if (hk_is_int(val2))
  {
    int64_t index = (int64_t) hk_as_number(val2);
    if (index < 0 || index >= arr->length)
    {
      hk_runtime_error("range error: index %d is out of bounds for array of length %d",
        index, arr->length);
      return HK_STATUS_ERROR;
    }
    hk_value_t result = hk_array_get_element(arr, (int32_t) index);
    hk_value_incr_ref(result);
    slots[0] = result;
    --state->stack_top;
    hk_array_release(arr);
    return HK_STATUS_OK;
  }
  if (!hk_is_range(val2))
  {
    hk_runtime_error("type error: array cannot be indexed by %s", hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  slice_array(state, slots, arr, hk_as_range(val2));
  return HK_STATUS_OK;
}

static inline void slice_string(hk_state_t *state, hk_value_t *slot, hk_string_t *str, hk_range_t *range)
{
  int32_t str_end = str->length - 1;
  int64_t start = range->start;
  int64_t end = range->end;
  hk_string_t *result;
  if (start > end || start > str_end || end < 0)
  {
    result = hk_string_new();
    goto end;
  }
  if (start <= 0 && end >= str_end)
  {
    --state->stack_top;
    hk_range_release(range);
    return;
  }
  int32_t length = end - start + 1;
  result = hk_string_from_chars(length, &str->chars[start]);
end:
  hk_incr_ref(result);
  *slot = hk_string_value(result);
  --state->stack_top;
  hk_string_release(str);
  hk_range_release(range);
}

static inline void slice_array(hk_state_t *state, hk_value_t *slot, hk_array_t *arr, hk_range_t *range)
{
  int32_t arr_end = arr->length - 1;
  int64_t start = range->start;
  int64_t end = range->end;
  hk_array_t *result;
  if (start > end || start > arr_end || end < 0)
  {
    result = hk_array_new();
    goto end;
  }
  if (start <= 0 && end >= arr_end)
  {
    --state->stack_top;
    hk_range_release(range);
    return;
  }
  int32_t length = end - start + 1;
  result = hk_array_new_with_capacity(length);
  result->length = length;
  for (int32_t i = start, j = 0; i <= end ; ++i, ++j)
  {
    hk_value_t elem = hk_array_get_element(arr, i);
    hk_value_incr_ref(elem);
    result->elements[j] = elem;
  }
end:
  hk_incr_ref(result);
  *slot = hk_array_value(result);
  --state->stack_top;
  hk_array_release(arr);
  hk_range_release(range);
}

static inline int32_t do_fetch_element(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_array(val1))
  {
    hk_runtime_error("type error: cannot use %s as an array", hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  if (!hk_is_int(val2))
  {
    hk_runtime_error("type error: array cannot be indexed by %s", hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  hk_array_t *arr = hk_as_array(val1);
  int64_t index = (int64_t) hk_as_number(val2);
  if (index < 0 || index >= arr->length)
  {
    hk_runtime_error("range error: index %d is out of bounds for array of length %d",
      index, arr->length);
    return HK_STATUS_ERROR;
  }
  hk_value_t elem = hk_array_get_element(arr, (int32_t) index);
  if (push(state, elem) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_value_incr_ref(elem);
  return HK_STATUS_OK;
}

static inline void do_set_element(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 2];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  hk_value_t val3 = slots[2];
  hk_array_t *arr = hk_as_array(val1);
  int32_t index = (int32_t) hk_as_number(val2);
  hk_array_t *result = hk_array_set_element(arr, index, val3);
  hk_incr_ref(result);
  slots[0] = hk_array_value(result);
  state->stack_top -= 2;
  hk_array_release(arr);
  hk_value_decr_ref(val3);
}

static inline int32_t do_put_element(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 2];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  hk_value_t val3 = slots[2];
  if (!hk_is_array(val1))
  {
    hk_runtime_error("type error: cannot use %s as an array", hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  if (!hk_is_int(val2))
  {
    hk_runtime_error("type error: array cannot be indexed by %s", hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  hk_array_t *arr = hk_as_array(val1);
  int64_t index = (int64_t) hk_as_number(val2);
  if (index < 0 || index >= arr->length)
  {
    hk_runtime_error("range error: index %d is out of bounds for array of length %d",
      index, arr->length);
    return HK_STATUS_ERROR;
  }
  hk_array_t *result = hk_array_set_element(arr, (int32_t) index, val3);
  hk_incr_ref(result);
  slots[0] = hk_array_value(result);
  state->stack_top -= 2;
  hk_array_release(arr);
  hk_value_decr_ref(val3);
  return HK_STATUS_OK;
}

static inline int32_t do_delete_element(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_array(val1))
  {
    hk_runtime_error("type error: cannot use %s as an array", hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  if (!hk_is_int(val2))
  {
    hk_runtime_error("type error: array cannot be indexed by %s", hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  hk_array_t *arr = hk_as_array(val1);
  int64_t index = (int64_t) hk_as_number(val2);
  if (index < 0 || index >= arr->length)
  {
    hk_runtime_error("range error: index %d is out of bounds for array of length %d",
      index, arr->length);
    return HK_STATUS_ERROR;
  }
  hk_array_t *result = hk_array_delete_element(arr, (int32_t) index);
  hk_incr_ref(result);
  slots[0] = hk_array_value(result);
  --state->stack_top;
  hk_array_release(arr);
  return HK_STATUS_OK;
}

static inline int32_t do_inplace_add_element(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_array(val1))
  {
    hk_runtime_error("type error: cannot use %s as an array", hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  hk_array_t *arr = hk_as_array(val1);
  if (arr->ref_count == 2)
  {
    hk_array_inplace_add_element(arr, val2);
    --state->stack_top;
    hk_value_decr_ref(val2);
    return HK_STATUS_OK;
  }
  hk_array_t *result = hk_array_add_element(arr, val2);
  hk_incr_ref(result);
  slots[0] = hk_array_value(result);
  --state->stack_top;
  hk_array_release(arr);
  hk_value_decr_ref(val2);
  return HK_STATUS_OK;
}

static inline int32_t do_inplace_put_element(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 2];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  hk_value_t val3 = slots[2];
  if (!hk_is_array(val1))
  {
    hk_runtime_error("type error: cannot use %s as an array", hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  if (!hk_is_int(val2))
  {
    hk_runtime_error("type error: array cannot be indexed by %s", hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  hk_array_t *arr = hk_as_array(val1);
  int64_t index = (int64_t) hk_as_number(val2);
  if (index < 0 || index >= arr->length)
  {
    hk_runtime_error("range error: index %d is out of bounds for array of length %d",
      index, arr->length);
    return HK_STATUS_ERROR;
  }
  if (arr->ref_count == 2)
  {
    hk_array_inplace_set_element(arr, (int32_t) index, val3);
    state->stack_top -= 2;
    hk_value_decr_ref(val3);
    return HK_STATUS_OK;
  }
  hk_array_t *result = hk_array_set_element(arr, index, val3);
  hk_incr_ref(result);
  slots[0] = hk_array_value(result);
  state->stack_top -= 2;
  hk_array_release(arr);
  hk_value_decr_ref(val3);
  return HK_STATUS_OK;
}

static inline int32_t do_inplace_delete_element(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_array(val1))
  {
    hk_runtime_error("type error: cannot use %s as an array", hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  if (!hk_is_int(val2))
  {
    hk_runtime_error("type error: array cannot be indexed by %s", hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  hk_array_t *arr = hk_as_array(val1);
  int64_t index = (int64_t) hk_as_number(val2);
  if (index < 0 || index >= arr->length)
  {
    hk_runtime_error("range error: index %d is out of bounds for array of length %d",
      index, arr->length);
    return HK_STATUS_ERROR;
  }
  if (arr->ref_count == 2)
  {
    hk_array_inplace_delete_element(arr, (int32_t) index);
    --state->stack_top;
    return HK_STATUS_OK;
  }
  hk_array_t *result = hk_array_delete_element(arr, index);
  hk_incr_ref(result);
  slots[0] = hk_array_value(result);
  --state->stack_top;
  hk_array_release(arr);
  return HK_STATUS_OK;
}

static inline int32_t do_get_field(hk_state_t *state, hk_string_t *name)
{
  hk_value_t *slots = &state->stack[state->stack_top];
  hk_value_t val = slots[0];
  if (!hk_is_instance(val))
  {
    hk_runtime_error("type error: cannot use %s as an instance of struct",
      hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  hk_instance_t *inst = hk_as_instance(val);
  int32_t index = hk_struct_index_of(inst->ztruct, name);
  if (index == -1)
  {
    hk_runtime_error("no field %.*s on struct", name->length, name->chars);
    return HK_STATUS_ERROR;
  }
  hk_value_t value = hk_instance_get_field(inst, index);
  hk_value_incr_ref(value);
  slots[0] = value;
  hk_instance_release(inst);
  return HK_STATUS_OK;
}

static inline int32_t do_fetch_field(hk_state_t *state, hk_string_t *name)
{
  hk_value_t *slots = &state->stack[state->stack_top];
  hk_value_t val = slots[0];
  if (!hk_is_instance(val))
  {
    hk_runtime_error("type error: cannot use %s as an instance of struct",
      hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  hk_instance_t *inst = hk_as_instance(val);
  int32_t index = hk_struct_index_of(inst->ztruct, name);
  if (index == -1)
  {
    hk_runtime_error("no field %.*s on struct", name->length, name->chars);
    return HK_STATUS_ERROR;
  }
  if (push(state, hk_number_value(index)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_value_t value = hk_instance_get_field(inst, index);
  if (push(state, value) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_value_incr_ref(value);
  return HK_STATUS_OK;
}

static inline void do_set_field(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 2];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  hk_value_t val3 = slots[2];
  hk_instance_t *inst = hk_as_instance(val1);
  int32_t index = (int32_t) hk_as_number(val2);
  hk_instance_t *result = hk_instance_set_field(inst, index, val3);
  hk_incr_ref(result);
  slots[0] = hk_instance_value(result);
  state->stack_top -= 2;
  hk_instance_release(inst);
  hk_value_decr_ref(val3);
}

static inline int32_t do_put_field(hk_state_t *state, hk_string_t *name)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_instance(val1))
  {
    hk_runtime_error("type error: cannot use %s as an instance of struct",
      hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  hk_instance_t *inst = hk_as_instance(val1);
  int32_t index = hk_struct_index_of(inst->ztruct, name);
  if (index == -1)
  {
    hk_runtime_error("no field %.*s on struct", name->length, name->chars);
    return HK_STATUS_ERROR;
  }
  hk_instance_t *result = hk_instance_set_field(inst, index, val2);
  hk_incr_ref(result);
  slots[0] = hk_instance_value(result);
  --state->stack_top;
  hk_instance_release(inst);
  hk_value_decr_ref(val2);
  return HK_STATUS_OK;
}

static inline int32_t do_inplace_put_field(hk_state_t *state, hk_string_t *name)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_instance(val1))
  {
    hk_runtime_error("type error: cannot use %s as an instance of struct",
      hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  hk_instance_t *inst = hk_as_instance(val1);
  int32_t index = hk_struct_index_of(inst->ztruct, name);
  if (index == -1)
  {
    hk_runtime_error("no field %.*s on struct", name->length, name->chars);
    return HK_STATUS_ERROR;
  }
  if (inst->ref_count == 2)
  {
    hk_instance_inplace_set_field(inst, index, val2);
    --state->stack_top;
    hk_value_decr_ref(val2);
    return HK_STATUS_OK;
  }
  hk_instance_t *result = hk_instance_set_field(inst, index, val2);
  hk_incr_ref(result);
  slots[0] = hk_instance_value(result);
  --state->stack_top;
  hk_instance_release(inst);
  hk_value_decr_ref(val2);
  return HK_STATUS_OK;
}

static inline void do_current(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val = slots[1];
  hk_iterator_t *it = hk_as_iterator(val);
  hk_value_t result = hk_iterator_get_current(it);
  hk_value_incr_ref(result);
  hk_value_release(slots[0]);
  slots[0] = result;
}

static inline void do_next(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top];
  hk_value_t val = slots[0];
  hk_iterator_t *it = hk_as_iterator(val);
  if (it->ref_count == 2)
  {
    hk_iterator_inplace_next(it);
    return;
  }
  hk_iterator_t *result = hk_iterator_next(it);
  hk_incr_ref(result);
  slots[0] = hk_iterator_value(result);
  hk_iterator_release(it);
}

static inline void do_equal(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  slots[0] = hk_value_equal(val1, val2) ? HK_TRUE_VALUE : HK_FALSE_VALUE;
  --state->stack_top;
  hk_value_release(val1);
  hk_value_release(val2);
}

static inline int32_t do_greater(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  int32_t result;
  if (hk_state_compare(val1, val2, &result) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  slots[0] = result > 0 ? HK_TRUE_VALUE : HK_FALSE_VALUE;
  --state->stack_top;
  hk_value_release(val1);
  hk_value_release(val2);
  return HK_STATUS_OK;
}

static inline int32_t do_less(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  int32_t result;
  if (hk_state_compare(val1, val2, &result) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  slots[0] = result < 0 ? HK_TRUE_VALUE : HK_FALSE_VALUE;
  --state->stack_top;
  hk_value_release(val1);
  hk_value_release(val2);
  return HK_STATUS_OK;
}

static inline void do_not_equal(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  slots[0] = hk_value_equal(val1, val2) ? HK_FALSE_VALUE : HK_TRUE_VALUE;
  --state->stack_top;
  hk_value_release(val1);
  hk_value_release(val2);
}

static inline int32_t do_not_greater(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  int32_t result;
  if (hk_state_compare(val1, val2, &result) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  slots[0] = result > 0 ? HK_FALSE_VALUE : HK_TRUE_VALUE;
  --state->stack_top;
  hk_value_release(val1);
  hk_value_release(val2);
  return HK_STATUS_OK;
}

static inline int32_t do_not_less(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  int32_t result;
  if (hk_state_compare(val1, val2, &result) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  slots[0] = result < 0 ? HK_FALSE_VALUE : HK_TRUE_VALUE;
  --state->stack_top;
  hk_value_release(val1);
  hk_value_release(val2);
  return HK_STATUS_OK;
}

static inline int32_t do_bitwise_or(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: cannot apply `bitwise or` between %s and %s", hk_type_name(val1.type),
      hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  int64_t data = ((int64_t) hk_as_number(val1)) | ((int64_t) hk_as_number(val2));
  slots[0] = hk_number_value(data);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_bitwise_xor(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: cannot apply `bitwise xor` between %s and %s", hk_type_name(val1.type),
      hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  int64_t data = ((int64_t) hk_as_number(val1)) ^ ((int64_t) hk_as_number(val2));
  slots[0] = hk_number_value(data);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_bitwise_and(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: cannot apply `bitwise and` between %s and %s", hk_type_name(val1.type),
      hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  int64_t data = ((int64_t) hk_as_number(val1)) & ((int64_t) hk_as_number(val2));
  slots[0] = hk_number_value(data);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_left_shift(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: cannot apply `left shift` between %s and %s", hk_type_name(val1.type),
      hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  int64_t data = ((int64_t) hk_as_number(val1)) << ((int64_t) hk_as_number(val2));
  slots[0] = hk_number_value(data);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_right_shift(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: cannot apply `right shift` between %s and %s", hk_type_name(val1.type),
      hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  int64_t data = ((int64_t) hk_as_number(val1)) >> ((int64_t) hk_as_number(val2));
  slots[0] = hk_number_value(data);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_add(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (hk_is_number(val1))
  {
    if (!hk_is_number(val2))
    {
      hk_runtime_error("type error: cannot add %s to number", hk_type_name(val2.type));
      return HK_STATUS_ERROR;
    }
    double data = hk_as_number(val1) + hk_as_number(val2);
    slots[0] = hk_number_value(data);
    --state->stack_top;
    return HK_STATUS_OK;
  }
  if (hk_is_string(val1))
  {
    if (!hk_is_string(val2))
    {
      hk_runtime_error("type error: cannot concatenate string and %s",
        hk_type_name(val2.type));
      return HK_STATUS_ERROR;
    }
    return concat_strings(state, slots, val1, val2);
  }
  if (hk_is_array(val1))
  {
    if (!hk_is_array(val2))
    {
      hk_runtime_error("type error: cannot concatenate array and %s",
        hk_type_name(val2.type));
      return HK_STATUS_ERROR;
    }
    return concat_arrays(state, slots, val1, val2);
  }
  hk_runtime_error("type error: cannot add %s to %s", hk_type_name(val2.type),
    hk_type_name(val1.type));
  return HK_STATUS_ERROR;
}

static inline int32_t concat_strings(hk_state_t *state, hk_value_t *slots, hk_value_t val1, hk_value_t val2)
{
  hk_string_t *str1 = hk_as_string(val1);
  if (!str1->length)
  {
    slots[0] = val2;
    --state->stack_top;
    hk_string_release(str1);
    return HK_STATUS_OK;
  }
  hk_string_t *str2 = hk_as_string(val2);
  if (!str2->length)
  {
    --state->stack_top;
    hk_string_release(str2);
    return HK_STATUS_OK;
  }
  if (str1->ref_count == 1)
  {
    hk_string_inplace_concat(str1, str2);
    --state->stack_top;
    hk_string_release(str2);
    return HK_STATUS_OK;
  }
  hk_string_t *result = hk_string_concat(str1, str2);
  hk_incr_ref(result);
  slots[0] = hk_string_value(result);
  --state->stack_top;
  hk_string_release(str1);
  hk_string_release(str2);
  return HK_STATUS_OK;
}

static inline int32_t concat_arrays(hk_state_t *state, hk_value_t *slots, hk_value_t val1, hk_value_t val2)
{
  hk_array_t *arr1 = hk_as_array(val1);
  if (!arr1->length)
  {
    slots[0] = val2;
    --state->stack_top;
    hk_array_release(arr1);
    return HK_STATUS_OK;
  }
  hk_array_t *arr2 = hk_as_array(val2);
  if (!arr2->length)
  {
    --state->stack_top;
    hk_array_release(arr2);
    return HK_STATUS_OK;
  }
  if (arr1->ref_count == 1)
  {
    hk_array_inplace_concat(arr1, arr2);
    --state->stack_top;
    hk_array_release(arr2);
    return HK_STATUS_OK;
  }
  hk_array_t *result = hk_array_concat(arr1, arr2);
  hk_incr_ref(result);
  slots[0] = hk_array_value(result);
  --state->stack_top;
  hk_array_release(arr1);
  hk_array_release(arr2);
  return HK_STATUS_OK;
}

static inline int32_t do_subtract(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (hk_is_number(val1))
  {
    if (!hk_is_number(val2))
    {
      hk_runtime_error("type error: cannot subtract %s from number",
        hk_type_name(val2.type));
      return HK_STATUS_ERROR;
    }
    double data = hk_as_number(val1) - hk_as_number(val2);
    slots[0] = hk_number_value(data);
    --state->stack_top;
    return HK_STATUS_OK;
  }
  if (hk_is_array(val1))
  {
    if (!hk_is_array(val2))
    {
      hk_runtime_error("type error: cannot diff between array and %s",
        hk_type_name(val2.type));
      return HK_STATUS_ERROR;
    }
    return diff_arrays(state, slots, val1, val2);
  }
  hk_runtime_error("type error: cannot subtract %s from %s", hk_type_name(val2.type),
    hk_type_name(val1.type));
  return HK_STATUS_ERROR;
}

static inline int32_t diff_arrays(hk_state_t *state, hk_value_t *slots, hk_value_t val1, hk_value_t val2)
{
  hk_array_t *arr1 = hk_as_array(val1);
  hk_array_t *arr2 = hk_as_array(val2);
  if (!arr1->length || !arr2->length)
  {
    --state->stack_top;
    hk_array_release(arr2);
    return HK_STATUS_OK;
  }
  if (arr1->ref_count == 1)
  {
    hk_array_inplace_diff(arr1, arr2);
    --state->stack_top;
    hk_array_release(arr2);
    return HK_STATUS_OK;
  }
  hk_array_t *result = hk_array_diff(arr1, arr2);
  hk_incr_ref(result);
  slots[0] = hk_array_value(result);
  --state->stack_top;
  hk_array_release(arr1);
  hk_array_release(arr2);
  return HK_STATUS_OK;
}

static inline int32_t do_multiply(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: cannot multiply %s to %s", hk_type_name(val2.type),
      hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  double data = hk_as_number(val1) * hk_as_number(val2);
  slots[0] = hk_number_value(data);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_divide(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: cannot divide %s by %s", hk_type_name(val1.type),
      hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  double data = hk_as_number(val1) / hk_as_number(val2);
  slots[0] = hk_number_value(data);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_quotient(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: cannot apply `quotient` between %s and %s",
      hk_type_name(val1.type), hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  double data = floor(hk_as_number(val1) / hk_as_number(val2));
  slots[0] = hk_number_value(data);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_remainder(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top - 1];
  hk_value_t val1 = slots[0];
  hk_value_t val2 = slots[1];
  if (!hk_is_number(val1) || !hk_is_number(val2))
  {
    hk_runtime_error("type error: cannot apply `remainder` between %s and %s",
      hk_type_name(val1.type), hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  double data = fmod(hk_as_number(val1), hk_as_number(val2));
  slots[0] = hk_number_value(data);
  --state->stack_top;
  return HK_STATUS_OK;
}

static inline int32_t do_negate(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top];
  hk_value_t val = slots[0];
  if (!hk_is_number(val))
  {
    hk_runtime_error("type error: cannot apply `negate` to %s", hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  double data = -hk_as_number(val);
  slots[0] = hk_number_value(data);
  return HK_STATUS_OK;
}

static inline void do_not(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top];
  hk_value_t val = slots[0];
  slots[0] = hk_is_falsey(val) ? HK_TRUE_VALUE : HK_FALSE_VALUE;
  hk_value_release(val);
}

static inline int32_t do_bitwise_not(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top];
  hk_value_t val = slots[0];
  if (!hk_is_number(val))
  {
    hk_runtime_error("type error: cannot apply `bitwise not` to %s", hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  int64_t data = ~((int64_t) hk_as_number(val));
  slots[0] = hk_number_value(data);
  return HK_STATUS_OK;
}

static inline int32_t do_increment(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top];
  hk_value_t val = slots[0];
  if (!hk_is_number(val))
  {
    hk_runtime_error("type error: cannot increment value of type %s",
      hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  ++slots[0].as.number_value;
  return HK_STATUS_OK;
}

static inline int32_t do_decrement(hk_state_t *state)
{
  hk_value_t *slots = &state->stack[state->stack_top];
  hk_value_t val = slots[0];
  if (!hk_is_number(val))
  {
    hk_runtime_error("type error: cannot decrement value of type %s",
      hk_type_name(val.type));
    return HK_STATUS_ERROR;
  }
  --slots[0].as.number_value;
  return HK_STATUS_OK;
}

static inline int32_t do_call(hk_state_t *state, int32_t num_args)
{
  hk_value_t *slots = &state->stack[state->stack_top - num_args];
  hk_value_t val = slots[0];
  if (!hk_is_callable(val))
  {
    hk_runtime_error("type error: cannot call value of type %s",
      hk_type_name(val.type));
    discard_frame(state, slots);
    return HK_STATUS_ERROR;
  }
  if (hk_is_native(val))
  {
    hk_native_t *native = hk_as_native(val);
    if (adjust_call_args(state, native->arity, num_args) == HK_STATUS_ERROR)
    {
      discard_frame(state, slots);
      return HK_STATUS_ERROR;
    }
    int32_t status;
    if ((status = native->call(state, slots)) != HK_STATUS_OK)
    {
      if (status != HK_STATUS_NO_TRACE)
        print_trace(native->name, NULL, 0);
      discard_frame(state, slots);
      return HK_STATUS_ERROR;
    }
    hk_native_release(native);
    move_result(state, slots);
    return HK_STATUS_OK;
  }
  hk_closure_t *cl = hk_as_closure(val);
  hk_function_t *fn = cl->fn;
  if (adjust_call_args(state, fn->arity, num_args) == HK_STATUS_ERROR)
  {
    discard_frame(state, slots);
    return HK_STATUS_ERROR;
  }
  int32_t line;
  if (call_function(state, slots, cl, &line) == HK_STATUS_ERROR)
  {
    print_trace(fn->name, fn->file, line);
    discard_frame(state, slots);
    return HK_STATUS_ERROR;
  }
  hk_closure_release(cl);
  move_result(state, slots);
  return HK_STATUS_OK;
}

static inline int32_t adjust_call_args(hk_state_t *state, int32_t arity,int32_t num_args)
{
  if (num_args >= arity)
    return HK_STATUS_OK;
  while (num_args < arity)
  {
    if (push(state, HK_NIL_VALUE) == HK_STATUS_ERROR)
      return HK_STATUS_ERROR;
    ++num_args;
  }
  return HK_STATUS_OK;
}

static inline void print_trace(hk_string_t *name, hk_string_t *file, int32_t line)
{
  char *name_chars = name ? name->chars : "<anonymous>";
  if (file)
  {
    fprintf(stderr, "  at %s() in %.*s:%d\n", name_chars, file->length, file->chars, line);
    return;
  }
  fprintf(stderr, "  at %s() in <native>\n", name_chars);
}

static inline int32_t call_function(hk_state_t *state, hk_value_t *locals, hk_closure_t *cl, int32_t *line)
{
  hk_value_t *slots = state->stack;
  hk_function_t *fn = cl->fn;
  hk_value_t *nonlocals = cl->nonlocals;
  hk_chunk_t *chunk = &fn->chunk;
  uint8_t *code = chunk->code;
  hk_value_t *consts = chunk->consts->elements;
  hk_function_t **functions = fn->functions;
  uint8_t *pc = code;
  for (;;)
  {
    hk_opcode_t op = (hk_opcode_t) read_byte(&pc);
    switch (op)
    {
    case HK_OP_NIL:
      if (push(state, HK_NIL_VALUE) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_FALSE:
      if (push(state, HK_FALSE_VALUE) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_TRUE:
      if (push(state, HK_TRUE_VALUE) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_INT:
      if (push(state, hk_number_value(read_word(&pc))) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_CONSTANT:
      {
        hk_value_t val = consts[read_byte(&pc)];
        if (push(state, val) == HK_STATUS_ERROR)
          goto error;
        hk_value_incr_ref(val);
      }
      break;
    case HK_OP_RANGE:
      if (do_range(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_ARRAY:
      if (do_array(state, read_byte(&pc)) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_STRUCT:
      if (do_struct(state, read_byte(&pc)) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_INSTANCE:
      if (do_instance(state, read_byte(&pc)) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_CONSTRUCT:
      if (do_construct(state, read_byte(&pc)) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_ITERATOR:
      if (do_iterator(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_CLOSURE:
      if (do_closure(state, functions[read_byte(&pc)]) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_UNPACK_ARRAY:
      if (do_unpack_array(state, read_byte(&pc)) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_UNPACK_STRUCT:
      if (do_unpack_struct(state, read_byte(&pc)) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_POP:
      hk_value_release(slots[state->stack_top--]);
      break;
    case HK_OP_GLOBAL:
      {
        hk_value_t val = slots[read_byte(&pc)];
        if (push(state, val) == HK_STATUS_ERROR)
          goto error;
        hk_value_incr_ref(val);
      }
      break;
    case HK_OP_NONLOCAL:
      {
        hk_value_t val = nonlocals[read_byte(&pc)];
        if (push(state, val) == HK_STATUS_ERROR)
          goto error;
        hk_value_incr_ref(val);
      }
      break;
    case HK_OP_LOAD:
      {
        hk_value_t val = locals[read_byte(&pc)];
        if (push(state, val) == HK_STATUS_ERROR)
          goto error;
        hk_value_incr_ref(val);
      }
      break;
    case HK_OP_STORE:
      {
        int32_t index = read_byte(&pc);
        hk_value_t val = slots[state->stack_top];
        --state->stack_top;
        hk_value_release(locals[index]);
        locals[index] = val;
      }
      break;
    case HK_OP_ADD_ELEMENT:
      if (do_add_element(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_GET_ELEMENT:
      if (do_get_element(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_FETCH_ELEMENT:
      if (do_fetch_element(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_SET_ELEMENT:
      do_set_element(state);
      break;
    case HK_OP_PUT_ELEMENT:
      if (do_put_element(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_DELETE_ELEMENT:
      if (do_delete_element(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_INPLACE_ADD_ELEMENT:
      if (do_inplace_add_element(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_INPLACE_PUT_ELEMENT:
      if (do_inplace_put_element(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_INPLACE_DELETE_ELEMENT:
      if (do_inplace_delete_element(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_GET_FIELD:
      if (do_get_field(state, hk_as_string(consts[read_byte(&pc)])) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_FETCH_FIELD:
      if (do_fetch_field(state, hk_as_string(consts[read_byte(&pc)])) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_SET_FIELD:
      do_set_field(state);
      break;
    case HK_OP_PUT_FIELD:
      if (do_put_field(state, hk_as_string(consts[read_byte(&pc)])) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_INPLACE_PUT_FIELD:
      if (do_inplace_put_field(state, hk_as_string(consts[read_byte(&pc)])) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_CURRENT:
      do_current(state);
      break;
    case HK_OP_JUMP:
      pc = &code[read_word(&pc)];
      break;
    case HK_OP_JUMP_IF_FALSE:
      {
        int32_t offset = read_word(&pc);
        hk_value_t val = slots[state->stack_top];
        if (hk_is_falsey(val))
          pc = &code[offset];
        hk_value_release(val);
        --state->stack_top;
      }
      break;
    case HK_OP_JUMP_IF_TRUE:
      {
        int32_t offset = read_word(&pc);
        hk_value_t val = slots[state->stack_top];
        if (hk_is_truthy(val))
          pc = &code[offset];
        hk_value_release(val);
        --state->stack_top;
      }
      break;
    case HK_OP_JUMP_IF_TRUE_OR_POP:
      {
        int32_t offset = read_word(&pc);
        hk_value_t val = slots[state->stack_top];
        if (hk_is_truthy(val))
        {
          pc = &code[offset];
          break;
        }
        hk_value_release(val);
        --state->stack_top;
      }
      break;
    case HK_OP_JUMP_IF_FALSE_OR_POP:
      {
        int32_t offset = read_word(&pc);
        hk_value_t val = slots[state->stack_top];
        if (hk_is_falsey(val))
        {
          pc = &code[offset];
          break;
        }
        hk_value_release(val);
        --state->stack_top;
      }
      break;
    case HK_OP_JUMP_IF_NOT_EQUAL:
      {
        int32_t offset = read_word(&pc);
        hk_value_t val1 = slots[state->stack_top - 1];
        hk_value_t val2 = slots[state->stack_top];
        if (hk_value_equal(val1, val2))
        {
          hk_value_release(val1);
          hk_value_release(val2);
          state->stack_top -= 2;
          break;
        }
        pc = &code[offset];
        hk_value_release(val2);
        --state->stack_top;
      }
      break;
    case HK_OP_JUMP_IF_NOT_VALID:
      {
        int32_t offset = read_word(&pc);
        hk_value_t val = slots[state->stack_top];
        hk_iterator_t *it = hk_as_iterator(val);
        if (!hk_iterator_is_valid(it))
          pc = &code[offset];
      }
      break;
    case HK_OP_NEXT:
      do_next(state);
      break;
    case HK_OP_EQUAL:
      do_equal(state);
      break;
    case HK_OP_GREATER:
      if (do_greater(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_LESS:
      if (do_less(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_NOT_EQUAL:
      do_not_equal(state);
      break;
    case HK_OP_NOT_GREATER:
      if (do_not_greater(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_NOT_LESS:
      if (do_not_less(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_BITWISE_OR:
      if (do_bitwise_or(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_BITWISE_XOR:
      if (do_bitwise_xor(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_BITWISE_AND:
      if (do_bitwise_and(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_LEFT_SHIFT:
      if (do_left_shift(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_RIGHT_SHIFT:
      if (do_right_shift(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_ADD:
      if (do_add(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_SUBTRACT:
      if (do_subtract(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_MULTIPLY:
      if (do_multiply(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_DIVIDE:
      if (do_divide(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_QUOTIENT:
      if (do_quotient(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_REMAINDER:
      if (do_remainder(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_NEGATE:
      if (do_negate(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_NOT:
      do_not(state);
      break;
    case HK_OP_BITWISE_NOT:
      if (do_bitwise_not(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_INCREMENT:
      if (do_increment(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_DECREMENT:
      if (do_decrement(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_CALL:
      if (do_call(state, read_byte(&pc)) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_LOAD_MODULE:
      if (load_module(state) == HK_STATUS_ERROR)
        goto error;
      break;
    case HK_OP_RETURN:
      return HK_STATUS_OK;
    case HK_OP_RETURN_NIL:
      if (push(state, HK_NIL_VALUE) == HK_STATUS_ERROR)
        goto error;
      return HK_STATUS_OK;
    }
  }
error:
  *line = hk_chunk_get_line(chunk, (int32_t) (pc - code));
  return HK_STATUS_ERROR;
}

static inline void discard_frame(hk_state_t *state, hk_value_t *slots)
{
  while (&state->stack[state->stack_top] >= slots)
    hk_value_release(state->stack[state->stack_top--]);
}

static inline void move_result(hk_state_t *state, hk_value_t *slots)
{
  slots[0] = state->stack[state->stack_top];
  --state->stack_top;
  while (&state->stack[state->stack_top] > slots)
    hk_value_release(state->stack[state->stack_top--]);
}

void hk_state_init(hk_state_t *state, int32_t min_capacity)
{
  int32_t capacity = min_capacity < HK_STACK_MIN_CAPACITY ? HK_STACK_MIN_CAPACITY : min_capacity;
  capacity = hk_power_of_two_ceil(capacity);
  state->stack_end = capacity - 1;
  state->stack_top = -1;
  state->stack = (hk_value_t *) hk_allocate(sizeof(*state->stack) * capacity);
  load_globals(state);
  init_module_cache();
}

void hk_state_free(hk_state_t *state)
{
  free_module_cache();
  hk_assert(state->stack_top == num_globals() - 1, "stack must contain the globals");
  while (state->stack_top > -1)
    hk_value_release(state->stack[state->stack_top--]);
  free(state->stack);
}

int32_t hk_state_push(hk_state_t *state, hk_value_t val)
{
  if (push(state, val) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_value_incr_ref(val);
  return HK_STATUS_OK;
}

int32_t hk_state_push_nil(hk_state_t *state)
{
  return push(state, HK_NIL_VALUE);
}

int32_t hk_state_push_bool(hk_state_t *state, bool data)
{
  return push(state, data ? HK_TRUE_VALUE : HK_FALSE_VALUE);
}

int32_t hk_state_push_number(hk_state_t *state, double data)
{
  return push(state, hk_number_value(data));
}

int32_t hk_state_push_string(hk_state_t *state, hk_string_t *str)
{
  if (push(state, hk_string_value(str)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_incr_ref(str);
  return HK_STATUS_OK;
}

int32_t hk_state_push_string_from_chars(hk_state_t *state, int32_t length, const char *chars)
{   
  hk_string_t *str = hk_string_from_chars(length, chars);
  if (hk_state_push_string(state, str) == HK_STATUS_ERROR)
  {
    hk_string_free(str);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

int32_t hk_state_push_string_from_stream(hk_state_t *state, FILE *stream, const char terminal)
{
  hk_string_t *str = hk_string_from_stream(stream, terminal);
  if (hk_state_push_string(state, str) == HK_STATUS_ERROR)
  {
    hk_string_free(str);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

int32_t hk_state_push_range(hk_state_t *state, hk_range_t *range)
{
  if (push(state, hk_range_value(range)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_incr_ref(range);
  return HK_STATUS_OK;
}

int32_t hk_state_push_array(hk_state_t *state, hk_array_t *arr)
{
  if (push(state, hk_array_value(arr)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_incr_ref(arr);
  return HK_STATUS_OK;
}

int32_t hk_state_push_struct(hk_state_t *state, hk_struct_t *ztruct)
{
  if (push(state, hk_struct_value(ztruct)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_incr_ref(ztruct);
  return HK_STATUS_OK;
}

int32_t hk_state_push_instance(hk_state_t *state, hk_instance_t *inst)
{
  if (push(state, hk_instance_value(inst)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_incr_ref(inst);
  return HK_STATUS_OK;
}

int32_t hk_state_push_iterator(hk_state_t *state, hk_iterator_t *it)
{
  if (push(state, hk_iterator_value(it)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_incr_ref(it);
  return HK_STATUS_OK;
}

int32_t hk_state_push_closure(hk_state_t *state, hk_closure_t *cl)
{
  if (push(state, hk_closure_value(cl)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_incr_ref(cl);
  return HK_STATUS_OK;
}

int32_t hk_state_push_native(hk_state_t *state, hk_native_t *native)
{
  if (push(state, hk_native_value(native)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_incr_ref(native);
  return HK_STATUS_OK;
}

int32_t hk_state_push_new_native(hk_state_t *state, const char *name, int32_t arity, int32_t (*call)(hk_state_t *, hk_value_t *))
{
  hk_native_t *native = hk_native_new(hk_string_from_chars(-1, name), arity, call);
  if (hk_state_push_native(state, native) == HK_STATUS_ERROR)
  {
    hk_native_free(native);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

int32_t hk_state_push_userdata(hk_state_t *state, hk_userdata_t *udata)
{
  if (push(state, hk_userdata_value(udata)) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  hk_incr_ref(udata);
  return HK_STATUS_OK;
}

int32_t hk_state_array(hk_state_t *state, int32_t length)
{
  return do_array(state, length);
}

int32_t hk_state_struct(hk_state_t *state, int32_t length)
{
  return do_struct(state, length);
}

int32_t hk_state_instance(hk_state_t *state, int32_t num_args)
{
  return do_instance(state, num_args);
}

int32_t hk_state_construct(hk_state_t *state, int32_t length)
{
  return do_construct(state, length);
}

void hk_state_pop(hk_state_t *state)
{
  pop(state);
}

int32_t hk_state_call(hk_state_t *state, int32_t num_args)
{
  return do_call(state, num_args);
}

int32_t hk_state_compare(hk_value_t val1, hk_value_t val2, int32_t *result)
{
  if (!hk_is_comparable(val1))
  {
    hk_runtime_error("type error: value of type %s is not comparable", hk_type_name(val1.type));
    return HK_STATUS_ERROR;
  }
  if (val1.type != val2.type)
  {
    hk_runtime_error("type error: cannot compare %s and %s", hk_type_name(val1.type),
      hk_type_name(val2.type));
    return HK_STATUS_ERROR;
  }
  hk_assert(hk_value_compare(val1, val2, result), "hk_value_compare failed");
  return HK_STATUS_OK; 
}
