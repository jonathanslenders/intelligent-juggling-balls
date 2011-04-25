#include <stdlib.h> /* for malloc, free, exit */

#include "../main.h"
#include "charriots_of_fire.h"
#include "../sound_utils.h"


// http://en.wikipedia.org/wiki/General_MIDI

/* === Music definitions === */


// Violin
struct theme_note_t intro_melody[] =
{
		{ D_FLAT__Db4, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__Db4, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__Db4, },
		{ D_FLAT__Ab4, },
};


// Violin / Piano
#define MAIN_THEME_A_COUNT 22
struct theme_status_t main_theme_a_status =
{
	false,
	0,
};

struct theme_note_t main_theme_a[] = 
{
		{ D_FLAT__Db4, },
		{ D_FLAT__Gb4, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__Bb4, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__F4, },

		{ D_FLAT__Db4, },
		{ D_FLAT__Gb4, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__Bb4, },
		{ D_FLAT__Ab4, },

		{ D_FLAT__Db4, },
		{ D_FLAT__Gb4, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__Bb4, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__F4, },

		{ D_FLAT__F4, },
		{ D_FLAT__Gb4, },
		{ D_FLAT__F4, },
		{ D_FLAT__Db4, },
		{ D_FLAT__Db4, },
};

// Piano
#define MAIN_THEME_B_COUNT 30
struct theme_status_t main_theme_b_status =
{
	false,
	0,
};
struct theme_note_t main_theme_b[] = 
{
		{ D_FLAT__Db5, },
		{ D_FLAT__C5, },
		{ D_FLAT__Bb4, },
		{ D_FLAT__Ab4, },

		{ D_FLAT__C5, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__Bb4, },
		{ D_FLAT__Gb4, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__Db5, },

		{ D_FLAT__C5, },
		{ D_FLAT__Bb4, },
		{ D_FLAT__Ab4, },

		{ D_FLAT__C5, },
		{ D_FLAT__Gb4, },
		{ D_FLAT__F4, },

		{ D_FLAT__Db5, },
		{ D_FLAT__C5, },
		{ D_FLAT__Bb4, },
		{ D_FLAT__Ab4, },

		{ D_FLAT__C5, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__Bb4, },
		{ D_FLAT__Gb4, },
		{ D_FLAT__Ab4, },
		{ D_FLAT__F4, },

		{ D_FLAT__Gb4, },
		{ D_FLAT__F4, },
		{ D_FLAT__Db4, },
		{ D_FLAT__Db4, },


		// ...

};


/* ==== Program === */



void charriots_activate(void)
{
    // Load the instruments at the right channel
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 24);
	fluid_synth_program_select(synth, 1, fluid_font_id, 0, 42);
	fluid_synth_program_select(synth, 2, fluid_font_id, 0, 0);
	fluid_synth_program_select(synth, 3, fluid_font_id, 0, 18); // accordeon/organ

	// Channel options
	fluid_synth_cc(synth, 0, 10, 120); /* 10=pan, between 0 and 127 */
	fluid_synth_cc(synth, 0, 7, 50); /* 7=volume, between 0 and 127 */
	fluid_synth_cc(synth, 0, 64, 30); /* 64=sustain */
	//fluid_synth_cc(synth, 0, 91, 100); /* 91=reverb */

	fluid_synth_cc(synth, 1, 10, 60); /* 10=pan, between 0 and 127 */
	fluid_synth_cc(synth, 1, 7, 90); /* 7=volume, between 0 and 127 */


	fluid_synth_cc(synth, 2, 10, 20); /* 10=pan, between 0 and 127 */
	fluid_synth_cc(synth, 2, 7, 90); /* 7=volume, between 0 and 127 */
	// Initialize positions
	main_theme_a_status.playing = false;
	main_theme_a_status.position = 0;
	main_theme_b_status.playing = false;
	main_theme_b_status.position = 0;

	// Activate colors
	send_packet("RUN", 0, "fixed", "000000"); // Turn everything off, to begin with

	send_packet("RUN", 1, "fixed", "110011");
	send_packet("RUN", 2, "fixed", "110011");
	send_packet("RUN", 3, "fixed", "110011");

	send_packet("RUN", 4, "fixed", "004400");
	send_packet("RUN", 5, "fixed", "003410");

	send_packet("RUN", 6, "fixed", "000044");
	send_packet("RUN", 7, "fixed", "000044");
	send_packet("RUN", 8, "fixed", "000044");

	send_packet("RUN", 9, "fixed", "111111");
	send_packet("RUN", 10, "fixed", "111111");
	send_packet("RUN", 11, "fixed", "111111");
}

void charriots_deactivate(void)
{
	// Mute all channels
	fluid_synth_cc(synth, 0, 123, 0);
	fluid_synth_cc(synth, 1, 123, 0);
	fluid_synth_cc(synth, 2, 123, 0);
	fluid_synth_cc(synth, 3, 123, 0);
}

void charriots_packet_received(struct juggle_packet_t* packet)
{
	print_string("Packet received: %i %s %s %s\n", packet->ball, packet->action, packet->param1, packet->param2);

    // Ball 1-3 caught: guitar strings for intro
	if (strcmp(packet->action, "CAUGHT") == 0 && packet->ball >= 1 && packet->ball <= 3)
    {
		// 'Reboot' note (off and on again.)
        fluid_synth_noteoff(synth, 0, D_FLAT__Db3);
        fluid_synth_noteon(synth, 0, D_FLAT__Db3, 100);
    }

    // Ball 4-5 Play the intro
	one_ball_engine(packet, 4, 1, D_FLAT__Db4);
	one_ball_engine(packet, 5, 1, D_FLAT__Ab4);

	// Ball 6-8: main theme
//	three_ball_engine(packet, 6, 7, 8, 2, main_theme_a, &main_theme_a_status, MAIN_THEME_A_COUNT);

on_catch_engine(packet, 6, 2, main_theme_a, &main_theme_a_status, MAIN_THEME_A_COUNT);
on_catch_engine(packet, 7, 2, main_theme_a, &main_theme_a_status, MAIN_THEME_A_COUNT);
on_catch_engine(packet, 8, 2, main_theme_a, &main_theme_a_status, MAIN_THEME_A_COUNT);




	// Ball 7-9: main theme b
	three_ball_engine(packet, 9, 10, 11, 3, main_theme_b, &main_theme_b_status, MAIN_THEME_B_COUNT);
}
