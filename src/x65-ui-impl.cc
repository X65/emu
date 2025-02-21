/*
    UI implementation for x65.c, this must live in a .cc file.
*/
#include "chips/chips_common.h"
#include "chips/w65c816s.h"
#include "chips/m6526.h"
#include "chips/kbd.h"
#include "chips/clk.h"
#include "systems/x65.h"
#define UI_DBG_USE_W65C816S
#define UI_DASM_USE_W65C816S
#define CHIPS_UTIL_IMPL
#include "util/w65c816sdasm.h"
#define CHIPS_UI_IMPL
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/ui_util.h"
#include "ui/ui_settings.h"
#include "ui/ui_chip.h"
#include "ui/ui_memedit.h"
#include "ui/ui_dasm.h"
#include "ui/ui_dbg.h"
#include "ui/ui_w65c816s.h"
#include "ui/ui_audio.h"
#include "ui/ui_display.h"
#include "ui/ui_kbd.h"
#include "ui/ui_snapshot.h"
#include "ui/ui_x65.h"
