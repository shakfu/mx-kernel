ROOTDIR := $(shell pwd)
SRCDIR := $(ROOTDIR)/source
PROJECTS := $(SRCDIR)/projects
PKG_NAME = mx-kernel
MAX_VERSIONS := 8 9

# Kernel instance name used by the `connect` target. Override on the command
# line to match the @name attribute of your object: make connect NAME=mine
NAME ?= testkernel

RUNTIME_DIR ?= $(HOME)/.local/share/jupyter/runtime

define section
	@echo ""
	@echo "-------------------------------------------------------------------"
	@echo ">>> $(1)"
	@echo "-------------------------------------------------------------------"
endef

.PHONY: all build rebuild clean setup update-submodules link connect test \
        test-cpp test-js install-kernelspec patch-thirdparty

all: build

# Incremental by default. Use `make rebuild` for a clean build.
build:
	$(call section,"building externals")
	@mkdir -p build && cd build && \
		cmake .. && \
		cmake --build . --config Release

rebuild: clean build

clean:
	$(call section,"cleaning build output")
	@rm -rf externals build build-test

setup: update-submodules link
	$(call section,"setup complete")

update-submodules:
	$(call section,"updating git submodules")
	@git submodule init && git submodule update

# Re-apply the local patches carried against the vendored dependencies.
# See patches/README.md -- the build does not depend on this, but refreshing
# a vendored library does.
patch-thirdparty:
	$(call section,"applying thirdparty patches")
	@./patches/apply.sh

test: test-cpp test-js

test-cpp:
	$(call section,"building and running C++ unit tests")
	@mkdir -p build-test && cd build-test && \
		cmake .. -DBUILD_TESTS=ON && \
		cmake --build . --target kernel_tests --config Release && \
		./source/projects/kernel/tests/kernel_tests

# The calculator example's parser is plain ES5 and testable outside Max.
# Skipped rather than failed when node is absent: it is not a build dependency.
test-js:
	$(call section,"running javascript tests")
	@if command -v node >/dev/null 2>&1; then \
		node javascript/tests/test_calc.js ; \
	else \
		echo "node not found -- skipping javascript tests" ; \
	fi

connect:
	@test -f "$(RUNTIME_DIR)/kernel-$(NAME).json" || \
		{ echo "No connection file for '$(NAME)'."; \
		  echo "Start a [kernel @name $(NAME)] object in Max first,"; \
		  echo "or pass the right name: make connect NAME=<your-kernel-name>"; \
		  exit 1; }
	@uv run jupyter console --existing $(RUNTIME_DIR)/kernel-$(NAME).json

install-kernelspec:
	@mkdir -p $(HOME)/.local/share/jupyter/kernels/mx-kernel
	@printf '{"argv":["echo","Start the kernel from a Max patch, then connect with --existing"],"display_name":"Max/MSP (connect to a running patch)","language":"max"}' \
		> $(HOME)/.local/share/jupyter/kernels/mx-kernel/kernel.json
	@echo "Kernelspec installed at $(HOME)/.local/share/jupyter/kernels/mx-kernel/kernel.json"
	@echo "Note: this only makes the kernel discoverable by name. It cannot launch"
	@echo "a kernel -- start one from a Max patch and connect with --existing."

link:
	$(call section,"symlink to Max 'Packages' Directories")
	@for MAX_VERSION in $(MAX_VERSIONS); do \
		MAX_DIR="Max $${MAX_VERSION}" ; \
		PACKAGES="$(HOME)/Documents/$${MAX_DIR}/Packages" ; \
		PY_PACKAGE="$${PACKAGES}/$(PKG_NAME)" ; \
		if [ -d "$${PACKAGES}" ]; then \
			echo "symlinking to $${PY_PACKAGE}" ; \
			if ! [ -L "$${PY_PACKAGE}" ]; then \
				ln -s "$(ROOTDIR)" "$${PY_PACKAGE}" ; \
				echo "... symlink created" ; \
			else \
				echo "... symlink already exists" ; \
			fi \
		fi \
	done
