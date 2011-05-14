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

void the_end_activate(void * data)
{
    // Initialize percussion -> always bank 128, and channel 10 (General MIDI)
	fluid_synth_program_select(synth, 10, fluid_font_id, 128, 0);


	// send_packet("RUN", 0, "pulse", "FFFFFF_000000:400");

	//randomize(); // Random start

	int i;
	for (i = 0; i < 80; i ++)
	{
   		int wait = my_random(100);

		print_string("wait %i", wait);
		usleep(wait * 1000);

		// Light ball
		int ball = 1 + my_random(13);
		send_packet("RUN", ball, "pulse", "FFFFFF_000000:80");

		// Play sound
   		int sound = my_random(13);  /* n is random number in range of 0 - 99 */
		if (sound == 1)
        	fluid_synth_noteon(synth, 10, sounds[0], 100);
		else if (sound == 2)
        	fluid_synth_noteon(synth, 10, sounds[1], 100);
		else
        	fluid_synth_noteon(synth, 10, sounds[2], 100);
	}

	for (i = 0; i < 3; i ++)
	{
		usleep(400 * 1000);
		fluid_synth_noteon(synth, 10, sounds[1], 100);
		send_packet("RUN", 0, "fade", "FFFFFF_000000:80");
	}
	send_packet("RUN", 0, "fixed", "000000");
}
