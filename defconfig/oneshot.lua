-- script which better run once on startup

local cwc = cwc

-- autostart app
cwc.spawn_with_shell("swaybg --output '*' --color '#222222'")
cwc.spawn_with_shell("waybar")
cwc.spawn_with_shell("playerctld daemon")
cwc.spawn_with_shell(
    'swayidle -w timeout 3600 "playerctl pause; wlopm --off \\*" resume "playerctl play; wlopm --on \\*"')

-- for app that use tray better to wait for the bar to load
cwc.spawn_with_shell("sleep 3 && copyq")
cwc.spawn_with_shell("sleep 3 && aria2tray --hide-window")

-- env var
cwc.setenv("HYPRCURSOR_THEME", "Bibata-Modern-Classic")

-- xdg-desktop-portal-wlr
cwc.spawn_with_shell("dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP")

-- load builtin cwc C plugin
local plugins_folder = cwc.is_nested() and "./build/plugins" or cwc.get_datadir() .. "/plugins"
cwc.plugin.load(plugins_folder .. "/cwcle.so")
cwc.plugin.load(plugins_folder .. "/flayout.so")
