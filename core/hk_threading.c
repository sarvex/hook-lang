//
// The Hook Programming Language
// hk_threading.c
//

#include "hk_threading.h"
#include "hk_memory.h"
#include "hk_status.h"
#include "hk_error.h"

#ifdef _WIN32
  #include <windows.h>
#endif

#ifndef _WIN32
  #include <pthread.h>
#endif

#ifdef _WIN32
  #define thread_t       HANDLE
  #define thread_mutex_t CRITICAL_SECTION
  #define thread_cond_t  CONDITION_VARIABLE
#endif

#ifndef _WIN32
  #define thread_t       pthread_t
  #define thread_mutex_t pthread_mutex_t
  #define thread_cond_t  pthread_cond_t
#endif

typedef struct
{
  HK_USERDATA_HEADER
  thread_t thread;
} thread_wrapper_t;

typedef struct
{
  HK_USERDATA_HEADER
  thread_mutex_t mutex;
} mutex_wrapper_t;

typedef struct
{
  HK_USERDATA_HEADER
  thread_cond_t cond;
} cond_wrapper_t;

static inline int32_t thread_create(thread_t *thread, void *(*handler)(void *), void *arg);
static inline bool thread_join(thread_t thread, int32_t *exit_code);
static inline thread_wrapper_t *thread_wrapper_new(thread_t thread);
static inline mutex_wrapper_t *mutex_wrapper_new(thread_mutex_t mutex);
static inline cond_wrapper_t *cond_wrapper_new(thread_cond_t cond);
static void thread_wrapper_deinit(hk_userdata_t *udata);
static void mutex_wrapper_deinit(hk_userdata_t *udata);
static void cond_wrapper_deinit(hk_userdata_t *udata);
static void *thread_handler(void *arg);
static int32_t new_thread_call(hk_vm_t *vm, hk_value_t *args);
static int32_t new_mutex_call(hk_vm_t *vm, hk_value_t *args);
static int32_t new_cond_call(hk_vm_t *vm, hk_value_t *args);

static inline int32_t thread_create(thread_t *thread, void *(*handler)(void *), void *arg)
{
#ifdef _WIN32
  *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) handler, arg, 0, NULL);
  return !(*thread) ? -1 : 0;
#else
  return pthread_create(thread, NULL, handler, arg);
#endif
}

static inline bool thread_join(thread_t thread, int32_t *exit_code)
{
#ifdef _WIN32
  bool result = WaitForSingleObject(th, INFINITE) == WAIT_OBJECT_0;
  GetExitCodeThread(th, (LPDWORD) exit_code);
  return result;
#else
  return !pthread_join(thread, (void **) exit_code);
#endif
}

static inline thread_wrapper_t *thread_wrapper_new(thread_t thread)
{
  thread_wrapper_t *wrapper = (thread_wrapper_t *) hk_allocate(sizeof(*wrapper));
  hk_userdata_init((hk_userdata_t *) wrapper, &thread_wrapper_deinit);
  wrapper->thread = thread;
  return wrapper;
}

static inline mutex_wrapper_t *mutex_wrapper_new(thread_mutex_t mutex)
{
  mutex_wrapper_t *wrapper = (mutex_wrapper_t *) hk_allocate(sizeof(*wrapper));
  hk_userdata_init((hk_userdata_t *) wrapper, &mutex_wrapper_deinit);
  wrapper->mutex = mutex;
  return wrapper;
}

static inline cond_wrapper_t *cond_wrapper_new(thread_cond_t cond)
{
  cond_wrapper_t *wrapper = (cond_wrapper_t *) hk_allocate(sizeof(*wrapper));
  hk_userdata_init((hk_userdata_t *) wrapper, &cond_wrapper_deinit);
  wrapper->cond = cond;
  return wrapper;
}

static void thread_wrapper_deinit(hk_userdata_t *udata)
{
  thread_t thread = ((thread_wrapper_t *) udata)->thread;
  if (thread == NULL)
    return;
  thread_join(thread, NULL);
}

