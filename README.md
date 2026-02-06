# README

## About CwC

CwC is an extensible Wayland compositor with dynamic window management based on wlroots. 
Highly influenced by [awesome window manager](https://awesomewm.org), CwC uses Lua for its
configuration and C plugins for extensions.

For new users, you may want to check out the [getting started][getting_started] page.

## Stability state

Crashes may happen so daily driving isn't recommended unless you're okay with them.
If you encounter any crashes, please report them by [creating an issue][github-issue] with steps to
reproduce. I will fix it as quickly as possible because I also daily drive it and
I want my setup to be super stable.

API breaking change is documented with exclamation mark (`!`) in the commit
message as per [conventional commits specification][conventional-commits].
APIs that derived from AwesomeWM are unlikely to get a change therefore
guaranteed not to break your configuration.

## Features

- Very configurable, fast, and lightweight.
- Hot reload configuration support.
- Can be used as floating/tiling window manager.
- Tabbed windows.
- Tags instead of workspaces.
- Documented Lua API.
- wlr protocols support.
- Multihead support with hotplugging and restore.

## Building and installation

Required dependencies:

- wayland
- wlroots 0.20 (git)
- hyprcursor
- cairo
- xkbcommon
- libinput
- xxhash
- LuaJIT
- Xwayland
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
## Installation

<summary>Packaging status</summary>
<a href="https://repology.org/project/cwc-misnamed/versions">
    <img src="https://repology.org/badge/vertical-allrepos/cwc-misnamed.svg?columns=3" alt="Packaging status">
</a>


<details>
<summary>Arch Linux</summary>

```
git clone https://aur.archlinux.org/cwc.git
cd cwc
makepkg -si
```
</details>

<details>
<summary>Gentoo</summary>

```
eselect repository enable zuki
emaint sync
emerge \=gui-wm/cwcwm-0.2.0 or \=gui-wm/cwcwm-9999 for the git version
```
</details>

<details>
<summary>NixOS</summary>

```
nix-env -iA nixos.cwc
```
</details>

### Manual

```console
$ git clone https://github.com/Cudiph/cwcwm/
$ cd cwcwm
$ make
$ sudo make install
```

CwC now should be available in your display manager or execute `cwc` in a tty.

To clear the installation and build artifacts, you can execute:

```
$ sudo make uninstall
$ make clean
```



<div align="center">
  <h2>Screenshot</h2>
  <img src="https://github.com/user-attachments/assets/99c3681a-e68c-4936-84be-586d8b2f04ad" alt="screenshot" />
</div>

## Credits

CwC contains verbatim or modified works from these awesome projects:

- [awesome](https://github.com/awesomeWM/awesome)
- [dwl](https://codeberg.org/dwl/dwl)
- [Hikari](https://hub.darcs.net/raichoo/hikari)
- [Hyprland](https://github.com/hyprwm/Hyprland)
- [Sway](https://github.com/swaywm/sway)
- [TinyWL](https://gitlab.freedesktop.org/wlroots/wlroots)

See [LICENSE.md](LICENSE.md) for license details.

<!-------------------- links -------------------->

[getting_started]: https://cudiph.github.io/cwc/apidoc/documentation/00-getting-started.md.html
[github-issue]: https://github.com/Cudiph/cwcwm/issues
[conventional-commits]: https://www.conventionalcommits.org/en/v1.0.0/#commit-message-with--to-draw-attention-to-breaking-change
[cwc]: https://aur.archlinux.org/packages/cwc
[cwc-git]: https://aur.archlinux.org/packages/cwc-git
