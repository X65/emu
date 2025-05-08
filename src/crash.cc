#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <libunwind.h>
#include <cxxabi.h>
#include <unistd.h>

extern "C" void segfault_handler(int sig) {
    fprintf(stderr, "Caught signal %d (%s)\n", sig, strsignal(sig));

    unw_cursor_t cursor;
    unw_context_t context;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    int frame = 0;
    while (unw_step(&cursor) > 0) {
        char symbol[256];
        unw_word_t offset, pc;

        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        symbol[0] = '\0';
        unw_get_proc_name(&cursor, symbol, sizeof(symbol), &offset);

        // Try to demangle the symbol
        int status;
        char* demangled = abi::__cxa_demangle(symbol, NULL, NULL, &status);

        fprintf(
            stderr,
            "#%d  %p : %s+0x%lx\n",
            frame++,
            (void*)pc,
            (status == 0 && demangled) ? demangled : symbol,
            (long)offset);

        free(demangled);
    }

    _exit(EXIT_FAILURE);
}