static void mutex_wrapper_deinit(hk_userdata_t *udata)
{
  thread_mutex_t mutex = ((mutex_wrapper_t *) udata)->mutex;
#ifdef _WIN32
  DeleteCriticalSection(&mutex);
#else
  pthread_mutex_destroy(&mutex);
#endif
}

static void cond_wrapper_deinit(hk_userdata_t *udata)
{
#ifdef _WIN32
  (void) udata;
#else
  thread_cond_t cond = ((cond_wrapper_t *) udata)->cond;
  pthread_cond_destroy(&cond);
#endif
}

// ISSUE: This function is not thread-safe
static void *thread_handler(void *arg)
{
  hk_vm_t *vm = (hk_vm_t *) arg;
  hk_vm_call(vm, 0);
  hk_vm_pop(vm);
  return NULL;
}

static int32_t new_thread_call(hk_vm_t *vm, hk_value_t *args)
{
  if (hk_vm_check_callable(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  thread_t thread;
  if (thread_create(&thread, &thread_handler, vm) == -1)
  {
    hk_runtime_error("cannot create thread");
    return HK_STATUS_ERROR;
  }
  thread_wrapper_t *wrapper = thread_wrapper_new(thread);
  return hk_vm_push_userdata(vm, (hk_userdata_t *) wrapper);
}

static int32_t new_mutex_call(hk_vm_t *vm, hk_value_t *args)
{
  // TODO: Implement this function
  (void) args;
  (void) mutex_wrapper_new;
  return hk_vm_push_nil(vm);
}

static int32_t new_cond_call(hk_vm_t *vm, hk_value_t *args)
{
  // TODO: Implement this function
  (void) args;
  (void) cond_wrapper_new;
  return hk_vm_push_nil(vm);
}

static int32_t join_call(hk_vm_t *vm, hk_value_t *args)
{
  if (hk_vm_check_userdata(args, 1) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  thread_wrapper_t *wrapper = (thread_wrapper_t *) hk_as_userdata(args[1]);
  thread_t thread = wrapper->thread;
  if (thread == NULL)
    return hk_vm_push_nil(vm);
  int32_t exit_code;
  bool ok = thread_join(thread, &exit_code);
  wrapper->thread = NULL;
  hk_array_t *result = hk_array_new_with_capacity(2);
  result->length = 2;
  result->elements[0] = ok ? HK_TRUE_VALUE : HK_FALSE_VALUE;
  result->elements[1] = hk_number_value(exit_code);
  return hk_vm_push_array(vm, result);
}

static int32_t lock_call(hk_vm_t *vm, hk_value_t *args)
{
  // TODO: Implement this function
  (void) args;
  return hk_vm_push_nil(vm);
}

static int32_t unlock_call(hk_vm_t *vm, hk_value_t *args)
{
  // TODO: Implement this function
  (void) args;
  return hk_vm_push_nil(vm);
}

static int32_t wait_call(hk_vm_t *vm, hk_value_t *args)
{
  // TODO: Implement this function
  (void) args;
  return hk_vm_push_nil(vm);
}

static int32_t signal_call(hk_vm_t *vm, hk_value_t *args)
{
  // TODO
  (void) args;
  return hk_vm_push_nil(vm);
}

HK_LOAD_FN(threading)
{
  if (hk_vm_push_string_from_chars(vm, -1, "threading") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "new_thread") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "new_thread", 1, &new_thread_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "new_mutex") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "new_mutex", 0, &new_mutex_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "new_cond") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "new_cond", 0, &new_cond_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "join") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "join", 1, &join_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "lock") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "lock", 1, &lock_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "unlock") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "unlock", 1, &unlock_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "wait") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "wait", 2, &wait_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_string_from_chars(vm, -1, "signal") == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  if (hk_vm_push_new_native(vm, "signal", 1, &signal_call) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  return hk_vm_construct(vm, 8);
}
