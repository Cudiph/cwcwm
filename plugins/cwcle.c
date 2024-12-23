/** Windows like Alt+Tab useful when in floating mode.
 *
 * @author Dwi Asmoro Bangun
 * @copyright 2024
 * @license MIT
 * @pluginlib cwc.cwcle
 */

#include <cairo.h>
#include <lauxlib.h>
#include <lua.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_keyboard.h>

#include "cwc/config.h"
#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/input/keyboard.h"
#include "cwc/input/seat.h"
#include "cwc/layout/container.h"
#include "cwc/luac.h"
#include "cwc/plugin.h"
#include "cwc/server.h"
#include "cwc/signal.h"

static cairo_pattern_t *raised_pattern = NULL;

static struct lifecwcle {
    struct cwc_container *raised;
    bool on_cycle;

    struct wlr_keyboard *kbd;
    uint32_t modifier;

    struct wl_listener on_modifier_l;
} lifecwcle = {0};

static void stop_cycle()
{
    lifecwcle.on_cycle = false;
    if (lifecwcle.raised)
        cwc_toplevel_focus(cwc_container_get_front_toplevel(lifecwcle.raised),
                           true);
    lifecwcle.raised = NULL;
}

static void raise_container(struct cwc_container *container)
{
    cwc_border_set_pattern(&container->border, raised_pattern);
    wlr_scene_node_raise_to_top(&container->tree->node);
    lifecwcle.raised = container;
}

static void raise_next_container(struct cwc_container *current)
{
    struct cwc_container *container;
    struct wl_list *sentinel_head = &current->output->state->focus_stack;

    wl_list_for_each(container, &current->link_output_fstack,
                     link_output_fstack)
    {
        if (&container->link_output_fstack == sentinel_head)
            continue;

        if (cwc_container_is_visible(container)) {
            cwc_border_set_pattern(&current->border,
                                   g_config.border_color_normal);
            raise_container(container);
            break;
        }
    }
}

static void raise_prev_container(struct cwc_container *current)
{
    struct cwc_container *container;
    struct wl_list *sentinel_head = &current->output->state->focus_stack;

    wl_list_for_each_reverse(container, &current->link_output_fstack,
                             link_output_fstack)
    {
        if (&container->link_output_fstack == sentinel_head)
            continue;

        if (cwc_container_is_visible(container)) {
            cwc_border_set_pattern(&current->border,
                                   g_config.border_color_normal);
            raise_container(container);
            break;
        }
    }
}

static void enter_cycle(bool is_next)
{
    struct cwc_container *current;
    struct cwc_output *output;

    if (lifecwcle.on_cycle)
        goto cycle;

    // init cycle
    output = server.focused_output;

    lifecwcle.raised = current = wl_container_of(
        output->state->focus_stack.next, current, link_output_fstack);

    struct cwc_container **visibles = cwc_output_get_visible_containers(output);
    int visible_count               = 0;
    struct cwc_container **vis_ptr  = visibles;
    while (*(vis_ptr++))
        visible_count++;
    free(visibles);

    if (visible_count < 2)
        return;

    lifecwcle.on_cycle = true;
    cwc_toplevel_focus(NULL, false);

cycle:
    if (is_next)
        raise_next_container(lifecwcle.raised);
    else
        raise_prev_container(lifecwcle.raised);
}

/** Cycle to the next toplevel.
 *
 * @staticfct next
 * @tparam integer enum A single modifier to mark end cycle when it released
 * @noreturn
 * @see cuteful.enum
 */
static int luaC_enter_cycle_next(lua_State *L)
{
    uint32_t mod       = luaL_checkint(L, 1);
    lifecwcle.modifier = mod;
    enter_cycle(true);
    return 0;
}

/** Cycle to the previous toplevel.
 *
 * @staticfct prev
 * @tparam integer enum A single modifier to mark end cycle when it released
 * @noreturn
 * @see cuteful.enum
 */
static int luaC_enter_cycle_prev(lua_State *L)
{
    uint32_t mod       = luaL_checkint(L, 1);
    lifecwcle.modifier = mod;
    enter_cycle(false);
    return 0;
}

/** Set the border of toplevel when raised.
 *
 * @tparam cairo_pattern_t color Color from gears.color
 * @configfct set_border_color_raised
 * @noreturn
 * @see gears.color
 */
static int luaC_set_border_color_raised(lua_State *L)
{
    cairo_pattern_t *pattern = luaC_checkcolor(L, 1);
    cairo_pattern_destroy(raised_pattern);
    raised_pattern = cairo_pattern_reference(pattern);

    return 0;
}

static void on_kbd_mod(struct wl_listener *listener, void *data)
{
    uint32_t mod = wlr_keyboard_get_modifiers(lifecwcle.kbd);

    if (lifecwcle.on_cycle && (mod & lifecwcle.modifier) == 0)
        stop_cycle();
}

void setup_cwcle(void *data)
{
    struct wlr_keyboard *kbd = lifecwcle.kbd =
        wlr_seat_get_keyboard(server.seat->wlr_seat);

    lifecwcle.on_modifier_l.notify = on_kbd_mod;
    wl_signal_add(&kbd->events.modifiers, &lifecwcle.on_modifier_l);
}

static const luaL_Reg cwcle_staticlibs[] = {
    {"next",                    luaC_enter_cycle_next       },
    {"prev",                    luaC_enter_cycle_prev       },
    {"set_border_color_raised", luaC_set_border_color_raised},
    {NULL,                      NULL                        },
};

static void register_lualibs(void *data)
{
    lua_State *L = g_config_get_lua_State();
    lua_getglobal(L, "cwc");

    lua_newtable(L);
    luaL_register(L, NULL, cwcle_staticlibs);
    lua_setfield(L, -2, "cwcle");

    lua_pop(L, 1);
}

static void handle_destroyed_raised_client(void *data)
{
    struct cwc_container *current = data;

    if (!lifecwcle.on_cycle || current != lifecwcle.raised)
        return;

    struct wl_list *sentinel_head = &current->output->state->focus_stack;
    struct cwc_container *container;
    wl_list_for_each(container, &current->output->state->focus_stack,
                     link_output_fstack)
    {
        if (&container->link_output_fstack == sentinel_head
            || container == current)
            continue;

        if (cwc_container_is_visible(container)) {
            raise_container(container);
            return;
        }
    }

    lifecwcle.raised = NULL;
    stop_cycle();
}

static int cwcle_init()
{
    // default to purple
    raised_pattern =
        cairo_pattern_create_rgba(0xff / 255.0, 0xaa / 255.0, 0xff / 255.0, 1);

    /* since the module loaded before the server initialized which mean the
     * seat hasn't been created yet, we register the keyboard modifier signal
     * when the event loop run. */
    wl_event_loop_add_idle(server.wl_event_loop, setup_cwcle, NULL);

    cwc_signal_connect("lua::reload", register_lualibs);
    cwc_signal_connect("container::destroy", handle_destroyed_raised_client);

    /* cwc.cwcle */
    register_lualibs(NULL);

    return 0;
}

// this plugin doesn't support unloading so there is no plugin_exit
plugin_init(cwcle_init);

PLUGIN_NAME("cwcle");
PLUGIN_VERSION("0.1.0");
PLUGIN_DESCRIPTION("windows like alt+tab");
PLUGIN_LICENSE("MIT");
PLUGIN_AUTHOR("Dwi Asmoro Bangun <dwiaceromo@gmail.com>");
