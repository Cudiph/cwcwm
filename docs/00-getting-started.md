# Getting Started

Getting started with CwC.

## Basic concept

### Tag system

In CwC, every window can have multiple tags. Tags are implemented as a 32 bit field (similar to dwm)
which means there can be at most 30 tags. Since tags are tied to specific screens / monitors, tag operations happen
on the screen objects when using the lua or C API.
In this guide the words *tag* and *workspace* may be used interchangeably.

### Layout system

CwC's layout system is similar to Awesome but in CwC, layouts are grouped into multiple `layout mode`s.
In a single `layout mode` there are multiple `strategy`s.
Currently there are three: `layout mode`s with varying `strategy`s:
- `floating` (Traditional):        Currently there are no strategies of this mode.
- `master` (Awesome layout style): Tiled \[left\], monocle, fullscreen (via flayout plugin).
- `bsp`: (bspwm tiling mode):      Longest side scheme insertion.

The `layout mode` can be changed at runtime by pressing `Super + Space` and
the `strategy` can be changed by pressing `Super + Control + Space`.

### The client

Every window or client (or toplevel in wayland terms) is a surface that can be transformed by the
user. These transformations are things like full screening, maximizing, minimizing, resizing, etc.

### The screen

The screen or output in wayland terms is a device that describes part of the compositor geometry.

### The container

A container is a group of clients which live inside of a single rectangular area. 
Only one client out of the group is displayed at a time. Cycle to the next / previous client in the group by pressing 
`Super + Tab` or `Super + Shift + Tab`.
This can be used to simulate window swallowing or a tabbed layout. Every client operation like resizing
will be applied to every client in the container.

To try it, use `Super + T` to mark currently focused container, then open any application.
The new client will be inserted to the marked container.

### Signals

The compositor may emit a signal when an event occurs. CwC doesn't have per object signals like
Awesome. This makes it simpler to coordinate between lua script and C plugins.
One can subscribe to a signal by using `cwc.connect_signal`, to unsubscribe use `cwc.disconnect_signal`,
and to notify use `cwc.emit_signal`. To mimic per object signals, CwC uses the following format: `object::eventname`
`object` will be passed as the first argument to the callback function. For example when
a client is mapped to the screen, it emits a `client::map` signal.

## Lua configuration

CwC will search for lua files in `$XDG_CONFIG_HOME/cwc/rc.lua` or `~/.config/cwc/rc.lua`,
if both path are not exist or contain an error cwc will load the default lua configuration.

The default configuration is the best way to learn the lua API.
The entry point is located at `defconfig/rc.lua` from the project root directory.
There are code comments that explain how things work.
If the comments are unclear or wrong feel free to open an issue.
Here is some highlight of common things in the lua configuration:

Configuration can be reloaded by calling `cwc.reload`, to check whether the configuration is
triggered by `cwc.reload`, use `cwc.is_startup`.

```lua
-- execute oneshot.lua once, cwc.is_startup() mark that the configuration is loaded for the first time
if cwc.is_startup() then
    require("oneshot")
end
```

To create keybindings, use `cwc.kbd.bind`; for mouse buttons, use `cwc.pointer.bind`.

```lua
local cful = require("cuteful")
local MODKEY = cful.enum.modifier.LOGO

-- mouse
pointer.bind({ MODKEY }, button.LEFT, pointer.move_interactive)

-- keyboard
kbd.bind({ MODKEY }, "j", function()
    local c = cwc.client.focused()
    if c then
        local near = c:get_nearest(direction.DOWN)
        if near then near:focus() end
    end
end, { description = "focus down" })
```

Some of the object have functions for configuration. To set a configuration just call the function
with value you want to set. Here is an example when user want to set mouse sensitivity to very low, keyboard
repeat rate to 30hz, and set the normal client border to dark grey.

```lua
cwc.pointer.set_cursor_size(24)

-- how quickly additional keypress event are send when holding down a key
cwc.kbd.set_repeat_rate(30)

local gears = require("gears")
cwc.client.set_border_color_normal(gears.color("#423e44"))

-- plugin specific settings config, check if such a plugin exist first
if cwc.cwcle then
    cwc.cwcle.set_border_color_raised(gears.color("#d2d6f9"))
end
```

Or better yet using the declarative config like so.
The script below is equivalent to the one above.

```lua
---- ./conf.lua ----
local gears = require("gears")

local conf = {
    cursor_size = 24,
    repeat_rate = 30,
    border_color_normal = gears.color(gears.color("#423e44"))
    border_color_raised = gears.color(gears.color("#d2d6f9"))
}

return conf

---- ./rc.lua ----
local config = require("config")

-- config.init should go first before anything else
config.init(require("conf"))

-- rest of script...
```

Before starting CwC run `cwc --check` to check if there are any errors in your configuration,
so your not staring at a confusing blank screen.
If there is ever a problem and you cant exit CwC with `Super + CTRL + Delete`,
you can switch to a different tty by pressing `CTRL + ALT + FnN` where N is the tty number.

## Default Keybindings

### cwc

- `Super + CTRL + Delete` exit cwc
- `Super + CTRL + r` reload configuration
- `Super + Delete` trigger lua garbage collection
- `Super + Escape` reset leftover server state

