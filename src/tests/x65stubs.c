// Host-side globals that src/systems/x65.c and the chips it wires up reference
// but that frame-publication tests have no use for. Kept in a C translation
// unit so the declarations they must match can simply be included.
#include "args.h"
#include "hid.h"
#include "firmware/src/south/term/term.h"

// zeromem skips x65_init()'s 16M-iteration rand() fill of RAM, which costs more
// than every case in the suite put together and leaves the machines differing
// in memory the tests never read.
struct arguments arguments = { .zeromem = true };

void hid_init(void) {}
void hid_shutdown(void) {}
void hid_reset(void) {}
void hid_key_down(sapp_keycode) {}
void hid_key_up(sapp_keycode) {}

// pulled in by term/font.c, which x65.c includes for the RIA font API
void term_RIS(void) {}

void log_func(uint32_t, const char*, const char*, uint32_t, const char*, ...) {}
