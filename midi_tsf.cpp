/*
 * MIDI music playback with TinySoundFont (used when no MUSIC/trackNN.ogg exists).
 * The soundfont is loaded once from bermuda.sf2 (working directory, then romfs:/).
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#define TSF_IMPLEMENTATION
#include "tsf.h"
#define TML_IMPLEMENTATION
#include "tml.h"
#include "midi_tsf.h"

static tsf *_baseFont = 0;
static bool _fontTried = false;

static tsf *loadFont() {
	if (!_fontTried) {
		_fontTried = true;
		static const char *paths[] = { "bermuda.sf2", "romfs:/bermuda.sf2", 0 };
		for (int i = 0; paths[i] && !_baseFont; ++i) {
			_baseFont = tsf_load_filename(paths[i]);
			if (_baseFont) {
				fprintf(stderr, "MIDI: soundfont '%s' loaded\n", paths[i]);
			}
		}
		if (!_baseFont) {
			fprintf(stderr, "MIDI: no soundfont (bermuda.sf2), music disabled\n");
		}
	}
	return _baseFont;
}

struct MidiPlayer {
	tsf *f;
	tml_message *first;
	tml_message *cur;
	double msec;
	double msecPerFrame;
	int16_t tmp[64 * 2];
};

static void resetChannels(tsf *f) {
	tsf_reset(f);
	for (int ch = 0; ch < 16; ++ch) {
		tsf_channel_set_presetnumber(f, ch, 0, ch == 9);
	}
}

MidiPlayer *Midi_open(const uint8_t *data, int size, int rate) {
	tsf *base = loadFont();
	if (!base) {
		return 0;
	}
	tml_message *msg = tml_load_memory(data, size);
	if (!msg) {
		return 0;
	}
	MidiPlayer *p = (MidiPlayer *)calloc(1, sizeof(MidiPlayer));
	p->f = tsf_copy(base);
	tsf_set_output(p->f, TSF_STEREO_INTERLEAVED, rate, -10.0f);
	tsf_set_max_voices(p->f, 32);
	resetChannels(p->f);
	p->first = p->cur = msg;
	p->msec = 0;
	p->msecPerFrame = 1000.0 / rate;
	return p;
}

void Midi_close(MidiPlayer *p) {
	if (p) {
		tml_free(p->first);
		tsf_close(p->f);
		free(p);
	}
}

static void process(MidiPlayer *p) {
	tsf *f = p->f;
	while (p->cur && p->msec >= p->cur->time) {
		tml_message *m = p->cur;
		switch (m->type) {
		case TML_PROGRAM_CHANGE:
			tsf_channel_set_presetnumber(f, m->channel, m->program, m->channel == 9);
			break;
		case TML_NOTE_ON:
			tsf_channel_note_on(f, m->channel, m->key, m->velocity / 127.0f);
			break;
		case TML_NOTE_OFF:
			tsf_channel_note_off(f, m->channel, m->key);
			break;
		case TML_PITCH_BEND:
			tsf_channel_set_pitchwheel(f, m->channel, m->pitch_bend);
			break;
		case TML_CONTROL_CHANGE:
			tsf_channel_midi_control(f, m->channel, m->control, m->control_value);
			break;
		}
		p->cur = m->next;
	}
	if (!p->cur) {
		// loop the song
		tsf_note_off_all(f);
		p->cur = p->first;
		p->msec = 0;
	}
}

int Midi_render(MidiPlayer *p, int16_t *out, int frames) {
	int done = 0;
	while (done < frames) {
		int n = frames - done;
		if (n > 64) {
			n = 64;
		}
		process(p);
		tsf_render_short(p->f, out + done * 2, n, 0);
		p->msec += n * p->msecPerFrame;
		done += n;
	}
	return done;
}
