
#include "../main.h"

// http://en.wikipedia.org/wiki/General_MIDI

#define PERCUSSION_INSTRUMENT_COUNT 13
int percussion_instruments[] =
{

	35, // bass drum
	37, // side kick
	40, // snare drum
	41, // Low Tom 2
	47, // mid tom
	54, // Tambourine
	58, // Vibra Slap
	61, // Low Bongo
	64, // Low Conga
	69, // Cabasa
	72, // Long Whistle
	75, // Claves
	81, // Open triangle
};

void percussion_activate(void)
{
    // Initialize percussion -> always bank 128, and channel 10 (General MIDI)
	fluid_synth_program_select(synth, 10, fluid_font_id, 128, 0);
}

void percussion_deactivate(void)
{

}

void percussion_packet_received(struct juggle_packet_t* packet)
{
	// Midi channel 10 is always reserved for percussion
	if (strcmp(packet->action, "CAUGHT") == 0 && packet->ball >= 1 && packet->ball <= PERCUSSION_INSTRUMENT_COUNT)
        fluid_synth_noteoff(synth, 10, percussion_instruments[packet->ball-1]);

	if (strcmp(packet->action, "CAUGHT*") == 0 && packet->ball >= 1 && packet->ball <= PERCUSSION_INSTRUMENT_COUNT)
        fluid_synth_noteoff(synth, 10, percussion_instruments[packet->ball-1]);

	if (strcmp(packet->action, "THROWN") == 0 && packet->ball >= 1 && packet->ball <= PERCUSSION_INSTRUMENT_COUNT)
        fluid_synth_noteon(synth, 10, percussion_instruments[packet->ball-1], 100);

}
