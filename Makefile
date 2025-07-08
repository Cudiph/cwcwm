SHELL := /bin/sh
BUILDDIR := build

default:
	@if [ ! -d "$(BUILDDIR)" ]; then meson setup $(BUILDDIR) --reconfigure; fi
	@ninja -C $(BUILDDIR)/

all:
	@if [ ! -d "$(BUILDDIR)" ]; then meson setup $(BUILDDIR) -Dplugins=true -Dtests=true; fi
	@ninja -C $(BUILDDIR)/

all-release: docs
	@if [ ! -d "$(BUILDDIR)" ]; then meson setup $(BUILDDIR) -Dplugins=true -Dtests=true --buildtype=release; fi
	@ninja -C $(BUILDDIR)/

all-debugrelease:
	@if [ ! -d "$(BUILDDIR)" ]; then meson setup $(BUILDDIR) -Dplugins=true -Dtests=true --buildtype=debugoptimized; make docs; fi
	@ninja -C $(BUILDDIR)/

clean:
	rm -rf ./$(BUILDDIR) ./doc

install:
	ninja -C $(BUILDDIR) install

uninstall:
	ninja -C $(BUILDDIR) uninstall

format:
	@find src plugins tests include cwctl -type f -name "*.[ch]" -print0 | xargs -0 clang-format -i --verbose
	@CodeFormat format -w .
	@CodeFormat format -f ./docs/config.ld --overwrite

docs:
	rm -rf doc
	cd docs && ldoc .

.PHONY: default all all-release all-debugrelease clean install uninstall format docs
