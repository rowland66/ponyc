#include <platform.h>

#ifdef PLATFORM_IS_POSIX_BASED

#include "lsda.h"
#include "ponyassert.h"
#include <unwind.h>
#include <backtrace.h>
#include <backtrace-supported.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_IS_ARM32
#define PONY_EXCEPTION_CLASS "Pony\0\0\0\0"
#else
#define PONY_EXCEPTION_CLASS 0x506F6E7900000000 // "Pony"
#endif

#ifndef ATTRIBUTE_UNUSED
# define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif

PONY_EXTERN_C_BEGIN

struct _Frame_Ptr_List_Element {
  uintptr_t frame_ptr;
  struct _Frame_Ptr_List_Element *next_element;
};

struct _Pony_Error {
  struct _Unwind_Exception unwind_exception;
  char const *desc;
  struct _Frame_Ptr_List_Element *first_frame_ptr_list_element;
  struct _Pony_Error *caused_by;
};

static __pony_thread_local struct _Pony_Error *exception;
static __pony_thread_local uintptr_t landing_pad;

// Required by the backtrace library.
static struct backtrace_state *bt_state = NULL;

static void exception_cleanup(_Unwind_Reason_Code reason,
  struct _Unwind_Exception* exception)
{
  (void)reason;
  (void)exception;
}

/**
 Callback function to deliver a backtrace line to the Pony runtime. The pony_stream is an OutStream to which the line
 should be written. The pony_buffer_string is a Pony String that will contain the backtrace line text. The Pony String
 should be truncated to the backtrace_line_length in the Pony function. The backtrace_line_buffer should not be used
 in the Pony callback function. It is used by the internal_backtrace_callback function below to write backtrace lines to
 stderr on runtime panics.
*/
typedef void (*Pony_Backtrace_Callback)(void* pony_stream,
                                        void* pony_buffer_string,
                                        char* backtrace_line_buffer,
                                        int backtrace_line_length);

/**
 A pointer to this structure is passed to bt_full_callback as data. The callback function can be used to callback into
 the Pony runtime to print backtrace lines. The pony_stream and pony_buffer_string are pony allocated objects. The
 backtrace_line_buffer is a cpointer from the pony_buffer_string. backtrace_line_buffer_length is the size of the buffer
 allocated in pony_buffer_string.
*/
struct _pony_callback_data {
  Pony_Backtrace_Callback callback;
  void* pony_stream;
  void* pony_buffer_string;
  char* backtrace_line_buffer;
  int backtrace_line_buffer_length;
  int include_lf;
};

static void strreplace(char s[], char chr[], char repl_chr[], int n)
{
  int i=0;
  int j=0;
  while(s[i]!='\0') {
    if(s[i]==chr[j]) {
      if(j < n) {
        j++;
      }
      if(j == n) {
        i = i - j + 1;
        j = 0;
        while(j<n) {
          s[i]=repl_chr[j];
          j++;
          i++;
        }
        j = 0;
      }
    } else {
      i = i - j;
      j = 0;
    }
    i++;
  }
}

enum DEMANGLE_STATE{DEMANGLE_STATE_START, DEMANGLE_STATE_TYPE_PARAMETERS, DEMANGLE_STATE_CAPABILITIES, DEMANGLE_STATE_FUNCTION_NAME};

/**
 Demangle a Pony function name. Current logic tokenizes the function name on '_', takes the first token
 as the class/actor/primitive name. Looks for type parameters to include []'s. Drops any number of capability
 tokens. Adds a '.', and all of the remaining tokens except the last separated by '_'.
*/
static char* demangle_function_name(char* function_name, char* buffer) {
  strreplace(function_name, "__", "_*", 2); // replace double '_' with "_*" so that strtok does not consume both '_'
  char* token = strtok(function_name, "_");
  char last_func_name_part[256];
  int append_underscore = 0;
  int type_param_found = 0;
  enum DEMANGLE_STATE state = DEMANGLE_STATE_START;
  while (token != NULL) {
    switch (state) {
      case DEMANGLE_STATE_START :
        strcpy(buffer, token);
        state = DEMANGLE_STATE_TYPE_PARAMETERS;
        break;
      case DEMANGLE_STATE_TYPE_PARAMETERS :
        if (isupper(token[0])) {
          if (type_param_found) {
            strcat(buffer, ",");
          } else {
            strcat(buffer, "[");
            type_param_found = 1;
          }
          strcat(buffer, token);
          token = strtok(NULL, "_");
          strcat(buffer, " ");
          strcat(buffer, token);
          break;
        } else {
          if (type_param_found) {
            strcat(buffer, "]");
          }
          state = DEMANGLE_STATE_CAPABILITIES;
        }
      case DEMANGLE_STATE_CAPABILITIES :
        if (strcmp("val", token) == 0 || strcmp("ref", token) == 0 || strcmp("iso", token) == 0 ||
            strcmp("box", token) == 0 || strcmp("tag", token) == 0 || strcmp("trn", token) == 0) {
          //nop
        } else {
          state = DEMANGLE_STATE_FUNCTION_NAME;
          strcpy(last_func_name_part, token);
          if (last_func_name_part[0] == '*') { // undo the replacement character we inserted above
            last_func_name_part[0] = '_';
          }
          strcat(buffer, ".");
        }
        break;
      case DEMANGLE_STATE_FUNCTION_NAME :
        if (append_underscore) {
          strcat(buffer, "_");
        } else {
          append_underscore = 1;
        }
        strcat(buffer, last_func_name_part);
        strcpy(last_func_name_part, token);
        break;
    }
    token = strtok(NULL, "_");
  }
  return buffer;
}

