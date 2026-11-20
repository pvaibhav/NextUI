#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <msettings.h>
#include "ma_internal.h"
#include "ma_audio.h"

static bool resetAudio = false;

void Audio_onSinkChanged(int device, int watch_event) {
	switch (watch_event) {
	case DIRWATCH_CREATE:      LOG_info("callback reason: DIRWATCH_CREATE\n");      break;
	case DIRWATCH_DELETE:      LOG_info("callback reason: DIRWATCH_DELETE\n");      break;
	case FILEWATCH_MODIFY:     LOG_info("callback reason: FILEWATCH_MODIFY\n");     break;
	case FILEWATCH_DELETE:     LOG_info("callback reason: FILEWATCH_DELETE\n");     break;
	case FILEWATCH_CLOSE_WRITE:LOG_info("callback reason: FILEWATCH_CLOSE_WRITE\n");break;
	}

	resetAudio = true;

	// FIXME: This shouldnt be necessary, alsa should just read .asoundrc for the changed default device.
	if (device == AUDIO_SINK_BLUETOOTH)
		SDL_setenv("AUDIODEV", "bluealsa", 1);
	else
		SDL_setenv("AUDIODEV", "default", 1);
}

void Audio_checkAndResetIfNeeded(void) {
	if (!resetAudio) return;
	resetAudio = false;
	LOG_info("Resetting audio device config! (new state: %s)\n", SDL_getenv("AUDIODEV"));
	SND_resetAudio(core.sample_rate, core.fps);
}

#define PENDING_FRAMES 4096
static SND_Frame pending[PENDING_FRAMES];
static size_t pending_count = 0;

void Audio_flush(void) {
	if (!pending_count) return;
	if (use_core_fps || fast_forward) SND_batchSamples_fixed_rate(pending, pending_count);
	else SND_batchSamples(pending, pending_count);
	pending_count = 0;
}

void Audio_discard(void) {
	pending_count = 0;
}

static bool audio_dropped(void) {
	return (rewinding && !rewind_ctx.audio) || (fast_forward && !ff_audio);
}

void audio_sample_callback(int16_t left, int16_t right) {
	if (audio_dropped()) return;
	if (pending_count == PENDING_FRAMES) Audio_flush();
	pending[pending_count++] = (SND_Frame){left,right};
}

size_t audio_sample_batch_callback(const int16_t *data, size_t frames) {
	if (audio_dropped()) return frames;
	const SND_Frame *in = (const SND_Frame*)data;
	size_t left = frames;
	while (left > 0) {
		if (pending_count == PENDING_FRAMES) Audio_flush();
		size_t n = PENDING_FRAMES - pending_count;
		if (n > left) n = left;
		memcpy(&pending[pending_count], in, n * sizeof(SND_Frame));
		pending_count += n;
		in += n;
		left -= n;
	}
	return frames;
}
