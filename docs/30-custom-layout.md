# Custom Layout

Custom layouts are only applicable for the master/stack layout mode. Layouts for other modes need to be
integrated to the compositor itself.

## Custom layout in C

To create a custom layout in C, implement the `struct layout_interface`. The structure is as follows:

```C
struct layout_interface {
    char *name;

    /* toplevels doesn't include maximized/fullscreen or not tiled client, so
     * the implementation can focus arranging the given toplevels.
     */
    void (*arrange)(struct cwc_toplevel **toplevels,
                    int len,
                    struct cwc_output *output,
                    struct master_state *master_state);

    // private...
};
```

The field `name` is the name of the layout and the `arrange` is function to arrange
the layout.

- `toplevels` - array of toplevels that can be arranged.
- `len` - length of the toplevel array
- `output` - screen where all the toplevel placed.
- `master_state` - info such as such as master width factor, master_count, etc.

Let's take a look at `monocle` layout for the simplest implementation how to arrange the toplevels.
The monocle layout just sets all the tileable toplevels to the size of the workarea/usable area.

```C
static void arrange_monocle(struct cwc_toplevel **toplevels,
                            int len,
                            struct cwc_output *output,
                            struct master_state *master_state)
{
    int i                         = 0;
    struct cwc_toplevel *toplevel = toplevels[i];

    // You can also loop through the array by checking if the element is NULL.
    // The end of the array is guaranteed to be NULL.
    while (toplevel) {
        cwc_container_set_position_gap(
            toplevel->container, output->usable_area.x, output->usable_area.y);
        cwc_container_set_size(toplevel->container, output->usable_area.width,
                               output->usable_area.height);

        toplevel = toplevels[++i];
    }
}
```

One thing to keep in mind is to set the toplevel size and position we use `cwc_container_set_xxx`
instead of `cwc_toplevel_set_xxx` because the toplevel decoration need to be accounted. Also
you don't need to worry about the gap as it's already taken care of by both `set_position_gap` and
`set_size`.

Once the interface is implemented, now register it by using `master_register_layout`.
To create a layout as a plugin see @{80-c-plugin.md}.

```C
static void master_register_monocle()
{
    struct layout_interface *monocle_impl = calloc(1, sizeof(*monocle_impl));
    monocle_impl->name                    = "monocle";
    monocle_impl->arrange                 = arrange_monocle;

    master_register_layout(monocle_impl);
}
```

To remove it use `master_unregister_layout`.

```C
master_unregister_layout(&monocle_impl);
free(monocle_impl);
```

## Custom layout in Lua

Planned.