static int bt_full_callback(void *data, uintptr_t pc ATTRIBUTE_UNUSED,
                                        const char *filename, int lineno,
                                        const char *function)
{
  if (filename == NULL || strcmp("handle_message", function) == 0) {
    return 1;
  }
  char demangled_function[256];
  char function_copy[256];
  strcpy(function_copy, function);
  demangle_function_name(function_copy, demangled_function);

  struct _pony_callback_data *pony_callback_data = (struct _pony_callback_data*) data;
  int line_length = snprintf(pony_callback_data->backtrace_line_buffer,
                             pony_callback_data->backtrace_line_buffer_length,
                             "  %s at %s:%d%c", demangled_function, filename, lineno, (pony_callback_data->include_lf ? '\n' : '\0'));
  pony_callback_data->callback(pony_callback_data->pony_stream,
                               pony_callback_data->pony_buffer_string,
                               pony_callback_data->backtrace_line_buffer,
                               line_length);
  return 0;
}

static int bt_simple_callback(void *data, uintptr_t pc)
{
  struct _Frame_Ptr_List_Element **first_element = (struct _Frame_Ptr_List_Element**) data;
  struct _Frame_Ptr_List_Element *next_element = *first_element;
  struct _Frame_Ptr_List_Element *prev_element = NULL;
  while (next_element != NULL) {
    prev_element = next_element;
    next_element = next_element->next_element;
  }
  struct _Frame_Ptr_List_Element *new_element = malloc(sizeof(struct _Frame_Ptr_List_Element));
  new_element->frame_ptr = pc;
  new_element->next_element = NULL;
  if (prev_element == NULL) {
    *first_element = new_element;
    next_element = new_element;
  } else {
    prev_element->next_element = new_element;
  }
  return 0;
}

static void bt_error_callback(void *data ATTRIBUTE_UNUSED, const char *msg,
                                          int errnum ATTRIBUTE_UNUSED)
{
  fprintf(stderr, "Error: %s\n", msg);
}

static int duplicate_stack(struct _Frame_Ptr_List_Element *next_element, struct _Pony_Error *error)
{
    if (error == NULL) {
      return 0; // No caused by error always returns false;
    }
    struct _Frame_Ptr_List_Element *caused_by_next_element = error->first_frame_ptr_list_element;
    while (caused_by_next_element != NULL && next_element->frame_ptr != caused_by_next_element->frame_ptr) {
      caused_by_next_element = caused_by_next_element->next_element;
    }

    if (caused_by_next_element == NULL) {
      return 0;
    }

    while (next_element->frame_ptr == caused_by_next_element->frame_ptr) {
      next_element = next_element->next_element;
      caused_by_next_element = caused_by_next_element->next_element;
      if (next_element == NULL && caused_by_next_element == NULL) {
        return 1;
      }
    }
    return 0;
}

static void print_backtrace(struct _Frame_Ptr_List_Element *next_element,
                            struct _Pony_Error *parent_error,
                            struct _pony_callback_data *pony_callback_data)
{
  while (next_element != NULL && !duplicate_stack(next_element, parent_error)) {
    if (backtrace_pcinfo(bt_state, next_element->frame_ptr, bt_full_callback, bt_error_callback, pony_callback_data) != 0) {
      break;
    }
    next_element = next_element->next_element;
  }
}

PONY_API void pony_error(char const *msg)
{
  if (bt_state == NULL) {
    bt_state = backtrace_create_state(NULL, BACKTRACE_SUPPORTS_THREADS, bt_error_callback, NULL);
  }

  struct _Pony_Error *existing_exception = NULL;
  if ((exception != NULL && strcmp(msg, "Unlabeled") != 0) || exception == NULL) {

    if (exception != NULL) {
      existing_exception = exception;
    }

    exception = malloc(sizeof(struct _Pony_Error));

    *exception = (struct _Pony_Error){ 0 };
    exception->desc = msg;
    exception->caused_by = existing_exception;

    backtrace_simple(bt_state, 1, bt_simple_callback, bt_error_callback, &exception->first_frame_ptr_list_element);

#ifdef PLATFORM_IS_ARM32
    memcpy(exception->unwind_exception.exception_class, PONY_EXCEPTION_CLASS, 8);
#else
    exception->unwind_exception.exception_class = PONY_EXCEPTION_CLASS;
#endif
    exception->unwind_exception.exception_cleanup = exception_cleanup;
  }
  _Unwind_RaiseException((_Unwind_Exception*) exception);

  abort();
}

