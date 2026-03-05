# C Plugin

DISCLAIMER: By writing or loading a C plugin, the Writer assumes you already know what you're doing and
is not responsible for segfault, loss of data, kernel panic, bad API design,
thermonuclear war, or the current economic crisis. Always make sure the plugin you want to load
doesn't do anything malicious.

## Loading C Plugin

Loading C plugin can be done by using the `cwc.plugin.load` API. A plugin can be unload using
`cwc.plugin.unload_byname`. If the plugin already exist `cwc.plugin.load` won't reload it,
if the plugin doesn't support unloading `unload_byname` changes nothing.

## Writing C Plugin

You may want to write a C Plugin because you need full access to CwC internals and perhaps you
want to add an amazing feature so that CwC becomes more awesome (pun intended). Knowledge of the CwC
internals and wlroots might be necessary to start hacking.

The API for writing the C plugin is very similar to writing a linux kernel module; in fact some of the code
is from the linux `module.h`, the difference is that it start with `PLUGIN` instead of `MODULE`.

What it does under the hood is it just loads the shared object using dlopen.
Let's take a look at `flayout.c` plugin for example.

```C
#include <wlr/types/wlr_output_layout.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/layout/master.h"
#include "cwc/plugin.h"
#include "cwc/server.h"

// function arrange_flayout...

struct layout_interface fullscreen_impl = {.name    = "fullscreen",
                                           .arrange = arrange_flayout};

static int init()
{
    master_register_layout(&fullscreen_impl);

    return 0;
}

static void fini()
{
    master_unregister_layout(&fullscreen_impl);
}

plugin_init(init);
plugin_exit(fini);

PLUGIN_NAME("flayout");
PLUGIN_VERSION("0.1.0");
PLUGIN_DESCRIPTION("f layout we go f screen");
PLUGIN_LICENSE("MIT");
PLUGIN_AUTHOR("Dwi Asmoro Bangun <dwiaceromo@gmail.com>");
```

First we includes the CwC header and other library headers that we need. All the public CwC headers
are located at `/usr/include/cwc`, may need to adjust a compiler flag.

```C
#include <wlr/types/wlr_output_layout.h>

#include "cwc/desktop/output.h"
#include "cwc/desktop/toplevel.h"
#include "cwc/layout/master.h"
#include "cwc/plugin.h"
#include "cwc/server.h"
```

Then create an entry point for the plugin load to call. The function name can be anything
as long as you call `plugin_init` macro with the function name as argument.
The function signature is `int (*name)(void)`, the return value is zero if successful and non-zero if there is an error
in case something changes in the future.

```C
static int init_can_be_any_name()
{
    master_register_layout(&fullscreen_impl);

    return 0;
}

plugin_init(init_can_be_any_name);
```

If the plugin supports unloading, the `plugin_exit` macro can be used to mark a function to call
before the plugin is unloaded. If the plugin doesn't support unloading you may omit it.
If you support unloading but doesn't need a cleanup just pass an empty function.
The function signature is `void (*name)(void)`.

```C
static void fini()
{
    master_unregister_layout(&fullscreen_impl);
}

plugin_exit(fini);
```

Lastly, plugin metadata. `PLUGIN_NAME` and `PLUGIN_VERSION` is mandatory so that the loader can
manage it.

```C
PLUGIN_NAME("flayout");
PLUGIN_VERSION("0.1.0");
PLUGIN_DESCRIPTION("f layout we go f screen");
PLUGIN_LICENSE("MIT");
PLUGIN_AUTHOR("Dwi Asmoro Bangun <dwiaceromo@gmail.com>");
```

After writing the plugin, compile it to a shared object file (.so) with linker flags
`--allow-shlib-undefined`, `-shared`, and `--as-needed`

If you want to create a lua API in the plugin the convention used is `cwc.<plugin_name>`. For example
in `cwcle` the `PLUGIN_NAME` is `cwcle` so on the lua side it should be accessed with `cwc.cwcle`.
