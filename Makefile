SHELL := /bin/sh
BUILDDIR := build

all:
	@if [ ! -d "$(BUILDDIR)" ]; then meson setup $(BUILDDIR) -Dplugins=true -Dtests=true; fi
	@ninja -C $(BUILDDIR)/

all-release: docs
	@if [ ! -d "$(BUILDDIR)" ]; then meson setup $(BUILDDIR) -Dplugins=true -Dtests=true --buildtype=release; fi
	@ninja -C $(BUILDDIR)/

all-debugrelease:
	@if [ ! -d "$(BUILDDIR)" ]; then meson setup $(BUILDDIR) -Dplugins=true -Dtests=true --buildtype=debugoptimized; make docs; fi
	@ninja -C $(BUILDDIR)/

cwc:
	@if [ ! -d "$(BUILDDIR)" ]; then meson setup $(BUILDDIR); fi

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

.PHONY: cwc release clean install uninstall header format docs
