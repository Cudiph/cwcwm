#include <lua.h>

extern void luaC_object_setup(lua_State *L);
extern void luaC_client_setup(lua_State *L);
extern void luaC_container_setup(lua_State *L);
extern void luaC_screen_setup(lua_State *L);
extern void luaC_tag_setup(lua_State *L);
extern void luaC_pointer_setup(lua_State *L);
extern void luaC_plugin_setup(lua_State *L);
extern void luaC_kbd_setup(lua_State *L);
extern void luaC_input_setup(lua_State *L);
extern void luaC_layer_shell_setup(lua_State *L);
extern void luaC_kbindmap_setup(lua_State *L);
extern void luaC_kbind_setup(lua_State *L);
extern void luaC_timer_setup(lua_State *L);
