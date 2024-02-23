#include <stdlib.h>
#include <string>
#include "dpi.h"


int main(int argc, const char **argv) {
	// Parse command line arguments
	if (argc != 2) {
		printf("Usage: ./dpi [config_file]\n");
		return 1;
	}

	DPI dpi(argv[1]);
	dpi.run();

	return 0;
}