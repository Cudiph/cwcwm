#include <wlr/types/wlr_output_layout.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/layout/container.h"
#include "cwc/layout/master.h"
#include "cwc/plugin.h"

static void arrange_flayout(struct cwc_toplevel **toplevels,
                            int len,
                            struct cwc_output *output,
                            struct master_state *master_state)
{
    int i                         = 0;
    struct cwc_toplevel *toplevel = toplevels[i];

    while (toplevel) {
        cwc_container_set_position_gap(toplevel->container, 0, 0);
        cwc_container_set_size(toplevel->container,
                               output->output_layout_box.width,
                               output->output_layout_box.height);

        toplevel = toplevels[++i];
    }
}

struct layout_interface fullscreen_impl = {.name    = "fullscreen",
                                           .arrange = arrange_flayout};

static int init()
{
    master_register_layout(&fullscreen_impl);

    return 0;
}

/* crash when removing layout while its the current layout
 * TODO: refactor layout list to array instead of linked list.
 */
static void fini()
{
    master_unregister_layout(&fullscreen_impl);
}

plugin_init(init);
plugin_exit(fini);

PLUGIN_NAME("flayout");
PLUGIN_VERSION("0.4.0-dev");
PLUGIN_DESCRIPTION("f layout we go f screen");
PLUGIN_LICENSE("MIT");
PLUGIN_AUTHOR("Dwi Asmoro Bangun <dwiaceromo@gmail.com>");
