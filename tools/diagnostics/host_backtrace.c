/* Linux debugging observer only. Stripped binaries provide module offsets.
 * The SIGSEGV handler uses non-async-signal-safe diagnostics; never deploy it.
 * Prefer gdb when ptrace is available. This observer is not an Eden fix. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
__attribute__((noreturn)) void __cxa_throw(void *object, void *type, void (*destroy)(void *)) {
    const char *name = type ? ((const char **)type)[1] : "unknown";
    dprintf(2,"FHNX_HOST_THROW type=%s\n",name);
    void *frames[48]; int count=backtrace(frames,48);
    for(int i=0;i<count;i++) {
        Dl_info info={0}; dladdr(frames[i],&info);
        dprintf(2,"FHNX_HOST_FRAME %d %s +0x%lx %s\n",i,info.dli_fname?info.dli_fname:"?",(unsigned long)((char*)frames[i]-(char*)info.dli_fbase),info.dli_sname?info.dli_sname:"?");
    }
    void (*next)(void*,void*,void(*)(void*)) = dlsym(RTLD_NEXT,"__cxa_throw");
    if (!next) _exit(125);
    next(object,type,destroy); __builtin_unreachable();
}
#include <signal.h>
static void crash_handler(int sig) {
    dprintf(2,"FHNX_HOST_SIGNAL %d\n",sig);
    void *frames[48];int count=backtrace(frames,48);
    for(int i=0;i<count;i++) { Dl_info info={0}; dladdr(frames[i],&info); dprintf(2,"FHNX_HOST_FRAME %d %s +0x%lx %s\n",i,info.dli_fname?info.dli_fname:"?",(unsigned long)((char*)frames[i]-(char*)info.dli_fbase),info.dli_sname?info.dli_sname:"?"); }
    _exit(128+sig);
}
__attribute__((constructor)) static void install_handler(void) { signal(SIGSEGV,crash_handler); }
