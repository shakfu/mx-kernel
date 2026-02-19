ROOTDIR := $(shell pwd)
SRCDIR := $(ROOTDIR)/source
PROJECTS := $(SRCDIR)/projects
PKG_NAME = mx-kernel
MAX_VERSIONS := 8 9

.phony: all build clean setup update-submodules link connect test install-kernelspec

all: build


build: clean
	@mkdir -p build && cd build && \
		cmake .. && \
		cmake --build . --config Release

clean:
	@rm -rf exterals build

setup: update-submodules link
	$(call section,"setup complete")

update-submodules:
	$(call section,"updating git submodules")
	@git submodule init && git submodule update

test:
	@mkdir -p build-test && cd build-test && \
		cmake .. -DBUILD_TESTS=ON && \
		cmake --build . --target kernel_tests --config Release && \
		./source/projects/kernel/tests/kernel_tests

connect:
	@uv run jupyter console --existing $(HOME)/.local/share/jupyter/runtime/kernel-testkernel.json

install-kernelspec:
	@mkdir -p $(HOME)/.local/share/jupyter/kernels/mx-kernel
	@printf '{"argv":["echo","Connect via Max"],"display_name":"Max/MSP","language":"max"}' \
		> $(HOME)/.local/share/jupyter/kernels/mx-kernel/kernel.json
	@echo "Kernelspec installed at $(HOME)/.local/share/jupyter/kernels/mx-kernel/kernel.json"

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