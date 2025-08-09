# Change log

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
