// Keeping the desktop awake while Emu owns the whole screen.
//
// sokol has no idle-inhibition hook outside its Win32 backend, and what it does
// there -- swallowing SC_SCREENSAVE/SC_MONITORPOWER -- is the Win9x-era trick
// that a secure screen saver on Windows 10/11 ignores. So each platform we ship
// gets its own answer here, behind one API the frame loop can call blindly.
#include "screensaver.h"

#include "log.h"

static bool inhibited = false;

static void plat_inhibit(void);
static void plat_release(void);
static void plat_shutdown(void);

void screensaver_inhibit(bool inhibit) {
    if (inhibit == inhibited) {
        return;
    }
    // Record the intent even if the backend can't honour it, so a desktop that
    // has no one to answer isn't asked again on every frame.
    inhibited = inhibit;
    if (inhibit) {
        plat_inhibit();
    }
    else {
        plat_release();
    }
}

void screensaver_shutdown(void) {
    if (inhibited) {
        inhibited = false;
        plat_release();
    }
    plat_shutdown();
}

#if defined(__EMSCRIPTEN__)
// -- web: Screen Wake Lock ---------------------------------------------------

    #include <emscripten/emscripten.h>

// The browser drops the wake lock whenever the tab goes hidden, so it has to be
// re-taken on visibilitychange for as long as we still want it. The state lives
// on Module because the sentinel outlives any single call.
// This body is JavaScript, which clang-format would mangle into C.
// clang-format off
EM_JS(void, emu_js_wake_lock, (int want), {
    if (!Module.emuWakeLock) {
        Module.emuWakeLock = { want: false, sentinel: null, hooked: false };
    }
    const st = Module.emuWakeLock;
    st.want = !!want;

    // absent outside a secure context, and on browsers without the API
    if (!navigator.wakeLock) {
        return;
    }

    const acquire = () => {
        if (!st.want || st.sentinel || document.hidden) {
            return;
        }
        navigator.wakeLock.request('screen').then((s) => {
            if (!st.want) {
                // fullscreen was left while the request was still in flight
                s.release().catch(() => {});
                return;
            }
            st.sentinel = s;
            s.addEventListener('release', () => { st.sentinel = null; });
        }).catch(() => {});
    };

    if (st.want) {
        if (!st.hooked) {
            st.hooked = true;
            document.addEventListener('visibilitychange', () => {
                if (!document.hidden) {
                    acquire();
                }
            });
        }
        acquire();
    }
    else if (st.sentinel) {
        const s = st.sentinel;
        st.sentinel = null;
        s.release().catch(() => {});
    }
});
// clang-format on

static void plat_inhibit(void) {
    emu_js_wake_lock(1);
}

static void plat_release(void) {
    emu_js_wake_lock(0);
}

static void plat_shutdown(void) {}

#elif defined(_WIN32)
// -- windows: SetThreadExecutionState ----------------------------------------

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

// ES_CONTINUOUS is what makes this a standing request rather than a one-shot
// nudge of the idle timer. The state belongs to the calling thread, and both
// calls come from the sokol main thread.
static void plat_inhibit(void) {
    if (SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED) == 0) {
        LOG_WARNING("SetThreadExecutionState failed; screen-saver not inhibited");
    }
}

static void plat_release(void) {
    SetThreadExecutionState(ES_CONTINUOUS);
}

static void plat_shutdown(void) {}

#elif defined(USE_DBUS_SCREENSAVER)
// -- linux: org.freedesktop.ScreenSaver --------------------------------------
//
// The only mechanism the desktops actually honour: GNOME, KDE, XFCE and friends
// all own this name, and it works the same under Wayland, where the X screen
// saver API is a no-op. libdbus is dlopen'd rather than linked so a machine
// without it still runs -- we just don't get to inhibit anything.

    #include <dbus/dbus.h>
    #include <dlfcn.h>

    #define SS_NAME       "org.freedesktop.ScreenSaver"
    #define SS_PATH       "/org/freedesktop/ScreenSaver"
    #define SS_APP        "X65 Emu"
    #define SS_REASON     "Fullscreen emulation"
    #define SS_TIMEOUT_MS (1000)

static struct {
    void* lib;
    bool loaded;  // dlopen + dlsym attempted, successfully or not
    bool usable;
    bool conn_warned;
    bool call_warned;

    DBusConnection* conn;
    dbus_uint32_t cookie;
    bool has_cookie;

    void (*error_init)(DBusError*);
    dbus_bool_t (*error_is_set)(const DBusError*);
    void (*error_free)(DBusError*);
    DBusConnection* (*bus_get)(DBusBusType, DBusError*);
    void (*conn_set_exit_on_disconnect)(DBusConnection*, dbus_bool_t);
    void (*conn_unref)(DBusConnection*);
    DBusMessage* (*conn_send_block)(DBusConnection*, DBusMessage*, int, DBusError*);
    DBusMessage* (*msg_new_method_call)(const char*, const char*, const char*, const char*);
    void (*msg_unref)(DBusMessage*);
    void (*iter_init_append)(DBusMessage*, DBusMessageIter*);
    dbus_bool_t (*iter_append_basic)(DBusMessageIter*, int, const void*);
    dbus_bool_t (*iter_init)(DBusMessage*, DBusMessageIter*);
    int (*iter_get_arg_type)(DBusMessageIter*);
    void (*iter_get_basic)(DBusMessageIter*, void*);
} dbus;

    #define LOAD_SYM(field, name)                                                               \
        do {                                                                                    \
            *(void**)(&dbus.field) = dlsym(dbus.lib, name);                                     \
            if (!dbus.field) {                                                                  \
                LOG_WARNING("libdbus-1: %s missing; screen-saver will not be inhibited", name); \
                return false;                                                                   \
            }                                                                                   \
        } while (0)