static void free_backtrace_frame(struct _Frame_Ptr_List_Element *frame)
{
  if (frame->next_element != NULL) {
    free_backtrace_frame(frame->next_element);
    free(frame->next_element);
  }
}

static void cleanup_exception(struct _Pony_Error *exception)
{
  if (exception->caused_by != NULL) {
    cleanup_exception(exception->caused_by);
    free(exception->caused_by);
  }

  if (exception->first_frame_ptr_list_element != NULL) {
    free_backtrace_frame(exception->first_frame_ptr_list_element);
    free(exception->first_frame_ptr_list_element);
  }
}

PONY_API void pony_error_cleanup() {
  if (exception == NULL) {
    return;
  }

  cleanup_exception(exception);
  free(exception);
  exception = NULL;
}

static void set_registers(struct _Unwind_Exception* exception,
  struct _Unwind_Context* context)
{
  _Unwind_SetGR(context, __builtin_eh_return_data_regno(0),
    (uintptr_t)exception);
  _Unwind_SetGR(context, __builtin_eh_return_data_regno(1), 0);
  _Unwind_SetIP(context, landing_pad);
}

// Switch to ARM EHABI for ARM32 devices.
// Note that this does not apply to ARM64 devices which use DWARF Exception Handling.
#ifdef PLATFORM_IS_ARM32

_Unwind_Reason_Code __gnu_unwind_frame(_Unwind_Exception*, _Unwind_Context*);

static _Unwind_Reason_Code continue_unwind(_Unwind_Exception* exception,
  _Unwind_Context* context)
{
  if(__gnu_unwind_frame(exception, context) != _URC_OK)
    return _URC_FAILURE;

  return _URC_CONTINUE_UNWIND;
}

PONY_API _Unwind_Reason_Code ponyint_personality_v0(_Unwind_State state,
  _Unwind_Exception* exception, _Unwind_Context* context)
{
  if(exception == NULL || context == NULL)
    return _URC_FAILURE;

  if(memcmp(exception->exception_class, PONY_EXCEPTION_CLASS, 8) != 0)
    return continue_unwind(exception, context);

  // Save exception in r12.
  _Unwind_SetGR(context, 12, (uintptr_t)exception);

  switch(state & _US_ACTION_MASK)
  {
    case _US_VIRTUAL_UNWIND_FRAME:
    {
      if(!ponyint_lsda_scan(context, &landing_pad))
        return continue_unwind(exception, context);

      // Save r13.
      exception->barrier_cache.sp = _Unwind_GetGR(context, 13);

      // Save barrier.
      exception->barrier_cache.bitpattern[0] = 0;
      exception->barrier_cache.bitpattern[1] = 0;
      exception->barrier_cache.bitpattern[2] =
        (uint32_t)_Unwind_GetLanguageSpecificData(context);
      exception->barrier_cache.bitpattern[3] = (uint32_t)landing_pad;
      exception->barrier_cache.bitpattern[4] = 0;
      return _URC_HANDLER_FOUND;
    }

    case _US_UNWIND_FRAME_STARTING:
    {

      if(exception->barrier_cache.sp == _Unwind_GetGR(context, 13))
      {
        // Load barrier.
        landing_pad = exception->barrier_cache.bitpattern[3];

        // No need to search again, just set the registers.
        set_registers(exception, context);
        return _URC_INSTALL_CONTEXT;
      }

      return continue_unwind(exception, context);
    }

    case _US_UNWIND_FRAME_RESUME:
      return continue_unwind(exception, context);

    default:
      abort();
  }

  return _URC_FAILURE;
}

#else

PONY_API _Unwind_Reason_Code ponyint_personality_v0(int version,
  _Unwind_Action actions, uint64_t ex_class,
  struct _Unwind_Exception* exception, struct _Unwind_Context* context)
{
  if(version != 1 || exception == NULL || context == NULL)
    return _URC_FATAL_PHASE1_ERROR;

  if(ex_class != PONY_EXCEPTION_CLASS)
    return _URC_CONTINUE_UNWIND;

  // The search phase sets up the landing pad.
  if(actions & _UA_SEARCH_PHASE)
  {
    if(!ponyint_lsda_scan(context, &landing_pad))
      return _URC_CONTINUE_UNWIND;

    return _URC_HANDLER_FOUND;
  }

