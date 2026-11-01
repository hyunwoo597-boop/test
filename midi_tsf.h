#ifndef MIDI_TSF_H__
#define MIDI_TSF_H__
#include <stdint.h>
struct MidiPlayer;
MidiPlayer *Midi_open(const uint8_t *data, int size, int rate);
void Midi_close(MidiPlayer *p);
int Midi_render(MidiPlayer *p, int16_t *stereo, int frames);
#endif
