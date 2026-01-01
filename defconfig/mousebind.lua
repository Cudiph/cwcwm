-- Mouse binding

local cful = require("cuteful")
local gears = require("gears")
local cwc = cwc

local enum = cful.enum
local mod = enum.modifier
local button = enum.mouse_btn
local direction = enum.direction

local MODKEY = mod.LOGO
local TERMINAL = "kitty"
local kbd = cwc.kbd

-- prevent hotkey conflict on nested session
if cwc.is_nested() then
    MODKEY = mod.ALT
end

------------------- pointer/mouse binding ---------------------
local pointer = cwc.pointer

-- client interactive mode
pointer.bind(MODKEY, button.LEFT, pointer.move_interactive)
pointer.bind(MODKEY, button.RIGHT, pointer.resize_interactive)
pointer.bind(MODKEY, button.SCROLL_UP, function()
    cful.tag.viewprev()
end)
pointer.bind(MODKEY, button.SCROLL_DOWN, function()
    cful.tag.viewnext()
end)

------------------ Swipe Gestures ---------------------
local prev_active_tag = 1
cful.pointer.bind_swipe(3, direction.RIGHT, function()
    prev_active_tag = cwc.screen.focused().active_tag
    cful.tag.viewnext()
end, function()
    cwc.screen.focused().active_tag = prev_active_tag
end, { skip_events = true, threshold = 250, cancel_threshold = 60 })

cful.pointer.bind_swipe(3, direction.LEFT, function()
    prev_active_tag = cwc.screen.focused().active_tag
    cful.tag.viewprev()
end, function()
    cwc.screen.focused().active_tag = prev_active_tag
end)

------------------- Keyboard as a mouse ----------------------
local mouse_map = kbd.create_bindmap()
mouse_map.active = false

-- toggle this submap by pressing MOD + z
kbd.bind({ MODKEY }, "z", function()
    mouse_map.active = not mouse_map.active
end, { description = "activate mouse submap", group = "keymap" })

-- exit this submap by pressing Esc
mouse_map:bind({}, "Escape", function()
    mouse_map.active = false
end)

-- apparently the main device (seat0) is available after the lua is done initializing
local main_ptr = cwc.pointer.get()[1]
local main_kbd = cwc.kbd.get()[1]
cwc.timer.delayed_call(function()
    main_ptr = cwc.pointer.get()[1]
    main_kbd = cwc.kbd.get()[1]
end)

local PTR_SPEED = 10
local AXIS_DELTA = 15
local DELTA_DISCRETE = 120
local MULTIPLIER = 10

-- mouse movement
mouse_map:bind({}, "h", function()
    main_ptr:move(-PTR_SPEED, 0)
end, { description = "move pointer to the left", group = "pointer", repeated = true })
mouse_map:bind({}, "l", function()
    main_ptr:move(PTR_SPEED, 0)
end, { description = "move pointer to the right", group = "pointer", repeated = true })
mouse_map:bind({}, "k", function()
    main_ptr:move(0, -PTR_SPEED)
end, { description = "move pointer up", group = "pointer", repeated = true })
mouse_map:bind({}, "j", function()
    main_ptr:move(0, PTR_SPEED)
end, { description = "move pointer down", group = "pointer", repeated = true })

mouse_map:bind({ mod.SHIFT }, "h", function()
    main_ptr:move(-PTR_SPEED * MULTIPLIER, 0)
end, { description = "long move pointer to the left", group = "pointer", repeated = true })
mouse_map:bind({ mod.SHIFT }, "l", function()
    main_ptr:move(PTR_SPEED * MULTIPLIER, 0)
end, { description = "long move pointer to the right", group = "pointer", repeated = true })
mouse_map:bind({ mod.SHIFT }, "k", function()
    main_ptr:move(0, -PTR_SPEED * MULTIPLIER)
end, { description = "long move pointer up", group = "pointer", repeated = true })
mouse_map:bind({ mod.SHIFT }, "j", function()
    main_ptr:move(0, PTR_SPEED * MULTIPLIER)
end, { description = "long move pointer down", group = "pointer", repeated = true })

-- mouse axis
local last_mod_state = 0
mouse_map:bind({ mod.CTRL }, "h", function()
    last_mod_state = main_kbd.modifiers
    main_kbd:update_modifiers(0, 0, 0)
    main_ptr:send_axis(-AXIS_DELTA, -DELTA_DISCRETE, true)
    main_kbd:update_modifiers(last_mod_state, 0, 0)
end, { description = "scroll left", group = "pointer", repeated = true })
mouse_map:bind({ mod.CTRL }, "l", function()
    last_mod_state = main_kbd.modifiers
    main_kbd:update_modifiers(0, 0, 0)
    main_ptr:send_axis(AXIS_DELTA, DELTA_DISCRETE, true)
    main_kbd:update_modifiers(last_mod_state, 0, 0)
end, { description = "scroll right", group = "pointer", repeated = true })
mouse_map:bind({ mod.CTRL }, "k", function()
    last_mod_state = main_kbd.modifiers
    main_kbd:update_modifiers(0, 0, 0)
    main_ptr:send_axis(-AXIS_DELTA, -DELTA_DISCRETE)
    main_kbd:update_modifiers(last_mod_state, 0, 0)
end, { description = "scroll up", group = "pointer", repeated = true })
mouse_map:bind({ mod.CTRL }, "j", function()
    last_mod_state = main_kbd.modifiers
    main_kbd:update_modifiers(0, 0, 0)
    main_ptr:send_axis(AXIS_DELTA, DELTA_DISCRETE)
    main_kbd:update_modifiers(last_mod_state, 0, 0)
end, { description = "scroll down", group = "pointer", repeated = true })

-- mouse button click
mouse_map:bind({}, "d", function()
    main_ptr:send_key(enum.mouse_btn.LEFT, enum.key_state.PRESSED)
    main_ptr:send_key(enum.mouse_btn.LEFT, enum.key_state.RELEASED)
end, { description = "mouse left click", group = "pointer" })
mouse_map:bind({}, "s", function()
    main_ptr:send_key(enum.mouse_btn.MIDDLE, enum.key_state.PRESSED)
    main_ptr:send_key(enum.mouse_btn.MIDDLE, enum.key_state.RELEASED)
end, { description = "mouse middle click", group = "pointer" })
mouse_map:bind({}, "a", function()
    main_ptr:send_key(enum.mouse_btn.RIGHT, enum.key_state.PRESSED)
    main_ptr:send_key(enum.mouse_btn.RIGHT, enum.key_state.RELEASED)
end, { description = "mouse right click", group = "pointer" })
mouse_map:bind({}, "e", function()
    main_ptr:send_key(enum.mouse_btn.EXTRA, enum.key_state.PRESSED)
    main_ptr:send_key(enum.mouse_btn.EXTRA, enum.key_state.RELEASED)
end, { description = "mouse forward thumb", group = "pointer" })
mouse_map:bind({}, "q", function()
    main_ptr:send_key(enum.mouse_btn.SIDE, enum.key_state.PRESSED)
    main_ptr:send_key(enum.mouse_btn.SIDE, enum.key_state.RELEASED)
end, { description = "mouse back thumb", group = "pointer" })
