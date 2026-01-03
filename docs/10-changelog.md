# Change log

## v0.3.0

This release introduce keyboard object `cwc_kbd` and pointer object `cwc_pointer` which allow
lua config to set the state of layout, modifiers, key, and axis. Another notable change is `cwc.spawn` now able to capture the output and exit code.

### BREAKING CHANGES:

- Rename property signal from `<object>::property::<name>` to `<object::prop::<name>`.
  The signals affected are:

```md
1. "client::property::fullscreen"
2. "client::property::maximized"
3. "client::property::minimized"
4. "client::property::floating"
5. "client::property::urgent"
6. "client::property::tag"
7. "client::property::workspace"
```

- Remove certain config field functions in exchange for config library:

```md
1. cwc.client.set_border_color_rotation
2. cwc.client.set_border_color_normal
3. cwc.cwcle.set_border_color_raised
```

### New features:

- add `cwc_kbd` and `cwc_pointer` objects
- add `pass` options to kbindmap to allow key event still sent to the client
- add `data` property to all object for user data
- scroll wheel now can be used for mouse binding
- allow `cwc.spawn` and `cwc.spawn_with_shell` to capture its process output and exit code

### Fixes:

- fix cursor restoration when interactive ended
- fix --force-grab-cursor issue in gamescope
- fix XWayland popup
- fix cursor shape doesn't change after client change under the cursor

### Other changes:

- add options to build without XWayland
- add nix packaging
- swapping container won't emit "container::insert" signal
- show where config is loaded in log
- updated the localization files

## v0.2.0

This release introduce dwl-ipc plugin which add ability to show tags state
and focused client on a screen.

Below are the highlights of change made since last version.

BREAKING CHANGES:

> no breaking change.

New features:

- implement dwl-ipc-v2 protocol as a plugin
- implement content-type-v1 protocol
- add field in `cwc.kbd` to set xkb settings
- client default decoration mode and `cwc.client.decoration_mode` property
- `cwc.unlock_session` function to recover from crashed screen locker
- add `cwctl input`, `cwctl reload`, and `cwctl plugin`
- `cwc_plugin` object

Fixes:

- fix move to output doesn't move when in maximized/fullscreen
- fix floating client appear at wrong size on tiling layout
- fix crash when calling cwc.input.get on initial load

Other changes:

- allow `set_mode` to use the exact mHz
- allow passing userdata to callback function in `cwc.timer`
- `cwc.<table>` now has getter and setter
- `cwc.datadir` now follow XDG spec
- update meson packaging

## v0.1.0

Initial release of cwc
