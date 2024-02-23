.PHONY: build clean
all: build

build:
	$(MAKE) -C shared
	$(MAKE) -C enclave_node
	$(MAKE) -C dpi
	$(MAKE) -C coordination_server

clean:
	$(MAKE) -C shared clean
	$(MAKE) -C enclave_node clean
	$(MAKE) -C dpi clean
	$(MAKE) -C coordination_server clean

