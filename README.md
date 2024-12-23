# README

## About CwC

CwC is an extensible Wayland compositor with dynamic window management based on wlroots.
Highly influenced by Awesome window manager, cwc use lua for its configuration and C plugin for extension.

For new user you may want to check out [getting started][getting_started] page.

## Building and installation

Required dependencies:

- wayland
- wlroots 0.19 (git)
- hyprcursor
- cairo
- xkbcommon
- libinput
- xxhash
- luajit
- XWayland
- xcb

Lua library dependencies:

- LGI
- cairo with support for GObject introspection
- Pango with support for GObject introspection

Dev dependencies:

- meson
- ninja
- wayland-protocols
- clang-format & EmmyLuaCodeStyle (formatting)

Build and installation step:

```console
$ make all-release
$ sudo make install
```

CwC now should be available in the display manager or execute `cwc` in the tty.

To clear the installation and build items you can execute this command:

```console
$ sudo make uninstall
$ make clean
```

<div align="center">
  <h2>Screenshot</h2>
  <img src="https://github.com/user-attachments/assets/99c3681a-e68c-4936-84be-586d8b2f04ad" alt="screenshot" />
</div>


## Credits

CwC took ~~inspiration~~ code from these awesome projects:

- [Awesome](https://github.com/awesomeWM/awesome)
- [dwl](https://codeberg.org/dwl/dwl)
- [Hikari](https://hub.darcs.net/raichoo/hikari)
- [Hyprland](https://github.com/hyprwm/Hyprland)
- [Sway](https://github.com/swaywm/sway)
- [TinyWL](https://gitlab.freedesktop.org/wlroots/wlroots)

See [LICENSE.md](LICENSE.md) for license details.


[getting_started]: https://cudiph.github.io/cwc/apidoc/documentation/00-getting-started.md.html
