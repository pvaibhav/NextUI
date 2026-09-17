/* RG SP vendor-kernel workaround. Its sunxi_spk_event is a no-op.
 * Only touch the verified active-high PI5 amplifier pin; other hardware keeps
 * the normal mixer path. No pin mux, pull, drive, or regulator changes.
 */
#include <time.h>

#ifndef SPEAKER_AMP_DT
#define SPEAKER_AMP_DT "/sys/firmware/devicetree/base/soc@03000000/codec@0x05096000/"
#endif
#ifndef SPEAKER_AMP_GPIO
#define SPEAKER_AMP_GPIO "/sys/kernel/debug/gpio"
#endif
#ifndef SPEAKER_AMP_DATA
#define SPEAKER_AMP_DATA "/sys/kernel/debug/sunxi_pinctrl/data"
#endif

static int speakerAmpProperty(const char *name, unsigned char *data, size_t size) {
    char path[256];
    snprintf(path, sizeof(path), "%s%s", SPEAKER_AMP_DT, name);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int ok = fread(data, 1, size, f) == size && fgetc(f) == EOF;
    fclose(f);
    return ok;
}

static int speakerAmpSupported(void) {
    const char *device = getenv("DEVICE");
    unsigned char pin[28], level[4], count[4];
    static const unsigned char expected_pin[] = {
        0,0,0,8, 0,0,0,5, 0,0,0,1
    };
    static const unsigned char one[] = {0,0,0,1};
    return device && !strcmp(device, "rgsp") &&
        speakerAmpProperty("pa-pin-0", pin, sizeof(pin)) &&
        !memcmp(pin + 4, expected_pin, sizeof(expected_pin)) &&
        speakerAmpProperty("pa-pin-level-0", level, sizeof(level)) &&
        !memcmp(level, one, sizeof(one)) &&
        speakerAmpProperty("pa-pin-max", count, sizeof(count)) &&
        !memcmp(count, one, sizeof(one));
}

static int speakerAmpState(void) {
    FILE *f = fopen(SPEAKER_AMP_GPIO, "r");
    if (!f) return -1;
    char line[256];
    int result = -1;
    while (fgets(line, sizeof(line), f)) {
        unsigned int gpio;
        if (sscanf(line, " gpio-%u", &gpio) == 1 && gpio == 261) {
            if (strstr(line, "out hi")) result = 1;
            else if (strstr(line, "out lo")) result = 0;
            break;
        }
    }
    fclose(f);
    return result;
}

static int speakerAmpSet(int enabled) {
    int previous = speakerAmpState();
    if (previous < 0) return -1;
    if (previous == enabled) return 0;
    /* Let the codec settle before connecting the amplifier. */
    if (enabled) usleep(100000);
    FILE *f = fopen(SPEAKER_AMP_DATA, "w");
    if (!f) return -1;
    int ok = fprintf(f, "PI5 %d\n", !!enabled) > 0;
    if (fclose(f)) ok = 0;
    if (!ok || speakerAmpState() != enabled) return -1;
    /* Let amplifier shutdown settle before changing the analog output. */
    if (!enabled) usleep(100000);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    fprintf(stderr, "AUDIO_AMP %ld.%06ld pid=%d enabled=%d\n",
            (long)ts.tv_sec, ts.tv_nsec / 1000, getpid(), enabled);
    return 0;
}
