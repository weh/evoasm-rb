#include "awasm-alloc.h"

#include <stdlib.h>
#include <errno.h>

void *
awasm_malloc(size_t size) {
  void *ptr = malloc(size);
  if(AWASM_UNLIKELY(!ptr)) {
    awasm_set_error(AWASM_ERROR_TYPE_MEMORY, AWASM_ERROR_CODE_NONE,
      NULL, "Allocationg %zu bytes via malloc failed", size);
    return NULL;
  }
  return ptr;
}

void *
awasm_calloc(size_t n, size_t size) {
  void *ptr = calloc(n, size);

  if(AWASM_UNLIKELY(!ptr)) {
    awasm_set_error(AWASM_ERROR_TYPE_MEMORY, AWASM_ERROR_CODE_NONE,
      NULL, "Allocationg %zux%zu () bytes via calloc failed", n, size, n * size);
    return NULL;
  }
  return ptr;
}

void *
awasm_realloc(void *ptr, size_t size) {
  void *new_ptr = realloc(ptr, size);

  if(AWASM_UNLIKELY(!ptr)) {
    awasm_set_error(AWASM_ERROR_TYPE_MEMORY, AWASM_ERROR_CODE_NONE,
        NULL, "Allocating %zu bytes via realloc failed", size);
    return NULL;
  }
  return new_ptr;
}

void
awasm_free(void *ptr) {
  free(ptr);
}

void *
awasm_mmap(size_t size, void *p) {
  /* Note that mmap considers the pointer passed soley as a hint address
   * and returns a valid address (possibly at a different address) in any case.
   * VirtualAlloc, on the other hand, will return NULL if the address is
   * not available
   */
    void *mem;

#if defined(_WIN32)
retry:
    mem = VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE);
    if(mem == NULL) {
      if(p != NULL) {
        goto retry;
      } else {
        goto error;
      }
    }
    return mem;
#elif defined(_POSIX_VERSION)
    mem = mmap(p, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if(mem == MAP_FAILED) {
      goto error;
    }
#else
#error
#endif
  return mem;

error:
    awasm_set_error(AWASM_ERROR_TYPE_MEMORY, AWASM_ERROR_CODE_NONE,
        NULL, "Allocationg %zu bytes via mmap failed: %s", size, strerror(errno));
    return NULL;
}

awasm_success
awasm_munmap(void *p, size_t size) {
  bool ret;
#if defined(_WIN32)
  ret = VirtualFree(p, size, MEM_DECOMMIT);
#elif defined(_POSIX_VERSION)
  ret = (munmap(p, size) == 0);
#else
#  error
#endif

  if(!ret) {
    awasm_set_error(AWASM_ERROR_TYPE_MEMORY, AWASM_ERROR_CODE_NONE,
        NULL, "Unmapping %zu bytes via munmap failed: %s", size, strerror(errno));
  }

  return ret;
}

awasm_success
awasm_mprot(void *p, size_t size, int mode)
{
  int m = 0;

#if defined(_WIN32)
  if(VirtualProtect(p, size, mode, NULL) != 0) {
    goto error;
  }
#elif defined(_POSIX_VERSION)
  if(mprotect(p, size, mode) != 0) {
    goto error;
  }
#else
#error
#endif
  return true;

error:
  awasm_set_error(AWASM_ERROR_TYPE_MEMORY, AWASM_ERROR_CODE_NONE,
      NULL, "Changing memory protection failed: %s", strerror(errno));
  return false;
}

static long _awasm_page_size = -1;

static long
awasm_query_page_size() {
#if defined(_WIN32)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
#elif defined(_POSIX_VERSION)
  return sysconf(_SC_PAGESIZE);
#else
#error
#endif
}

long
awasm_page_size() {
  if(_awasm_page_size == -1) {
    _awasm_page_size = awasm_query_page_size();
  }
  return _awasm_page_size;
}
