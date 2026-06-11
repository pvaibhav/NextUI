#include "displaycal_ui.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <msettings.h>

#include "sdl.h"
#include "defines.h"
#include "api.h"
#include "displaycal_core.h"

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
	displaycal_apply_config(saved);
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
			displaycal_apply_config(&live_config);
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

int displaycal_run_ui(const char *config_path) {
	DisplayCalConfig config;
	displaycal_init_config(&config);
	displaycal_load_config(&config, config_path);
	displaycal_save_config(&config, config_path);
	displaycal_apply_config(&config);

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
			displaycal_save_config(&config, config_path);
			displaycal_apply_config(&config);
			dirty = 1;
		} else if ((PAD_justPressed(BTN_LEFT) || PAD_justPressed(BTN_RIGHT)) && selected == 0) {
			config.enabled = !config.enabled;
			displaycal_save_config(&config, config_path);
			displaycal_apply_config(&config);
			dirty = 1;
		} else if ((PAD_justPressed(BTN_LEFT) || PAD_justPressed(BTN_RIGHT) || PAD_justRepeated(BTN_LEFT) || PAD_justRepeated(BTN_RIGHT)) && selected >= 1 && selected <= 3 && config.enabled) {
			double delta = (PAD_justPressed(BTN_RIGHT) || PAD_justRepeated(BTN_RIGHT)) ? GAIN_STEP : -GAIN_STEP;
			if (selected == 1)
				config.red_gain = displaycal_quantize_gain(config.red_gain + delta);
			else if (selected == 2)
				config.green_gain = displaycal_quantize_gain(config.green_gain + delta);
			else
				config.blue_gain = displaycal_quantize_gain(config.blue_gain + delta);
			displaycal_save_config(&config, config_path);
			displaycal_apply_config(&config);
			dirty = 1;
		} else if (PAD_justPressed(BTN_A) && selected == 4) {
			reset_recommended(&config);
			displaycal_save_config(&config, config_path);
			displaycal_apply_config(&config);
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
