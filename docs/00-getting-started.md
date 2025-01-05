# Getting Started

Getting started with cwc.

## Basic concept

### Tag system

CwC is using dwm tag model rather than like in Awesome where the tag is a 32 bit bitfield
which mean it's limited to 30 tags but it should be enough for most people I think. The tags
is integrated to the screen so the tags is independent across screens and since it's integrated,
all the tag operation is on the screen object when using lua or C api. The tag and workspace in this
guide may be used interchangeably.

### Layout system

The layout system is similar like in Awesome with the difference in cwc is that it's grouped.
The grouping is called `layout mode` with the layout options is called `strategy`,
there are 3 modes which is `floating`, `master/stack`, and `bsp`.
This allow cwc to act like traditional floating/stacking window manager,
Awesome layout style, and bspwm tiling mode.

Strategy:

- `Floating` - Currently there are none.
- `Master/stack` - Tiled \[left\], monocle, fullscreen (via flayout plugin).
- `BSP` - Longest side scheme insertion.

The `layout mode` can be changed at runtime by pressing `Super + Space` and
to change the `strategy` is by pressing `Super + Control + Space`.

### The client

The client or window or toplevel in wayland terms is a surface that can be transformed by the
user such as fullscreen, maximizing, minimizing, resizing, etc.

### The screen

The screen or output in wayland terms is a device that describes part of the compositor geometry.

### The container

The container basically a group of client overlapping each other inside a rectangular area.
It's can be used to simulate window swallowing or tabbed layout. Every client operation like resizing
will be reflected to every client in the container.

To try it, use `Super + T` to mark current focused container then open any application it
will be inserted to the marked container. To switch view between clients in the container
press `Super + Tab` and `Super + Shift + Tab`, this concept is taken from tabbing in browser.

### Signals

The compositor may emit a signal when an event occurs. CwC doesn't have per object signal like in
Awesome because it's simpler to manage between lua script and C plugins so every signal is need
to be subscribed by using `cwc.connect_signal`, to unsubscribe use `cwc.disconnect_signal`,
and to notify use `cwc.emit_signal`. To mimic per object signal cwc use format like `object::eventname`
where object will be passed as first argument in the callback function. For example when
a client is mapped to the screen, it emits `client::map` signal.

## Lua configuration

CwC will search lua configuration in `$XDG_CONFIG_HOME/cwc/rc.lua` or `~/.config/cwc/rc.lua`,
if both path are not exist or contain an error cwc will load the default lua configuration.

The default configuration is the best way to learn the lua API.
The entry point located at `defconfig/rc.lua` from the project root directory,
there's explanation how things working.
If the comments still unclear or wrong feel free to open an issue.
Here is some highlight of common things in the lua configuration.

Configuration can be reloaded by calling `cwc.reload`, to check whether the configuration is
triggered by `cwc.reload`, use `cwc.is_startup`.

```lua
-- execute oneshot.lua once, cwc.is_startup() mark that the configuration is loaded for the first time
if cwc.is_startup() then
    require("oneshot")
end
```

To create a keybinding, use `cwc.pointer.bind` for mouse buttons and use `cwc.kbd.bind` for keyboard
buttons.

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

Some of the object has function for configuration, to set a configuration just call the function
with value you want to set. Here is an example when user want to set mouse sensitivity to very low with keyboard
repeat rate to 30hz and set the normal client border to dark grey.

```lua
cwc.pointer.set_cursor_size(24)

cwc.kbd.set_repeat_rate(30)

local gears = require("gears")
cwc.client.set_border_color_normal(gears.color("#423e44"))

-- plugin specific settings config, check if such a plugin exist first
if cwc.cwcle then
    cwc.cwcle.set_border_color_raised(gears.color("#d2d6f9"))
end
```

## Default Keybinding

### cwc

- `Super + Control + Delete` exit cwc
- `Super + Control + r` reload configuration
- `Super + Delete` trigger lua garbage collection
- `Super + Escape` reset leftover server state

### client

