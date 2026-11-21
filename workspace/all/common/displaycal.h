#ifndef DISPLAYCAL_H
#define DISPLAYCAL_H

#include <stdint.h>

#define DISPLAYCAL_GAIN_SCALE 100
#define DISPLAYCAL_GAIN_MIN 0
#define DISPLAYCAL_GAIN_MAX 200
#define DISPLAYCAL_DEFAULT_RED_GAIN 100
#define DISPLAYCAL_DEFAULT_GREEN_GAIN 100
#define DISPLAYCAL_DEFAULT_BLUE_GAIN 100

// Fixed kernel gamma-table size. Only .cube files with exactly this many
// 1D entries are accepted; anything else is rejected, never resampled.
#define DISPLAYCAL_LUT_ENTRIES 256

// Colour correction modes. OFF=0/ON=1 preserve the values of the old
// displaycal_enabled flag so persisted pre-mode settings migrate directly.
enum DisplayCalMode {
	DISPLAYCAL_MODE_OFF = 0,
	DISPLAYCAL_MODE_ON = 1,
	DISPLAYCAL_MODE_CUSTOM = 2
};

#define DISPLAYCAL_DEFAULT_MODE DISPLAYCAL_MODE_OFF

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DisplayCalDefaults {
	int mode;
	int red_gain;
	int green_gain;
	int blue_gain;
} DisplayCalDefaults;

// TrimUI Brick, measured with X-Rite i1Display Pro, calibrated to sRGB D65 2.2
static const struct DisplayCalDefaults DisplayCalDefaults_Brick = { DISPLAYCAL_MODE_ON, 100, 92, 58 };

// TrimUI Smart Pro, measured with Spyder 5 Pro and slightly modified by eye to match the Brick
static const struct DisplayCalDefaults DisplayCalDefaults_SmartPro = { DISPLAYCAL_MODE_ON, 100, 77, 50 };

// TrimUI Brick Pro, bundled brickpro.cube
static const struct DisplayCalDefaults DisplayCalDefaults_BrickPro = { DISPLAYCAL_MODE_ON, 100, 94, 54 };

// Anbernic H700 family. Uncalibrated models remain disabled with neutral gains.
static const struct DisplayCalDefaults DisplayCalDefaults_RG28XX = { DISPLAYCAL_MODE_ON, 100, 92, 65 };
static const struct DisplayCalDefaults DisplayCalDefaults_RG34XX = { DISPLAYCAL_MODE_OFF, 100, 100, 100 };
static const struct DisplayCalDefaults DisplayCalDefaults_RG34XXSP = { DISPLAYCAL_MODE_ON, 100, 83, 86 };
static const struct DisplayCalDefaults DisplayCalDefaults_RGSP = { DISPLAYCAL_MODE_ON, 100, 68, 61 };
static const struct DisplayCalDefaults DisplayCalDefaults_RG35XXPlus = { DISPLAYCAL_MODE_OFF, 100, 100, 100 };
static const struct DisplayCalDefaults DisplayCalDefaults_RG35XXH = { DISPLAYCAL_MODE_OFF, 100, 100, 100 };
static const struct DisplayCalDefaults DisplayCalDefaults_RG35XXSP = { DISPLAYCAL_MODE_OFF, 100, 100, 100 };
static const struct DisplayCalDefaults DisplayCalDefaults_RG35XXPRO = { DISPLAYCAL_MODE_OFF, 100, 100, 100 };
static const struct DisplayCalDefaults DisplayCalDefaults_RG40XXH = { DISPLAYCAL_MODE_OFF, 100, 100, 100 };
static const struct DisplayCalDefaults DisplayCalDefaults_RG40XXV = { DISPLAYCAL_MODE_ON, 91, 100, 63 };
static const struct DisplayCalDefaults DisplayCalDefaults_RGCubeXX = { DISPLAYCAL_MODE_OFF, 100, 100, 100 };

enum DisplayCalPreset {
	DISPLAYCAL_PRESET_DEFAULT = 0,
	DISPLAYCAL_PRESET_BRICK,
	DISPLAYCAL_PRESET_SMARTPRO,
	DISPLAYCAL_PRESET_BRICKPRO,
	DISPLAYCAL_PRESET_RG28XX,
	DISPLAYCAL_PRESET_RG34XX,
	DISPLAYCAL_PRESET_RG34XXSP,
	DISPLAYCAL_PRESET_RGSP,
	DISPLAYCAL_PRESET_RG35XXPLUS,
	DISPLAYCAL_PRESET_RG35XXH,
	DISPLAYCAL_PRESET_RG35XXSP,
	DISPLAYCAL_PRESET_RG35XXPRO,
	DISPLAYCAL_PRESET_RG40XXH,
	DISPLAYCAL_PRESET_RG40XXV,
	DISPLAYCAL_PRESET_RGCUBEXX
};

