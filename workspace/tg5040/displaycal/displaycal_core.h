#ifndef DISPLAYCAL_CORE_H
#define DISPLAYCAL_CORE_H

#define DEFAULT_RED_GAIN   1.0000000000000000
#define DEFAULT_GREEN_GAIN 0.9233642796405507
#define DEFAULT_BLUE_GAIN  0.5833412353395729

#define GAIN_MIN  0.0
#define GAIN_MAX  2.0
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

void displaycal_init_config(DisplayCalConfig *config);
double displaycal_quantize_gain(double v);
void displaycal_load_config(DisplayCalConfig *config, const char *path);
int displaycal_save_config(const DisplayCalConfig *config, const char *path);
int displaycal_apply_config(const DisplayCalConfig *config);
int displaycal_apply_identity(int screen);
int displaycal_disable_screen(int screen);
void displaycal_print_status(const DisplayCalConfig *config);

#endif
