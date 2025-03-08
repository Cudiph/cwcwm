#ifndef _CWC_SIGNAL_H
#define _CWC_SIGNAL_H

#include <lua.h>
#include <wayland-util.h>

#include "cwc/luaobject.h"

typedef void (*signal_callback_t)(void *data);

struct signal_c_callback {
    struct wl_list link;
    signal_callback_t callback;
};

struct signal_lua_callback {
    struct wl_list link; // struct cwc_signal_entry.lua_callback
    int luaref;
};

struct cwc_signal_entry {
    struct wl_list c_callbacks;   // struct signal_c_callback.link
    struct wl_list lua_callbacks; // struct signal_lua_callback.link
};

/* Register a listener for C function */
void cwc_signal_connect(const char *name, signal_callback_t callback);

/* Register a listener for a lua function at idx index */
void cwc_signal_connect_lua(const char *name, lua_State *L, int idx);

/* Unregister a listener */
void cwc_signal_disconnect(const char *name, signal_callback_t callback);

/* Unregister a listener for lua */
void cwc_signal_disconnect_lua(const char *name, lua_State *L, int idx);

/* Notify signal for C listener only */
void cwc_signal_emit_c(const char *name, void *data);

/* notify for lua listener only */
void cwc_signal_emit_lua(const char *name, lua_State *L, int nargs);

/** Notify listener for both C and lua side
 *
 * \param name Signal name
 * \param data Data to pass for C callback
 * \param L Lua VM state
 * \param n Length of element to pass to the lua callback
 */
void cwc_signal_emit(const char *name, void *data, lua_State *L, int nargs);

/* only for reloading lua config */
struct cwc_hhmap;
void cwc_lua_signal_clear(struct cwc_hhmap *map);

//====================== MACRO ===================

/* emit signal name with an object pointed as pointer as the only argument
 * passed
 */
int cwc_object_emit_signal_simple(const char *name,
                                  lua_State *L,
                                  void *pointer);

/* the C listener data arg is a list of what is passed as by the varargs ... */
void cwc_object_emit_signal_varr(const char *name,
                                 lua_State *L,
                                 int nargs,
                                 ...);

#endif // !_CWC_SIGNAL_H