### layout

- `Super + ALT + h` decrease master/bsp width factor
- `Super + ALT + l` increase master/bsp width factor
- `Super + CTRL + SHIFT + space` cycle to previous strategy in focused screen
- `Super + CTRL + h` increase the number of columns
- `Super + CTRL + l` decrease the number of columns
- `Super + CTRL + space` cycle to next strategy in focused screen
- `Super + SHIFT + h` increase the number of master clients
- `Super + SHIFT + l` decrease the number of master clients
- `Super + e` toggle bsp split
- `Super + equal` increase gaps
- `Super + minus` decrease gaps
- `Super + space` cycle to next layout mode in focused screen

### screen

- `Super + bracketleft` focus the previous screen
- `Super + bracketright` focus the next screen

### keymap

- `Super + w` activate client vim movement keymap

### client

- `ALT + SHIFT + Tab` cycle previous client
- `ALT + Tab` cycle next client
- `Super + CTRL + 0` toggle client always visible
- `Super + CTRL + Return` promote focused client to master
- `Super + CTRL + j` focus next client relative by index
- `Super + CTRL + k` focus previous client by index
- `Super + CTRL + m` maximize horizontally
- `Super + CTRL + n` restore minimized client
- `Super + CTRL + q` close client forcefully
- `Super + Down` move client downward
- `Super + Left` move client to the left
- `Super + Right` move client to the right
- `Super + SHIFT + Down` increase client height
- `Super + SHIFT + Left` reduce client width
- `Super + SHIFT + Right` increase client width
- `Super + SHIFT + Tab` cycle prev to toplevel inside container
- `Super + SHIFT + Up` reduce client height
- `Super + SHIFT + bracketleft` cycle move focused client to previous screen
- `Super + SHIFT + bracketright` cycle move focused client to next screen
- `Super + SHIFT + equal` increase opacity
- `Super + SHIFT + j` swap with next client by index
- `Super + SHIFT + k` swap with previous client by index
- `Super + SHIFT + m` maximize vertically
- `Super + SHIFT + minus` decrease opacity
- `Super + SHIFT + q` close client respectfully
- `Super + SHIFT + space` toggle floating
- `Super + Tab` cycle next to toplevel inside container
- `Super + Up` move client upward
- `Super + f` toggle fullscreen
- `Super + h` focus left
- `Super + i` toggle client above normal toplevel
- `Super + j` focus down
- `Super + k` focus up
- `Super + l` focus right
- `Super + m` toggle maximize
- `Super + n` minimize client
- `Super + o` toggle client always on top
- `Super + t` mark insert container from the focused client
- `Super + u` toggle client under normal toplevel

### launcher

- `Super + F1` open a web browser
- `Super + Print` screenshot entire screen
- `Super + b` toggle waybar
- `Super + p` application launcher (default is rofi)
- `Super + RETURN` open a terminal
- `Super + v` clipboard history

### tag

- `Super + 0` deactivate all tags on all screen
- `Super + 1` view tag #1
- `Super + 2` view tag #2
- `Super + 3` view tag #3
- `Super + 4` view tag #4
- `Super + 5` view tag #5
- `Super + 6` view tag #6
- `Super + 7` view tag #7
- `Super + 8` view tag #8
- `Super + 9` view tag #9
- `Super + CTRL + 1` toggle tag #1
- `Super + CTRL + 2` toggle tag #2
- `Super + CTRL + 3` toggle tag #3
- `Super + CTRL + 4` toggle tag #4
- `Super + CTRL + 5` toggle tag #5
- `Super + CTRL + 6` toggle tag #6
- `Super + CTRL + 7` toggle tag #7
- `Super + CTRL + 8` toggle tag #8
- `Super + CTRL + 9` toggle tag #9
- `Super + CTRL + SHIFT + 1` toggle focused client on tag #1
- `Super + CTRL + SHIFT + 2` toggle focused client on tag #2
- `Super + CTRL + SHIFT + 3` toggle focused client on tag #3
- `Super + CTRL + SHIFT + 4` toggle focused client on tag #4
- `Super + CTRL + SHIFT + 5` toggle focused client on tag #5
- `Super + CTRL + SHIFT + 6` toggle focused client on tag #6
- `Super + CTRL + SHIFT + 7` toggle focused client on tag #7
- `Super + CTRL + SHIFT + 8` toggle focused client on tag #8
- `Super + CTRL + SHIFT + 9` toggle focused client on tag #9
- `Super + SHIFT + 1` move focused client to tag #1
- `Super + SHIFT + 2` move focused client to tag #2
- `Super + SHIFT + 3` move focused client to tag #3
- `Super + SHIFT + 4` move focused client to tag #4
- `Super + SHIFT + 5` move focused client to tag #5
- `Super + SHIFT + 6` move focused client to tag #6
- `Super + SHIFT + 7` move focused client to tag #7
- `Super + SHIFT + 8` move focused client to tag #8
- `Super + SHIFT + 9` move focused client to tag #9
- `Super + comma` view next workspace/tag
- `Super + grave` activate last activated tags
- `Super + period` view prev workspace/tag