static inline DisplayCalDefaults DisplayCal_getDefaultSettings(enum DisplayCalPreset preset) {
	switch(preset) {
		case DISPLAYCAL_PRESET_SMARTPRO: return DisplayCalDefaults_SmartPro;
		case DISPLAYCAL_PRESET_BRICK: return DisplayCalDefaults_Brick;
		case DISPLAYCAL_PRESET_BRICKPRO: return DisplayCalDefaults_BrickPro;
		case DISPLAYCAL_PRESET_RG28XX: return DisplayCalDefaults_RG28XX;
		case DISPLAYCAL_PRESET_RG34XX: return DisplayCalDefaults_RG34XX;
		case DISPLAYCAL_PRESET_RG34XXSP: return DisplayCalDefaults_RG34XXSP;
		case DISPLAYCAL_PRESET_RGSP: return DisplayCalDefaults_RGSP;
		case DISPLAYCAL_PRESET_RG35XXPLUS: return DisplayCalDefaults_RG35XXPlus;
		case DISPLAYCAL_PRESET_RG35XXH: return DisplayCalDefaults_RG35XXH;
		case DISPLAYCAL_PRESET_RG35XXSP: return DisplayCalDefaults_RG35XXSP;
		case DISPLAYCAL_PRESET_RG35XXPRO: return DisplayCalDefaults_RG35XXPRO;
		case DISPLAYCAL_PRESET_RG40XXH: return DisplayCalDefaults_RG40XXH;
		case DISPLAYCAL_PRESET_RG40XXV: return DisplayCalDefaults_RG40XXV;
		case DISPLAYCAL_PRESET_RGCUBEXX: return DisplayCalDefaults_RGCubeXX;
		default: break;
	}
	// Default preset
	DisplayCalDefaults defaults = {
		DISPLAYCAL_DEFAULT_MODE,
		DISPLAYCAL_DEFAULT_RED_GAIN,
		DISPLAYCAL_DEFAULT_GREEN_GAIN,
		DISPLAYCAL_DEFAULT_BLUE_GAIN,
	};
	return defaults;
}

// Clamp a display calibration gain value to the supported 0-200 range.
static inline int DisplayCal_clampGainValue(int value) {
	if (value < DISPLAYCAL_GAIN_MIN)
		return DISPLAYCAL_GAIN_MIN;
	if (value > DISPLAYCAL_GAIN_MAX)
		return DISPLAYCAL_GAIN_MAX;
	return value;
}

// Coerce a persisted colour correction mode to a valid value. Unknown values
// (e.g. from a damaged settings file) safely become OFF.
static inline int DisplayCal_clampModeValue(int value) {
	if (value < DISPLAYCAL_MODE_OFF || value > DISPLAYCAL_MODE_CUSTOM)
		return DISPLAYCAL_MODE_OFF;
	return value;
}

// Parse a 1D .cube LUT file into a kernel gamma table. Strict: the file must
// declare LUT_1D_SIZE 256 with exactly 256 data rows and a missing or
// identity domain, otherwise the parse fails and the table is untouched.
// Returns 0 on success, -1 on failure.
int DisplayCal_parseCubeFile(const char *path, uint32_t table[DISPLAYCAL_LUT_ENTRIES]);

// Parse the .cube file at path and, only if it validates, apply it as the
// gamma table. Returns 0 on success, -1 on failure (LUT left untouched).
int DisplayCal_enableWithLutFile(const char *path);

// Apply the bundled device-specific .cube file resolved from the SDCARD_PATH
// and DEVICE environment ($SDCARD_PATH/.system/res/displaycal/$DEVICE.cube).
// Returns 0 on success, -1 when the file is missing or invalid.
int DisplayCal_enableWithDeviceFile(void);

// Apply the LUT using integer red, green, and blue gains in the 0-200 range.
// A value of 100 is neutral.
int DisplayCal_enableWithValues(int red_gain, int green_gain, int blue_gain);

// Load the identity LUT, then disable gamma correction.
int DisplayCal_disable(void);

#ifdef __cplusplus
}
#endif

#endif
