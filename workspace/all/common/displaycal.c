#include "displaycal.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DISP_LCD_SET_GAMMA_TABLE          0x10b
#define DISP_LCD_GAMMA_CORRECTION_ENABLE  0x10c
#define DISP_LCD_GAMMA_CORRECTION_DISABLE 0x10d

#define DISPLAYCAL_LUT_BYTES   (DISPLAYCAL_LUT_ENTRIES * 4)

#define DISPLAYCAL_CUBE_MAX_LINE 256
#define DISPLAYCAL_CUBE_MAX_LINES 8192
#define DISPLAYCAL_CUBE_MAX_BYTES (256 * 1024)
#define DISPLAYCAL_CUBE_PATH_MAX 512

typedef struct {
	double red_gain;
	double green_gain;
	double blue_gain;
} DisplayCalGains;

static unsigned char clamp_u8(double value) {
	if (value < 0.0)
		return 0;
	if (value > 255.0)
		return 255;
	return (unsigned char)(value + 0.5);
}

static double srgb_to_linear(double c) {
	if (c <= 0.04045)
		return c / 12.92;
	return pow((c + 0.055) / 1.055, 2.4);
}

static double linear_to_srgb(double c) {
	if (c <= 0.0)
		return 0.0;
	if (c <= 0.0031308)
		return c * 12.92;
	return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

static void fill_linear_gain_table(uint32_t table[DISPLAYCAL_LUT_ENTRIES], const DisplayCalGains *gains) {
	for (int i = 0; i < DISPLAYCAL_LUT_ENTRIES; i++) {
		double linear = srgb_to_linear(i / 255.0);
		uint32_t r = clamp_u8(linear_to_srgb(linear * gains->red_gain) * 255.0);
		uint32_t g = clamp_u8(linear_to_srgb(linear * gains->green_gain) * 255.0);
		uint32_t b = clamp_u8(linear_to_srgb(linear * gains->blue_gain) * 255.0);
		table[i] = (r << 16) | (g << 8) | b;
	}
}

static void fill_identity_table(uint32_t table[DISPLAYCAL_LUT_ENTRIES]) {
	for (int i = 0; i < DISPLAYCAL_LUT_ENTRIES; i++)
		table[i] = ((uint32_t)i << 16) | ((uint32_t)i << 8) | (uint32_t)i;
}

static int open_disp(void) {
	int fd = open("/dev/disp", O_RDWR);
	if (fd < 0)
		fprintf(stderr, "displaycal: open /dev/disp failed: %s\n", strerror(errno));
	return fd;
}

static int disable_gamma(int fd, int screen) {
	unsigned long param[4] = { (unsigned long)screen, 0, 0, 0 };
	if (ioctl(fd, DISP_LCD_GAMMA_CORRECTION_DISABLE, param) < 0) {
		fprintf(stderr, "displaycal: disable gamma failed: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static int set_gamma_table(int fd, int screen, uint32_t table[DISPLAYCAL_LUT_ENTRIES]) {
	unsigned long set_param[4] = {
		(unsigned long)screen,
		(unsigned long)table,
		DISPLAYCAL_LUT_BYTES,
		0,
	};

	if (ioctl(fd, DISP_LCD_SET_GAMMA_TABLE, set_param) < 0) {
		fprintf(stderr, "displaycal: set gamma table failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int enable_gamma(int fd, int screen) {
	unsigned long enable_param[4] = { (unsigned long)screen, 0, 0, 0 };
	if (ioctl(fd, DISP_LCD_GAMMA_CORRECTION_ENABLE, enable_param) < 0) {
		fprintf(stderr, "displaycal: enable gamma failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int apply_table(int fd, int screen, uint32_t table[DISPLAYCAL_LUT_ENTRIES]) {
	if (set_gamma_table(fd, screen, table) < 0)
		return -1;
	return enable_gamma(fd, screen);
}

static int reset_table_and_disable(int fd, int screen) {
	uint32_t table[DISPLAYCAL_LUT_ENTRIES];
	fill_identity_table(table);
	if (set_gamma_table(fd, screen, table) < 0)
		return -1;
	return disable_gamma(fd, screen);
}

static int apply_gains(const DisplayCalGains *gains) {
	int fd = open_disp();
	if (fd < 0)
		return -1;

	uint32_t table[DISPLAYCAL_LUT_ENTRIES];
	fill_linear_gain_table(table, gains);
	int ret = apply_table(fd, 0, table);
	close(fd);
	return ret;
}

static int is_cube_space(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Parse exactly three finite doubles and require end-of-line after them.
static int parse_cube_triplet(const char *s, double out[3]) {
	for (int i = 0; i < 3; i++) {
		while (is_cube_space(*s))
			s++;
		if (*s == '\0')
			return -1;
		char *end = NULL;
		double value = strtod(s, &end);
		if (end == s || !isfinite(value))
			return -1;
		out[i] = value;
		s = end;
	}
	while (is_cube_space(*s))
		s++;
	return *s == '\0' ? 0 : -1;
}

// Match KEYWORD at the start of s; on success return the pointer just past it.
static const char *match_cube_keyword(const char *s, const char *keyword) {
	size_t len = strlen(keyword);
	if (strncmp(s, keyword, len) != 0)
		return NULL;
	if (s[len] != '\0' && !is_cube_space(s[len]))
		return NULL;
	return s + len;
}

int DisplayCal_parseCubeFile(const char *path, uint32_t table[DISPLAYCAL_LUT_ENTRIES]) {
	if (!path || !table)
		return -1;

	FILE *file = fopen(path, "r");
	if (!file) {
		fprintf(stderr, "displaycal: open %s failed: %s\n", path, strerror(errno));
		return -1;
	}

	int size_ok = 0;
	if (fseek(file, 0, SEEK_END) == 0) {
		long size = ftell(file);
		size_ok = (size >= 0 && size <= DISPLAYCAL_CUBE_MAX_BYTES);
		rewind(file);
	}
	if (!size_ok) {
		fprintf(stderr, "displaycal: %s too large or unreadable\n", path);
		fclose(file);
		return -1;
	}

	double lut[DISPLAYCAL_LUT_ENTRIES][3];
	int rows = 0;
	int have_size = 0;
	long lines = 0;
	char line[DISPLAYCAL_CUBE_MAX_LINE];

	while (fgets(line, sizeof(line), file)) {
		if (++lines > DISPLAYCAL_CUBE_MAX_LINES) {
			fprintf(stderr, "displaycal: %s has too many lines\n", path);
			goto fail;
		}
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] != '\n' && !feof(file)) {
			fprintf(stderr, "displaycal: %s has an overlong line\n", path);
			goto fail;
		}

		const char *s = line;
		while (is_cube_space(*s))
			s++;
		if (*s == '\0' || *s == '#')
			continue;

		const char *rest;
		if ((rest = match_cube_keyword(s, "TITLE")) != NULL) {
			(void)rest; // display label only
			continue;
		}
		if ((rest = match_cube_keyword(s, "LUT_1D_SIZE")) != NULL) {
			if (have_size) {
				fprintf(stderr, "displaycal: %s has a duplicate LUT_1D_SIZE\n", path);
				goto fail;
			}
			char *end = NULL;
			long size = strtol(rest, &end, 10);
			while (is_cube_space(*end))
				end++;
			if (end == rest || *end != '\0' || size != DISPLAYCAL_LUT_ENTRIES) {
				fprintf(stderr, "displaycal: %s has unsupported LUT_1D_SIZE (want %d)\n",
					path, DISPLAYCAL_LUT_ENTRIES);
				goto fail;
			}
			have_size = 1;
			continue;
		}
		if (match_cube_keyword(s, "LUT_3D_SIZE") != NULL) {
			fprintf(stderr, "displaycal: %s contains a 3D LUT, unsupported\n", path);
			goto fail;
		}
		if ((rest = match_cube_keyword(s, "DOMAIN_MIN")) != NULL) {
			double d[3];
			if (parse_cube_triplet(rest, d) != 0 ||
					fabs(d[0]) > 1e-6 || fabs(d[1]) > 1e-6 || fabs(d[2]) > 1e-6) {
				fprintf(stderr, "displaycal: %s has a non-zero DOMAIN_MIN\n", path);
				goto fail;
			}
			continue;
		}
		if ((rest = match_cube_keyword(s, "DOMAIN_MAX")) != NULL) {
			double d[3];
			if (parse_cube_triplet(rest, d) != 0 ||
					fabs(d[0] - 1.0) > 1e-6 ||
					fabs(d[1] - 1.0) > 1e-6 ||
					fabs(d[2] - 1.0) > 1e-6) {
				fprintf(stderr, "displaycal: %s has a non-identity DOMAIN_MAX\n", path);
				goto fail;
			}
			continue;
		}

		// Anything else must be a data row after the size header.
		if (!have_size) {
			fprintf(stderr, "displaycal: %s has data before LUT_1D_SIZE\n", path);
			goto fail;
		}
		if (rows >= DISPLAYCAL_LUT_ENTRIES) {
			fprintf(stderr, "displaycal: %s has more than %d data rows\n",
				path, DISPLAYCAL_LUT_ENTRIES);
			goto fail;
		}
		if (parse_cube_triplet(s, lut[rows]) != 0) {
			fprintf(stderr, "displaycal: %s has invalid data on line %ld\n", path, lines);
			goto fail;
		}
		rows++;
	}

	if (ferror(file)) {
		fprintf(stderr, "displaycal: error reading %s\n", path);
		goto fail;
	}
	if (!have_size) {
		fprintf(stderr, "displaycal: %s is missing LUT_1D_SIZE\n", path);
		goto fail;
	}
	if (rows != DISPLAYCAL_LUT_ENTRIES) {
		fprintf(stderr, "displaycal: %s has %d data rows, want %d\n",
			path, rows, DISPLAYCAL_LUT_ENTRIES);
		goto fail;
	}

	for (int i = 0; i < DISPLAYCAL_LUT_ENTRIES; i++) {
		uint32_t r = clamp_u8(lut[i][0] * 255.0);
		uint32_t g = clamp_u8(lut[i][1] * 255.0);
		uint32_t b = clamp_u8(lut[i][2] * 255.0);
		table[i] = (r << 16) | (g << 8) | b;
	}
	fclose(file);
	return 0;

fail:
	fclose(file);
	return -1;
}

int DisplayCal_enableWithLutFile(const char *path) {
	uint32_t table[DISPLAYCAL_LUT_ENTRIES];
	if (DisplayCal_parseCubeFile(path, table) != 0)
		return -1;

	int fd = open_disp();
	if (fd < 0)
		return -1;

	int ret = apply_table(fd, 0, table);
	close(fd);
	return ret;
}

int DisplayCal_enableWithDeviceFile(void) {
	const char *sdcard = getenv("SDCARD_PATH");
	const char *device = getenv("DEVICE");
	if (!sdcard || !*sdcard || !device || !*device) {
		fprintf(stderr, "displaycal: SDCARD_PATH or DEVICE not set\n");
		return -1;
	}

	char path[DISPLAYCAL_CUBE_PATH_MAX];
	int len = snprintf(path, sizeof(path), "%s/.system/res/displaycal/%s.cube", sdcard, device);
	if (len < 0 || (size_t)len >= sizeof(path)) {
		fprintf(stderr, "displaycal: device LUT path too long\n");
		return -1;
	}
	return DisplayCal_enableWithLutFile(path);
}

int DisplayCal_enableWithValues(int red_gain, int green_gain, int blue_gain) {
	DisplayCalGains gains = {
		.red_gain = (double)DisplayCal_clampGainValue(red_gain) / DISPLAYCAL_GAIN_SCALE,
		.green_gain = (double)DisplayCal_clampGainValue(green_gain) / DISPLAYCAL_GAIN_SCALE,
		.blue_gain = (double)DisplayCal_clampGainValue(blue_gain) / DISPLAYCAL_GAIN_SCALE,
	};
	return apply_gains(&gains);
}

int DisplayCal_disable(void) {
	int fd = open_disp();
	if (fd < 0)
		return -1;

	int ret = reset_table_and_disable(fd, 0);
	close(fd);
	return ret;
}
