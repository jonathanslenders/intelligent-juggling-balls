#include "../main.h"

// http://en.wikipedia.org/wiki/General_MIDI

#define NOTE_COUNT 14
int notes[] =
{
	G_MAJOR__C4,
	G_MAJOR__D4,
	G_MAJOR__E4,
	G_MAJOR__F4,
	G_MAJOR__G4,
	G_MAJOR__A4,
	G_MAJOR__B4,

	G_MAJOR__C5,
	G_MAJOR__D5,
	G_MAJOR__E5,
	G_MAJOR__F5,
	G_MAJOR__G5,
	G_MAJOR__A5,
	G_MAJOR__B5,
};

void c_major_activate(void)
{
    // Initialize percussion -> always bank 128, and channel 10 (General MIDI)
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 1);
}

void c_majar_deactivate(void)
{

}

void c_major_packet_received(struct juggle_packet_t* packet)
{
	// Midi channel 10 is always reserved for percussion
	if (strcmp(packet->action, "CAUGHT") == 0 && packet->ball >= 1 && packet->ball <= NOTE_COUNT)
        fluid_synth_noteoff(synth, 0, notes[packet->ball-1]);

	if (strcmp(packet->action, "CAUGHT*") == 0 && packet->ball >= 1 && packet->ball <= NOTE_COUNT)
        fluid_synth_noteoff(synth, 0, notes[packet->ball-1]);

	if (strcmp(packet->action, "THROWN") == 0 && packet->ball >= 1 && packet->ball <= NOTE_COUNT)
        fluid_synth_noteon(synth, 0, notes[packet->ball-1], 100);

}
