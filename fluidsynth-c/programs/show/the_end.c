#include <time.h> /* for clock() */
#include <stdlib.h> /* required for randomize() and random() */


#include "../../main.h"


int sounds[] = {
	11,
	21,
	36,

	12,
	13,
	22,
	23,
	33,
	34,
	35,
	36,
	10,
	9,
};

int my_random(int max)
{
	return rand() / (RAND_MAX / max + 1);
}

void the_end_thread(void* data);

void the_end_activate(void * data)
{
	pthread_t thread;
	pthread_create(&thread, NULL, (void *) &the_end_thread, (void *) NULL);

    // Initialize percussion -> always bank 128, and channel 10 (General MIDI)
	fluid_synth_program_select(synth, 10, fluid_font_id, 128, 0);
	fluid_synth_program_select(synth, 1, fluid_font_id, 0, 127); // gunshot
	fluid_synth_program_select(synth, 2, fluid_font_id, 0, 1); // piano
	fluid_synth_cc(synth, 10, 7, 127); /* 7=volume, between 0 and 127 */
	fluid_synth_cc(synth, 1, 7, 127); /* 7=volume, between 0 and 127 */
	fluid_synth_cc(synth, 2, 7, 127); /* 7=volume, between 0 and 127 */

}

void the_end_thread(void* data)
{
	// send_packet("RUN", 0, "pulse", "FFFFFF_000000:400");

	//randomize(); // Random start

	// lights off
	send_packet("RUN", 0, "fade", "000000:200");


	int i;
	for (i = 0; i < 35; i ++)
	{
   		int wait = my_random(100);

		print_string("wait %i", wait);
		usleep(wait * 1000);

		// Light ball
		int ball = 1 + my_random(13);
		send_packet("RUN", ball, "pulse", "FFFFFF_000000:80");

		// Random pan
		int pan = my_random(127);
		fluid_synth_cc(synth, 10, 10, pan); /* 10=pan, between 0 and 127 */
		fluid_synth_cc(synth, 1, 10, pan); /* 10=pan, between 0 and 127 */

		// Play sound
   		int sound = my_random(13);  /* n is random number in range of 0 - 99 */
		if (sound == 1)
        	fluid_synth_noteon(synth, 10, sounds[0], 100);
		else if (sound == 2)
        	fluid_synth_noteon(synth, 10, sounds[1], 100);
		else
        	fluid_synth_noteon(synth, 10, sounds[2], 100);

		fluid_synth_noteon(synth, 1, 60, 100);
	}

	for (i = 0; i < 3; i ++)
	{
		usleep(400 * 1000);
		fluid_synth_noteon(synth, 10, sounds[1], 100);
		send_packet("RUN", 0, "pulse", "FFFFFF_000000:80");
	}
//	send_packet("RUN", 0, "fixed", "000000");

	// fade in and the end sound
	usleep(400*1000);

#define END_NOTE_1 71 // G
#define END_NOTE_2 69 // A
#define END_NOTE_3 67 // B
	send_packet("RUN", 0, "fade", "ffffff:200");
	send_packet("RUN", 0, "fade", "000000:1000");
return;

	fluid_synth_noteon(synth, 2, END_NOTE_1, 100);
	usleep(800*1000);
	fluid_synth_noteoff(synth, 2, END_NOTE_1);
	fluid_synth_noteon(synth, 2, END_NOTE_2, 100);
	usleep(800*1000);
	fluid_synth_noteoff(synth, 2, END_NOTE_2);
	fluid_synth_noteon(synth, 2, END_NOTE_3, 100);
}
