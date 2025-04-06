#ifndef _CWC_LUACLASS_H
#define _CWC_LUACLASS_H

#include <lauxlib.h>
#include <lua.h>

#include "cwc/luaobject.h"

void luaC_register_class(lua_State *L,
                         const char *classname,
                         luaL_Reg methods[],
                         luaL_Reg metamethods[]);

//==================== MACRO =====================

/* using macro is either don't repeat yourself or read and repeat because large
 * macro is unreadable. Here is the real form after the preprocessing:
 */

// extern const char *const client_classname;

// int luaC_client_create(lua_State *L, struct cwc_toplevel *toplevel)
// {
//     struct cwc_toplevel **c_udat = lua_newuserdata(L, sizeof(&toplevel));
//     *c_udat                      = toplevel;
//
//     luaL_getmetatable(L, client_classname);
//     lua_setmetatable(L, -2);
//
//     return 1;
// }

// struct cwc_toplevel *luaC_client_checkudata(lua_State *L, int ud)
// {
//     struct cwc_toplevel **c_udat = luaL_checkudata(L, ud, client_classname);
//     return *c_udat;
// }

// static void luaC_object_client_register(lua_State *L,
//                                         struct cwc_toplevel *pointer)
// {
//     lua_settop(L, 0);
//     luaC_client_create(L, pointer);
//     luaC_object_register(L, -1, pointer);
// }

// static int luaC_client_eq(lua_State *L)
// {
//     struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);
//     struct cwc_toplevel *toplevel2 = luaC_client_checkudata(L, 2);
//
//     lua_pushboolean(L, toplevel == toplevel2);
//     return 1;
// }

// static int luaC_client_tostring(lua_State *L)
// {
//     struct cwc_toplevel *toplevel = luaC_client_checkudata(L, 1);
//
//     lua_pushfstring(L, "cwc_client: %p", toplevel);
//     return 1;
// }

/* this macro will setup common interface for lua class */
#define LUAC_CLASS_CREATE(cstruct_name, obj_classname)                        \
    extern const char *const obj_classname##_classname;                       \
                                                                              \
    static inline struct cstruct_name *luaC_##obj_classname##_checkudata(     \
        lua_State *L, int ud)                                                 \
    {                                                                         \
        struct cstruct_name **udata =                                         \
            luaL_checkudata(L, ud, obj_classname##_classname);                \
        return *udata;                                                        \
    }                                                                         \
                                                                              \
    static inline int luaC_##obj_classname##_create(                          \
        lua_State *L, struct cstruct_name *cstruct)                           \
    {                                                                         \
        struct cstruct_name **udata = lua_newuserdata(L, sizeof(&cstruct));   \
        *udata                      = cstruct;                                \
        luaL_getmetatable(L, obj_classname##_classname);                      \
        lua_setmetatable(L, -2);                                              \
        return 1;                                                             \
    }                                                                         \
                                                                              \
    static inline void luaC_object_##obj_classname##_register(                \
        lua_State *L, struct cstruct_name *pointer)                           \
    {                                                                         \
        lua_settop(L, 0);                                                     \
        luaC_##obj_classname##_create(L, pointer);                            \
        luaC_object_register(L, -1, pointer);                                 \
    }                                                                         \
                                                                              \
    static inline int luaC_##obj_classname##_eq(lua_State *L)                 \
    {                                                                         \
        struct cstruct_name *udata = luaC_##obj_classname##_checkudata(L, 1); \
        struct cstruct_name *udat2 = luaC_##obj_classname##_checkudata(L, 2); \
        lua_pushboolean(L, udata == udat2);                                   \
        return 1;                                                             \
    }                                                                         \
                                                                              \
    static inline int luaC_##obj_classname##_tostring(lua_State *L)           \
    {                                                                         \
        struct cstruct_name *toplevel =                                       \
            luaC_##obj_classname##_checkudata(L, 1);                          \
        lua_pushfstring(L, "cwc_" #obj_classname ": %p", toplevel);           \
        return 1;                                                             \
    }

LUAC_CLASS_CREATE(cwc_toplevel, client)
LUAC_CLASS_CREATE(cwc_container, container)
LUAC_CLASS_CREATE(cwc_output, screen)
LUAC_CLASS_CREATE(cwc_tag_info, tag)
LUAC_CLASS_CREATE(cwc_libinput_device, input)
LUAC_CLASS_CREATE(cwc_layer_surface, layer_shell)

#endif // !_CWC_LUACLASS_H
