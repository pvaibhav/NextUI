#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "displaycal_core.h"
#include "displaycal_ui.h"

static int streq(const char *a, const char *b) {
	return strcmp(a, b) == 0;
}

static void usage(const char *argv0) {
	fprintf(stderr,
		"Usage:\n"
		"  %s apply [config]\n"
		"  %s status [config]\n"
		"  %s disable [screen]\n"
		"  %s identity [screen]\n"
		"  %s ui [config]\n",
		argv0, argv0, argv0, argv0, argv0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		usage(argv[0]);
		return 2;
	}

	if (streq(argv[1], "apply")) {
		DisplayCalConfig config;
		displaycal_init_config(&config);
		if (argc >= 3)
			displaycal_load_config(&config, argv[2]);
		return displaycal_apply_config(&config) < 0 ? 1 : 0;
	}

	if (streq(argv[1], "status")) {
		DisplayCalConfig config;
		displaycal_init_config(&config);
		if (argc >= 3)
			displaycal_load_config(&config, argv[2]);
		displaycal_print_status(&config);
		return 0;
	}

	if (streq(argv[1], "disable")) {
		int screen = argc >= 3 ? atoi(argv[2]) : 0;
		return displaycal_disable_screen(screen) < 0 ? 1 : 0;
	}

	if (streq(argv[1], "identity")) {
		int screen = argc >= 3 ? atoi(argv[2]) : 0;
		return displaycal_apply_identity(screen) < 0 ? 1 : 0;
	}

	if (streq(argv[1], "ui")) {
		const char *config_path = argc >= 3 ? argv[2] : USERDATA_PATH "/displaycal.cfg";
		return displaycal_run_ui(config_path);
	}

	usage(argv[0]);
	return 2;
}