static bool dbus_load(void) {
    if (dbus.loaded) {
        return dbus.usable;
    }
    dbus.loaded = true;

    dbus.lib = dlopen("libdbus-1.so.3", RTLD_LAZY | RTLD_LOCAL);
    if (!dbus.lib) {
        LOG_WARNING("libdbus-1.so.3 not available (%s); screen-saver will not be inhibited", dlerror());
        return false;
    }

    LOAD_SYM(error_init, "dbus_error_init");
    LOAD_SYM(error_is_set, "dbus_error_is_set");
    LOAD_SYM(error_free, "dbus_error_free");
    LOAD_SYM(bus_get, "dbus_bus_get");
    LOAD_SYM(conn_set_exit_on_disconnect, "dbus_connection_set_exit_on_disconnect");
    LOAD_SYM(conn_unref, "dbus_connection_unref");
    LOAD_SYM(conn_send_block, "dbus_connection_send_with_reply_and_block");
    LOAD_SYM(msg_new_method_call, "dbus_message_new_method_call");
    LOAD_SYM(msg_unref, "dbus_message_unref");
    LOAD_SYM(iter_init_append, "dbus_message_iter_init_append");
    LOAD_SYM(iter_append_basic, "dbus_message_iter_append_basic");
    LOAD_SYM(iter_init, "dbus_message_iter_init");
    LOAD_SYM(iter_get_arg_type, "dbus_message_iter_get_arg_type");
    LOAD_SYM(iter_get_basic, "dbus_message_iter_get_basic");

    dbus.usable = true;
    return true;
}

    #undef LOAD_SYM

static DBusConnection* dbus_session(void) {
    if (dbus.conn) {
        return dbus.conn;
    }

    DBusError err;
    dbus.error_init(&err);
    DBusConnection* conn = dbus.bus_get(DBUS_BUS_SESSION, &err);
    if (!conn) {
        if (!dbus.conn_warned) {
            dbus.conn_warned = true;
            LOG_WARNING(
                "no session bus (%s); screen-saver will not be inhibited",
                dbus.error_is_set(&err) ? err.message : "unknown error");
        }
        dbus.error_free(&err);
        return NULL;
    }
    dbus.error_free(&err);

    // dbus_bus_get() defaults this on, which would _exit() the emulator out
    // from under the user if the session bus ever restarts
    dbus.conn_set_exit_on_disconnect(conn, false);
    dbus.conn = conn;
    return conn;
}

static void plat_inhibit(void) {
    if (dbus.has_cookie || !dbus_load()) {
        return;
    }
    DBusConnection* conn = dbus_session();
    if (!conn) {
        return;
    }

    DBusMessage* msg = dbus.msg_new_method_call(SS_NAME, SS_PATH, SS_NAME, "Inhibit");
    if (!msg) {
        return;
    }

    const char* app = SS_APP;
    const char* reason = SS_REASON;
    DBusMessageIter args;
    dbus.iter_init_append(msg, &args);
    if (!dbus.iter_append_basic(&args, DBUS_TYPE_STRING, &app)
        || !dbus.iter_append_basic(&args, DBUS_TYPE_STRING, &reason)) {
        dbus.msg_unref(msg);
        return;
    }

    DBusError err;
    dbus.error_init(&err);
    DBusMessage* reply = dbus.conn_send_block(conn, msg, SS_TIMEOUT_MS, &err);
    dbus.msg_unref(msg);
    if (!reply) {
        if (!dbus.call_warned) {
            dbus.call_warned = true;
            LOG_WARNING(SS_NAME ".Inhibit failed: %s", dbus.error_is_set(&err) ? err.message : "no reply");
        }
        dbus.error_free(&err);
        return;
    }
    dbus.error_free(&err);

    DBusMessageIter ret;
    if (dbus.iter_init(reply, &ret) && dbus.iter_get_arg_type(&ret) == DBUS_TYPE_UINT32) {
        dbus.iter_get_basic(&ret, &dbus.cookie);
        dbus.has_cookie = true;
    }
    else if (!dbus.call_warned) {
        dbus.call_warned = true;
        LOG_WARNING(SS_NAME ".Inhibit returned no cookie");
    }
    dbus.msg_unref(reply);
}

static void plat_release(void) {
    if (!dbus.has_cookie || !dbus.conn) {
        return;
    }
    dbus.has_cookie = false;

    DBusMessage* msg = dbus.msg_new_method_call(SS_NAME, SS_PATH, SS_NAME, "UnInhibit");
    if (!msg) {
        return;
    }

    DBusMessageIter args;
    dbus.iter_init_append(msg, &args);
    if (dbus.iter_append_basic(&args, DBUS_TYPE_UINT32, &dbus.cookie)) {
        DBusError err;
        dbus.error_init(&err);
        DBusMessage* reply = dbus.conn_send_block(dbus.conn, msg, SS_TIMEOUT_MS, &err);
        if (reply) {
            dbus.msg_unref(reply);
        }
        else {
            LOG_WARNING(SS_NAME ".UnInhibit failed: %s", dbus.error_is_set(&err) ? err.message : "no reply");
        }
        dbus.error_free(&err);
    }
    dbus.msg_unref(msg);
}

static void plat_shutdown(void) {
    if (dbus.conn) {
        // dbus_bus_get() hands out a shared connection: unref it, never close it
        dbus.conn_unref(dbus.conn);
        dbus.conn = NULL;
    }
    // the dlopen handle outlives us on purpose -- shutdown is process exit, and
    // unloading libdbus buys nothing at that point
}

#else
// -- no backend for this platform --------------------------------------------

static void plat_inhibit(void) {}
static void plat_release(void) {}
static void plat_shutdown(void) {}

#endif
