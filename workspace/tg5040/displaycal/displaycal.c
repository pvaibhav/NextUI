#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <msettings.h>

#include "sdl.h"
#include "defines.h"
#include "api.h"

#define DISP_LCD_SET_GAMMA_TABLE          0x10b
#define DISP_LCD_GAMMA_CORRECTION_ENABLE  0x10c
#define DISP_LCD_GAMMA_CORRECTION_DISABLE 0x10d

#define DISPLAYCAL_LUT_ENTRIES 256
#define DISPLAYCAL_LUT_BYTES   (DISPLAYCAL_LUT_ENTRIES * 4)

#define DEFAULT_RED_GAIN   1.0000000000000000
#define DEFAULT_GREEN_GAIN 0.9233642796405507
#define DEFAULT_BLUE_GAIN  0.5833412353395729

#define GAIN_MIN 0.0
#define GAIN_MAX 2.0
#define GAIN_STEP 0.01

typedef struct {
	int enabled;
	int screen;
	double strength;
	double red_gain;
	double green_gain;
	double blue_gain;
	char format[4];
} DisplayCalConfig;

typedef struct {
	const char *name;
	int r;
	int g;
	int b;
} Pattern;

static bool quit = false;

static const Pattern patterns[] = {
	{"White", 255, 255, 255},
	{"Grey", 128, 128, 128},
	{"Red", 255, 0, 0},
	{"Green", 0, 255, 0},
	{"Blue", 0, 0, 255},
	{"Cyan", 0, 255, 255},
	{"Magenta", 255, 0, 255},
	{"Yellow", 255, 255, 0},
};

static void sigHandler(int sig) {
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		quit = true;
		break;
	default:
		break;
	}
}

static void init_config(DisplayCalConfig *config) {
	config->enabled = 1;
	config->screen = 0;
	config->strength = 1.0;
	config->red_gain = DEFAULT_RED_GAIN;
	config->green_gain = DEFAULT_GREEN_GAIN;
	config->blue_gain = DEFAULT_BLUE_GAIN;
	strcpy(config->format, "rgb");
}

static double clamp_unit(double v) {
	if (v < 0.0)
		return 0.0;
	if (v > 1.0)
		return 1.0;
	return v;
}

static double clamp_gain(double v) {
	if (v < GAIN_MIN)
		return GAIN_MIN;
	if (v > GAIN_MAX)
		return GAIN_MAX;
	return v;
}

static double quantize_gain(double v) {
	return floor(clamp_gain(v) * 100.0 + 0.5) / 100.0;
}

