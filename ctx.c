/*

  Copyright (c) 2016 Tai Chi Minh Ralph Eastwood

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include <stdlib.h>
#include <unistd.h>

#include "ctx.h"

#if defined(DILL_THREADS) && defined(DILL_SHARED)
DILL_THREAD_LOCAL struct dill_ctx dill_context = {0};
#endif

#if defined DILL_THREADS

#include "libdill.h"
#include "utils.h"

/* Needed to determine if current thread is the main thread on Linux. */
#if defined __linux__
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#endif

/* Needed to determine if current thread is the main thread on NetBSD. */
#if defined __NetBSD__
#include <sys/lwp.h>
#endif

/* Needed to determine if current thread is the main thread on other Unix. */
#if defined(__OpenBSD__) || defined(__FreeBSD__) ||\
    defined(__APPLE__) || defined(__DragonFly__) || defined(__sun)
#include <pthread.h>
#endif

static int dill_ismain() {
#if defined __linux__
    return syscall(SYS_gettid) == getpid();
#elif defined(__OpenBSD__) || defined(__FreeBSD__) ||\
    defined(__APPLE__) || defined(__DragonFly__)
    return pthread_main_np();
#elif defined __NetBSD__
    return _lwp_self() == 1;
#elif defined __sun
    return pthread_self() == 1;
#else
    return -1;
#endif
}

#if !(defined(DILL_THREADS) && defined(DILL_SHARED))
static DILL_THREAD_LOCAL struct dill_atexit_fn_list dill_fn_list = {0};
#endif

/* Run registered destructor functions on thread exit. */
static void dill_destructor(void *ptr) {
#if defined(DILL_THREADS) && defined(DILL_SHARED)
    dill_assert(ptr != NULL);
    struct dill_atexit_fn_list *list = &((struct dill_ctx *)ptr)->fn_list;
#else
    struct dill_atexit_fn_list *list = &dill_fn_list;
#endif
    int i = list->count - 1;
    for(; i >= 0; --i) {
        dill_assert(list->fn[i] != NULL);
#if defined(DILL_THREADS) && defined(DILL_SHARED)
        list->fn[i](ptr);
#else
        list->fn[i]();
#endif
    }
}

/* Register a destructor function. */
static int dill_register(void *f) {
#if defined(DILL_THREADS) && defined(DILL_SHARED)
    struct dill_atexit_fn_list *list = &dill_context.fn_list;
#else
    struct dill_atexit_fn_list *list = &dill_fn_list;
#endif
    if(list->count >= DILL_ATEXIT_MAX) {errno = ENOMEM; return -1;}
    list->fn[list->count++] = f;
    return 0;
}

#if defined DILL_PTHREAD
#include <pthread.h>
static pthread_key_t dill_key;
static pthread_once_t dill_keyonce = PTHREAD_ONCE_INIT;
static void dill_makekey(void) {
    int rc = pthread_key_create(&dill_key, dill_destructor);
    dill_assert(!rc);
}
#else
#error "No thread destructor support"
#endif

/* Runs atexit on main thread of simulates atexit on spawned threads. */
int dill_atexit(dill_atexit_fn fn) {
    int rc = dill_ismain();
    if(rc == 1) {
        /* FIXME: right now we assume that if context pointers are not NULL
           then it is either Linux or the main function (on all platforms). */
        return atexit((void (*)(void))fn);
    } else if (rc == 0) {
#if defined DILL_PTHREAD
        rc = pthread_once(&dill_keyonce, dill_makekey);
        if(rc) {errno = rc; return -1;}
#if defined(DILL_PTHREAD) && defined(DILL_SHARED)
        rc = pthread_setspecific(dill_key, &dill_context);
#else
        rc = pthread_setspecific(dill_key, &dill_fn_list);
#endif
        if(rc) {errno = rc; return -1;}
        return dill_register(fn);
#else
#error "No thread destructor support"
#endif
    }
    return -1;
}

#else
int dill_atexit(dill_atexit_fn fn) {
    return atexit(fn);
}
#endif

