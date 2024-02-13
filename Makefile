.PHONY: build clean
all: build

build:
	$(MAKE) -C shared
	$(MAKE) -C compute_server
	$(MAKE) -C client
	$(MAKE) -C register_server

clean:
	$(MAKE) -C shared clean
	$(MAKE) -C compute_server clean
	$(MAKE) -C client clean
	$(MAKE) -C register_server clean