static unsigned char clamp_u8(double v) {
	if (v < 0.0)
		return 0;
	if (v > 255.0)
		return 255;
	return (unsigned char)(v + 0.5);
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

static double mix_gain(double gain, double strength) {
	return 1.0 + (gain - 1.0) * clamp_unit(strength);
}

static char *trim_token(char *s) {
	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
		s++;

	char *end = s + strlen(s);
	while (end > s) {
		char c = end[-1];
		if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
			break;
		*--end = '\0';
	}

	return s;
}

static int streq(const char *a, const char *b) {
	return strcmp(a, b) == 0;
}

static int valid_format(const char *format) {
	return streq(format, "rgb") ||
		streq(format, "bgr") ||
		streq(format, "grb") ||
		streq(format, "gbr") ||
		streq(format, "rbg") ||
		streq(format, "brg");
}

static uint32_t pack_rgb(int r, int g, int b, const char *format) {
	uint32_t rr = (uint32_t)r & 0xff;
	uint32_t gg = (uint32_t)g & 0xff;
	uint32_t bb = (uint32_t)b & 0xff;

	if (streq(format, "bgr"))
		return (bb << 16) | (gg << 8) | rr;
	if (streq(format, "grb"))
		return (gg << 16) | (rr << 8) | bb;
	if (streq(format, "gbr"))
		return (gg << 16) | (bb << 8) | rr;
	if (streq(format, "rbg"))
		return (rr << 16) | (bb << 8) | gg;
	if (streq(format, "brg"))
		return (bb << 16) | (rr << 8) | gg;

	return (rr << 16) | (gg << 8) | bb;
}

static void fill_linear_gain_table(uint32_t table[DISPLAYCAL_LUT_ENTRIES], const DisplayCalConfig *config) {
	double red_gain = mix_gain(config->red_gain, config->strength);
	double green_gain = mix_gain(config->green_gain, config->strength);
	double blue_gain = mix_gain(config->blue_gain, config->strength);

	for (int i = 0; i < DISPLAYCAL_LUT_ENTRIES; i++) {
		double linear = srgb_to_linear(i / 255.0);
		int r = clamp_u8(linear_to_srgb(linear * red_gain) * 255.0);
		int g = clamp_u8(linear_to_srgb(linear * green_gain) * 255.0);
		int b = clamp_u8(linear_to_srgb(linear * blue_gain) * 255.0);
		table[i] = pack_rgb(r, g, b, config->format);
	}
}

static void fill_identity_table(uint32_t table[DISPLAYCAL_LUT_ENTRIES]) {
	for (int i = 0; i < DISPLAYCAL_LUT_ENTRIES; i++)
		table[i] = ((uint32_t)i << 16) | ((uint32_t)i << 8) | (uint32_t)i;
}

static void normalize_config(DisplayCalConfig *config) {
	config->enabled = config->enabled ? 1 : 0;
	config->strength = clamp_unit(config->strength);
	config->red_gain = clamp_gain(config->red_gain);
	config->green_gain = clamp_gain(config->green_gain);
	config->blue_gain = clamp_gain(config->blue_gain);
	if (!valid_format(config->format))
		strcpy(config->format, "rgb");
}

static void load_config(DisplayCalConfig *config, const char *path) {
	if (!path || !*path)
		return;

	FILE *file = fopen(path, "r");
	if (!file)
		return;

	char line[160];
	while (fgets(line, sizeof(line), file)) {
		char *comment = strchr(line, '#');
		if (comment)
			*comment = '\0';

		char *sep = strchr(line, '=');
		if (!sep)
			continue;

		*sep = '\0';
		char *key = trim_token(line);
		char *value = trim_token(sep + 1);

		if (streq(key, "enabled"))
			config->enabled = atoi(value) ? 1 : 0;
		else if (streq(key, "screen"))
			config->screen = atoi(value);
		else if (streq(key, "strength"))
			config->strength = atof(value);
		else if (streq(key, "red") || streq(key, "r") || streq(key, "red_gain"))
			config->red_gain = atof(value);
		else if (streq(key, "green") || streq(key, "g") || streq(key, "green_gain"))
			config->green_gain = atof(value);
		else if (streq(key, "blue") || streq(key, "b") || streq(key, "blue_gain"))
			config->blue_gain = atof(value);
		else if (streq(key, "format") && valid_format(value))
			snprintf(config->format, sizeof(config->format), "%s", value);
	}

	fclose(file);
	normalize_config(config);
}

static int save_config(const DisplayCalConfig *config, const char *path) {
	FILE *file = fopen(path, "w");
	if (!file) {
		fprintf(stderr, "displaycal: open config for write failed: %s\n", strerror(errno));
		return -1;
	}

	fprintf(file, "enabled=%d\n", config->enabled ? 1 : 0);
	fprintf(file, "screen=%d\n", config->screen);
	fprintf(file, "strength=%.6f\n", config->strength);
	fprintf(file, "red=%.16f\n", config->red_gain);
	fprintf(file, "green=%.16f\n", config->green_gain);
	fprintf(file, "blue=%.16f\n", config->blue_gain);
	fprintf(file, "format=%s\n", config->format);
	fclose(file);
	return 0;
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

static int apply_table(int fd, int screen, uint32_t table[DISPLAYCAL_LUT_ENTRIES]) {
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

	unsigned long enable_param[4] = { (unsigned long)screen, 0, 0, 0 };
	if (ioctl(fd, DISP_LCD_GAMMA_CORRECTION_ENABLE, enable_param) < 0) {
		fprintf(stderr, "displaycal: enable gamma failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int apply_config(const DisplayCalConfig *config) {
	int fd = open_disp();
	if (fd < 0)
		return -1;

	int ret = 0;
	if (!config->enabled) {
		uint32_t table[DISPLAYCAL_LUT_ENTRIES];
		fill_identity_table(table);
		ret = apply_table(fd, config->screen, table);
	} else {
		uint32_t table[DISPLAYCAL_LUT_ENTRIES];
		fill_linear_gain_table(table, config);
		ret = apply_table(fd, config->screen, table);
	}

	close(fd);
	return ret;
}

static int apply_identity(int screen) {
	int fd = open_disp();
	if (fd < 0)
		return -1;

	uint32_t table[DISPLAYCAL_LUT_ENTRIES];
	fill_identity_table(table);
	int ret = apply_table(fd, screen, table);
	close(fd);
	return ret;
}

static int disable_screen(int screen) {
	int fd = open_disp();
	if (fd < 0)
		return -1;

	int ret = disable_gamma(fd, screen);
	close(fd);
	return ret;
}

static void print_status(const DisplayCalConfig *config) {
	printf("enabled=%d\n", config->enabled);
	printf("screen=%d\n", config->screen);
	printf("strength=%.6f\n", config->strength);
	printf("red=%.16f\n", config->red_gain);
	printf("green=%.16f\n", config->green_gain);
	printf("blue=%.16f\n", config->blue_gain);
	printf("format=%s\n", config->format);
}

static void draw_text(SDL_Surface *screen, TTF_Font *text_font, const char *text, SDL_Color color, int x, int y) {
	SDL_Surface *surface = TTF_RenderUTF8_Blended(text_font, text, color);
	if (!surface)
		return;
	SDL_BlitSurface(surface, NULL, screen, &(SDL_Rect){x, y, surface->w, surface->h});
	SDL_FreeSurface(surface);
}

static void draw_row(SDL_Surface *screen, int row, int selected, int active, const char *text, double slider_value, int has_slider) {
	int x = SCALE1(PADDING);
	int y = SCALE1(PADDING + PILL_SIZE * (row + 1));
	int w = screen->w - SCALE1(PADDING * 2);
	int h = SCALE1(PILL_SIZE);
	int asset = selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL;
	SDL_Color text_color = selected ? COLOR_BLACK : active ? COLOR_WHITE : COLOR_DARK_TEXT;

	GFX_blitPill(asset, screen, &(SDL_Rect){x, y, w, h});
	draw_text(screen, row == 0 ? font.large : font.medium, text, text_color, x + SCALE1(BUTTON_PADDING), y + SCALE1(4));

	if (!has_slider)
		return;

	int bar_w = w / 3;
	int bar_h = SCALE1(4);
	int bar_x = x + w - bar_w - SCALE1(BUTTON_PADDING);
	int bar_y = y + (h - bar_h) / 2;
	int fill_w = (int)((slider_value / GAIN_MAX) * bar_w);
	if (fill_w < 0)
		fill_w = 0;
	if (fill_w > bar_w)
		fill_w = bar_w;

	uint32_t track = selected ? RGB_LIGHT_GRAY : RGB_DARK_GRAY;
	uint32_t fill = selected ? RGB_BLACK : active ? RGB_WHITE : RGB_GRAY;
	SDL_FillRect(screen, &(SDL_Rect){bar_x, bar_y, bar_w, bar_h}, track);
	SDL_FillRect(screen, &(SDL_Rect){bar_x, bar_y, fill_w, bar_h}, fill);
}

static void draw_main_ui(SDL_Surface *screen, const DisplayCalConfig *config, int selected, int show_setting) {
	GFX_clear(screen);
	int ow = GFX_blitHardwareGroup(screen, show_setting);
	if (show_setting)
		GFX_blitHardwareHints(screen, show_setting);

	GFX_blitButtonGroup((char*[]){"B", "BACK", NULL}, 1, screen, 1);
	GFX_blitButtonGroup((char*[]){"A", "SELECT", NULL}, 0, screen, 0);

	int max_width = screen->w - SCALE1(PADDING * 2) - ow;
	char title[256];
	GFX_truncateText(font.large, "White Point", title, max_width, SCALE1(BUTTON_PADDING * 2));
	GFX_blitPill(ASSET_BLACK_PILL, screen, &(SDL_Rect){SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE)});
	draw_text(screen, font.large, title, COLOR_WHITE, SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + 4));

	char line[128];
	snprintf(line, sizeof(line), "Correction: %s", config->enabled ? "ON" : "OFF");
	draw_row(screen, 0, selected == 0, 1, line, 0.0, 0);

	snprintf(line, sizeof(line), "Red: %.2f", config->red_gain);
	draw_row(screen, 1, selected == 1, config->enabled, line, config->red_gain, 1);

	snprintf(line, sizeof(line), "Green: %.2f", config->green_gain);
	draw_row(screen, 2, selected == 2, config->enabled, line, config->green_gain, 1);

	snprintf(line, sizeof(line), "Blue: %.2f", config->blue_gain);
	draw_row(screen, 3, selected == 3, config->enabled, line, config->blue_gain, 1);

	draw_row(screen, 4, selected == 4, 1, "Reset to recommended", 0.0, 0);
	draw_row(screen, 5, selected == 5, 1, "Calibration patterns", 0.0, 0);

	GFX_flip(screen);
}

static void restore_saved_state(const DisplayCalConfig *saved) {
	apply_config(saved);
}

static void run_calibration(SDL_Surface *screen, const DisplayCalConfig *saved_config) {
	DisplayCalConfig live_config = *saved_config;
	int selected_pattern = 0;
	int dirty = 1;
	int temporary_enabled = saved_config->enabled ? 1 : 0;

	while (!quit) {
		GFX_startFrame();
		PAD_poll();
		PWR_update(&dirty, NULL, NULL, NULL);

		if (PAD_justPressed(BTN_LEFT) || PAD_justRepeated(BTN_LEFT)) {
			selected_pattern = (selected_pattern - 1 + (int)(sizeof(patterns) / sizeof(patterns[0]))) % (int)(sizeof(patterns) / sizeof(patterns[0]));
			dirty = 1;
		} else if (PAD_justPressed(BTN_RIGHT) || PAD_justRepeated(BTN_RIGHT)) {
			selected_pattern = (selected_pattern + 1) % (int)(sizeof(patterns) / sizeof(patterns[0]));
			dirty = 1;
		} else if (PAD_justPressed(BTN_A)) {
			temporary_enabled = !temporary_enabled;
			live_config.enabled = temporary_enabled;
			apply_config(&live_config);
		} else if (PAD_justPressed(BTN_B)) {
			restore_saved_state(saved_config);
			return;
		}

		if (dirty) {
			const Pattern *pattern = &patterns[selected_pattern];
			SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, pattern->r, pattern->g, pattern->b));
			GFX_flip(screen);
			dirty = 0;
		} else {
			GFX_sync();
		}
	}

	restore_saved_state(saved_config);
}