  if(actions & _UA_CLEANUP_PHASE)
  {
    if(!(actions & _UA_HANDLER_FRAME))
      return _URC_CONTINUE_UNWIND;

    // No need to search again, just set the registers.
    set_registers(exception, context);
    return _URC_INSTALL_CONTEXT;
  }

  return _URC_FATAL_PHASE1_ERROR;
}

#endif

static void print_error_inner(struct _Pony_Error *error,
                              struct _Pony_Error *parent_error,
                              struct _pony_callback_data *pony_callback_data)
{
  int line_length = 0;
  if (error->desc == NULL || strlen(error->desc) == 0) {
    line_length = snprintf(pony_callback_data->backtrace_line_buffer,
                           pony_callback_data->backtrace_line_buffer_length,
                           "Unlabeled error at:%c", (pony_callback_data->include_lf ? '\n' : '\0'));
  } else {
    line_length = snprintf(pony_callback_data->backtrace_line_buffer,
                          pony_callback_data->backtrace_line_buffer_length,
                          "%s error at:%c", error->desc, (pony_callback_data->include_lf ? '\n' : '\0'));
  }
  pony_callback_data->callback(pony_callback_data->pony_stream,
                               pony_callback_data->pony_buffer_string,
                               pony_callback_data->backtrace_line_buffer,
                               line_length);

  print_backtrace(error->first_frame_ptr_list_element, parent_error, pony_callback_data);

  if (error->caused_by != NULL) {
    line_length = snprintf(pony_callback_data->backtrace_line_buffer,
                           pony_callback_data->backtrace_line_buffer_length,
                           "caused by: ");
    pony_callback_data->callback(pony_callback_data->pony_stream,
                                 pony_callback_data->pony_buffer_string,
                                 pony_callback_data->backtrace_line_buffer,
                                 line_length);
    print_error_inner(error->caused_by, error, pony_callback_data);
  }
}

static void internal_backtrace_callback(void* pony_stream ATTRIBUTE_UNUSED,
                                        void* pony_buffer_string ATTRIBUTE_UNUSED,
                                        char* backtrace_line_buffer,
                                        int backtrace_line_length ATTRIBUTE_UNUSED)
{
  fputs(backtrace_line_buffer, stderr);
}

PONY_API void pony_panic()
{
  char* backtrace_line_buffer = malloc(1024);
  struct _pony_callback_data pony_callback_data;
  pony_callback_data.callback = internal_backtrace_callback;
  pony_callback_data.pony_stream = NULL;
  pony_callback_data.pony_buffer_string = NULL;
  pony_callback_data.backtrace_line_buffer = backtrace_line_buffer;
  pony_callback_data.backtrace_line_buffer_length = 1024;
  pony_callback_data.include_lf = 1;
  if (exception != NULL) {
    print_error_inner(exception, NULL, &pony_callback_data);
  } else {
    if (bt_state == NULL) {
      bt_state = backtrace_create_state(NULL, BACKTRACE_SUPPORTS_THREADS, bt_error_callback, NULL);
    }
    struct _Frame_Ptr_List_Element* first_frame_ptr_list_element = NULL;
    backtrace_simple(bt_state, 2, bt_simple_callback, bt_error_callback, &first_frame_ptr_list_element);
    print_backtrace(first_frame_ptr_list_element, NULL, &pony_callback_data);
  }
  free(backtrace_line_buffer);
  abort();
}


PONY_API void pony_backtrace(Pony_Backtrace_Callback callback,
                             void* pony_stream,
                             void* pony_buffer_string,
                             char* backtrace_line_buffer,
                             int backtrace_line_buffer_size,
                             int include_lf)
{
  struct _pony_callback_data pony_callback_data;
  pony_callback_data.callback = callback;
  pony_callback_data.pony_stream = pony_stream;
  pony_callback_data.pony_buffer_string = pony_buffer_string;
  pony_callback_data.backtrace_line_buffer = backtrace_line_buffer;
  pony_callback_data.backtrace_line_buffer_length = backtrace_line_buffer_size;
  pony_callback_data.include_lf = include_lf;
  if (exception != NULL) {
    print_error_inner(exception, NULL, &pony_callback_data);
  } else {
    if (bt_state == NULL) {
      bt_state = backtrace_create_state(NULL, BACKTRACE_SUPPORTS_THREADS, bt_error_callback, NULL);
    }
    struct _Frame_Ptr_List_Element* first_frame_ptr_list_element = NULL;
    backtrace_simple(bt_state, 2, bt_simple_callback, bt_error_callback, &first_frame_ptr_list_element);
    print_backtrace(first_frame_ptr_list_element, NULL, &pony_callback_data);
  }
}

PONY_EXTERN_C_END

#endif