- `Alt + Shift + Tab` cycle previous client
- `Alt + Tab` cycle next client
- `Super + Control + 0` toggle client always visible
- `Super + Control + j` focus next client relative by index
- `Super + Control + k` focus previous client by index
- `Super + Control + m` maximize horizontally
- `Super + Control + n` restore minimized client
- `Super + Control + q` close client forcefully
- `Super + Control + Return` promote focused client to master
- `Super + Down` move client downward
- `Super + f` toggle fullscreen
- `Super + h` focus left
- `Super + i` toggle client above normal toplevel
- `Super + j` focus down
- `Super + k` focus up
- `Super + Left` move client to the left
- `Super + l` focus right
- `Super + m` toggle maximize
- `Super + n` minimize client
- `Super + o` toggle client always on top
- `Super + Right` move client to the right
- `Super + Shift + Down` increase client height
- `Super + Shift + j` swap with next client by index
- `Super + Shift + k` swap with previous client by index
- `Super + Shift + Left` reduce client width
- `Super + Shift + m` maximize vertically
- `Super + Shift + q` close client respectfully
- `Super + Shift + Right` increase client width
- `Super + Shift + space` toggle floating
- `Super + Shift + Tab` cycle prev to toplevel inside container
- `Super + Shift + Up` reduce client height
- `Super + Tab` cycle next to toplevel inside container
- `Super + t` mark insert container from the focused client
- `Super + Up` move client upward
- `Super + u` toggle client under normal toplevel

### launcher

- `Super + F1` open a web browser
- `Super + p` application launcher
- `Super + Print` screenshot entire screen
- `Super + Return` open a terminal
- `Super + Shift + s` snipping tool
- `Super + v` clipboard history

### layout

- `Super + Control + Shift + space` cycle to previous strategy in focused screen
- `Super + Control + space` cycle to next strategy in focused screen
- `Super + equal` increase gaps
- `Super + e` toggle bsp split
- `Super + minus` decrease gaps
- `Super + space` cycle to next layout mode in focused screen

### tag

- `Super + 1` view tag #1
- `Super + 2` view tag #2
- `Super + 3` view tag #3
- `Super + 4` view tag #4
- `Super + 5` view tag #5
- `Super + 6` view tag #6
- `Super + 7` view tag #7
- `Super + 8` view tag #8
- `Super + 9` view tag #9
- `Super + comma` view next workspace/tag
- `Super + period` view prev workspace/tag
- `Super + Control + 1` toggle tag #1
- `Super + Control + 2` toggle tag #2
- `Super + Control + 3` toggle tag #3
- `Super + Control + 4` toggle tag #4
- `Super + Control + 5` toggle tag #5
- `Super + Control + 6` toggle tag #6
- `Super + Control + 7` toggle tag #7
- `Super + Control + 8` toggle tag #8
- `Super + Control + 9` toggle tag #9
- `Super + Control + Shift + 1` toggle focused client on tag #1
- `Super + Control + Shift + 2` toggle focused client on tag #2
- `Super + Control + Shift + 3` toggle focused client on tag #3
- `Super + Control + Shift + 4` toggle focused client on tag #4
- `Super + Control + Shift + 5` toggle focused client on tag #5
- `Super + Control + Shift + 6` toggle focused client on tag #6
- `Super + Control + Shift + 7` toggle focused client on tag #7
- `Super + Control + Shift + 8` toggle focused client on tag #8
- `Super + Control + Shift + 9` toggle focused client on tag #9
- `Super + Shift + 1` move focused client to tag #1
- `Super + Shift + 2` move focused client to tag #2
- `Super + Shift + 3` move focused client to tag #3
- `Super + Shift + 4` move focused client to tag #4
- `Super + Shift + 5` move focused client to tag #5
- `Super + Shift + 6` move focused client to tag #6
- `Super + Shift + 7` move focused client to tag #7
- `Super + Shift + 8` move focused client to tag #8
- `Super + Shift + 9` move focused client to tag #9