static void reset_recommended(DisplayCalConfig *config) {
	config->enabled = 1;
	config->strength = 1.0;
	config->red_gain = DEFAULT_RED_GAIN;
	config->green_gain = DEFAULT_GREEN_GAIN;
	config->blue_gain = DEFAULT_BLUE_GAIN;
}

static int run_ui(const char *config_path) {
	DisplayCalConfig config;
	init_config(&config);
	load_config(&config, config_path);
	save_config(&config, config_path);
	apply_config(&config);

	InitSettings();
	PWR_setCPUSpeed(CPU_SPEED_AUTO);

	SDL_Surface *screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();

	signal(SIGINT, sigHandler);
	signal(SIGTERM, sigHandler);

	int selected = 0;
	int dirty = 1;
	int show_setting = 0;
	int was_online = PWR_isOnline();
	int had_bt = PLAT_btIsConnected();

	while (!quit) {
		GFX_startFrame();
		PAD_poll();

		PWR_update(&dirty, &show_setting, NULL, NULL);

		int is_online = PWR_isOnline();
		if (was_online != is_online)
			dirty = 1;
		was_online = is_online;

		int has_bt = PLAT_btIsConnected();
		if (had_bt != has_bt)
			dirty = 1;
		had_bt = has_bt;

		if (PAD_justPressed(BTN_B)) {
			quit = true;
		} else if (PAD_justPressed(BTN_DOWN)) {
			selected = (selected + 1) % 6;
			dirty = 1;
		} else if (PAD_justPressed(BTN_UP)) {
			selected = (selected - 1 + 6) % 6;
			dirty = 1;
		} else if (PAD_justPressed(BTN_A) && selected == 0) {
			config.enabled = !config.enabled;
			save_config(&config, config_path);
			apply_config(&config);
			dirty = 1;
		} else if ((PAD_justPressed(BTN_LEFT) || PAD_justPressed(BTN_RIGHT)) && selected == 0) {
			config.enabled = !config.enabled;
			save_config(&config, config_path);
			apply_config(&config);
			dirty = 1;
		} else if ((PAD_justPressed(BTN_LEFT) || PAD_justPressed(BTN_RIGHT) || PAD_justRepeated(BTN_LEFT) || PAD_justRepeated(BTN_RIGHT)) && selected >= 1 && selected <= 3 && config.enabled) {
			double delta = (PAD_justPressed(BTN_RIGHT) || PAD_justRepeated(BTN_RIGHT)) ? GAIN_STEP : -GAIN_STEP;
			if (selected == 1)
				config.red_gain = quantize_gain(config.red_gain + delta);
			else if (selected == 2)
				config.green_gain = quantize_gain(config.green_gain + delta);
			else
				config.blue_gain = quantize_gain(config.blue_gain + delta);
			save_config(&config, config_path);
			apply_config(&config);
			dirty = 1;
		} else if (PAD_justPressed(BTN_A) && selected == 4) {
			reset_recommended(&config);
			save_config(&config, config_path);
			apply_config(&config);
			dirty = 1;
		} else if (PAD_justPressed(BTN_A) && selected == 5) {
			DisplayCalConfig saved_config = config;
			run_calibration(screen, &saved_config);
			config = saved_config;
			dirty = 1;
		}

		if (dirty) {
			draw_main_ui(screen, &config, selected, show_setting);
			dirty = 0;
		} else {
			GFX_sync();
		}
	}

	PWR_quit();
	PAD_quit();
	GFX_quit();
	QuitSettings();
	return 0;
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
		init_config(&config);
		if (argc >= 3)
			load_config(&config, argv[2]);
		return apply_config(&config) < 0 ? 1 : 0;
	}

	if (streq(argv[1], "status")) {
		DisplayCalConfig config;
		init_config(&config);
		if (argc >= 3)
			load_config(&config, argv[2]);
		print_status(&config);
		return 0;
	}

	if (streq(argv[1], "disable")) {
		int screen = argc >= 3 ? atoi(argv[2]) : 0;
		return disable_screen(screen) < 0 ? 1 : 0;
	}

	if (streq(argv[1], "identity")) {
		int screen = argc >= 3 ? atoi(argv[2]) : 0;
		return apply_identity(screen) < 0 ? 1 : 0;
	}

	if (streq(argv[1], "ui")) {
		const char *config_path = argc >= 3 ? argv[2] : USERDATA_PATH "/displaycal.cfg";
		return run_ui(config_path);
	}

	usage(argv[0]);
	return 2;
}
