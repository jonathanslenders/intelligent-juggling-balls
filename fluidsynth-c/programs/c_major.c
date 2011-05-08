#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */

#include "../main.h"

// http://en.wikipedia.org/wiki/General_MIDI

#define NOTE_COUNT 14
int notes[] =
{
	C_MAJOR__G3,

	C_MAJOR__C4,
	C_MAJOR__D4,
	C_MAJOR__E4,
	C_MAJOR__F4,
	C_MAJOR__G4,
	C_MAJOR__A4,
	C_MAJOR__B_flat_4,
	C_MAJOR__B4,

	C_MAJOR__C5,
	C_MAJOR__D5,

	C_MAJOR__E5,
	C_MAJOR__F5,
	C_MAJOR__G5,
	C_MAJOR__A5,
	C_MAJOR__B5,
};

void c_major_test_thread(void* data);

void c_major_activate(void * data)
{
    // Initialize percussion -> always bank 128, and channel 10 (General MIDI)
//	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 1);
//	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 16); //organ
//	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 26); // jazz guitar
//	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 12); // Marimba
//	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 9); // Glockenspiel
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 2); // eletric piano


	// Send colors
	send_packet("RUN", 0, "fade", "000000:300");
	sleep(1);
	int i;
	for(i = 0; i < BALL_COUNT; i ++)
	{
		int pos = ( 255*i/BALL_COUNT);
		int r,g,b;
		int r2,g2,b2;
		interpolate(pos, &r, &g, &b);
		r2 = 0; // r / 8;
		g2 = 0; // g / 8;
		b2 = 0; // b / 8;
		char code[128];
		sprintf(code, "%02x%02x%02x_%02x%02x%02x_%02x%02x%02x", r2, g2, b2, r2, g2, b2, r, g, b);
		send_packet("RUN", i, "fixed", code);
		// TODO: insert delay in here for making sure all packets will arrive
	}

    // Test code for discovering instruments.
    if(0){
        pthread_t test_thread;
        pthread_create(&test_thread, NULL, (void *) &c_major_test_thread, (void *) NULL);
    }
}

void c_major_deactivate(void)
{

}

void c_major_packet_received(struct juggle_packet_t* packet)
{
//print_string("%s", packet->action);
	// Midi channel 10 is always reserved for percussion
	if (strcmp(packet->action, "CAUGHT") == 0 && packet->ball >= 1 && packet->ball <= NOTE_COUNT)
        fluid_synth_noteoff(synth, 0, notes[packet->ball-1]);

	if (strcmp(packet->action, "CAUGHT*") == 0 && packet->ball >= 1 && packet->ball <= NOTE_COUNT)
        fluid_synth_noteoff(synth, 0, notes[packet->ball-1]);

	if (strcmp(packet->action, "THROWN") == 0 && packet->ball >= 1 && packet->ball <= NOTE_COUNT)
//	if (strcmp(packet->action, "IN_FREE_FALL") == 0 && packet->ball >= 1 && packet->ball <= NOTE_COUNT)
        fluid_synth_noteon(synth, 0, notes[packet->ball-1], 100);
}


void c_major_test_thread(void* data)
{
    int i;
    for(i = 1; i < 255; i ++)
    {
        print_string("playing instrument %i", i);
	    fluid_synth_program_select(synth, 0, fluid_font_id, 0, i);
        fluid_synth_noteon(synth, 0, C_MAJOR__A4, 100);
        sleep(1);
        fluid_synth_noteoff(synth, 0, C_MAJOR__A4);
    }
}
