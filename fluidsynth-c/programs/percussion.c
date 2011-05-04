#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */

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

void percussion_test_thread(void*);

void percussion_activate(void)
{
    // Initialize percussion -> always bank 128, and channel 10 (General MIDI)
	fluid_synth_program_select(synth, 10, fluid_font_id, 128, 0);

	// Default program
	send_packet("RUN", 0, "fixed", "004400 880088 ff0000 0000ff ffffff");

    // Test code for discovering instruments.
    if(0){
        pthread_t test_thread;
        pthread_create(&test_thread, NULL, (void *) &percussion_test_thread, (void *) NULL);
    }
}

void percussion_deactivate(void)
{

}

void percussion_packet_received(struct juggle_packet_t* packet)
{
	// Midi channel 10 is always reserved for percussion
//	if (strcmp(packet->action, "CAUGHT") == 0 && packet->ball >= 1 && packet->ball <= PERCUSSION_INSTRUMENT_COUNT)
//        fluid_synth_noteoff(synth, 10, percussion_instruments[packet->ball-1]);
//
//	if (strcmp(packet->action, "CAUGHT*") == 0 && packet->ball >= 1 && packet->ball <= PERCUSSION_INSTRUMENT_COUNT)
//        fluid_synth_noteoff(synth, 10, percussion_instruments[packet->ball-1]);

//	if (strcmp(packet->action, "THROWN") == 0 && packet->ball >= 1 && packet->ball <= PERCUSSION_INSTRUMENT_COUNT)
	if (strcmp(packet->action, "CAUGHT") == 0 && packet->ball >= 1 && packet->ball <= PERCUSSION_INSTRUMENT_COUNT)
//	if (strcmp(packet->action, "MOVING") == 0 && packet->ball >= 1 && packet->ball <= PERCUSSION_INSTRUMENT_COUNT)
        fluid_synth_noteon(synth, 10, percussion_instruments[packet->ball-1], 100);

}

void percussion_test_thread(void* data)
{
    int i;
    for(i = 1; i < 255; i ++)
    {
        print_string("%i", i);
        fluid_synth_noteon(synth, 10, i+30, 100);
        sleep(1);
    }
}
