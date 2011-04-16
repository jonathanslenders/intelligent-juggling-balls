#include <stdlib.h> /* for malloc, free, exit */

#include "../main.h"
#include "charriots_of_fire.h"



/* === Music definitions === */

struct theme_note_t {
	int note;
};


int main_theme_position = 0;
bool main_theme_playing = false;
#define MAIN_THEME_COUNT 22
struct theme_note_t main_theme[] = 
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


/* ==== Program === */

void charriots_packet_received_thread(void* data);

void charriots_packet_received(struct juggle_packet_t* packet)
{
	// Allocate memory for using packet in thread
	struct juggle_packet_t* packet_copy = (struct juggle_packet_t*) malloc(sizeof(struct juggle_packet_t));
	*packet_copy = *packet;

	bool USE_THREADS = false;

	if (USE_THREADS)
	{
		// Start thread for handling packets (new thread allows us to use sleep.)
		pthread_t thread;
		int result = pthread_create(&thread, NULL, (void *) &charriots_packet_received_thread, (void *) packet_copy);
		if (result)
			print_string("Starting thread failed, error code %i", result);
	}
	else
		charriots_packet_received_thread(packet_copy);
}

void charriots_activate(void)
{
    // Load the instruments at the right channel
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 7);
	fluid_synth_program_select(synth, 1, fluid_font_id, 0, 18);
	//fluid_synth_program_select(synth, 0, fluid_font_id, 0, 18); // accordeon/organ
	main_theme_position = 0;
	main_theme_playing = false;
}

void charriots_packet_received_thread(void* data)
{
    struct juggle_packet_t* packet = (struct juggle_packet_t*) data;

	print_string("Packet received: %i %s %s %s\n", packet->ball, packet->action, packet->param1, packet->param2);

    // Ball 1-3 caught: guitar strings for intro
	if (strcmp(packet->action, "CAUGHT") == 0 && packet->ball >= 1 && packet->ball <= 3)
    {
		// 'Reboot' note (off and on again.)
        fluid_synth_noteoff(synth, 0, D_FLAT__Db3);
        fluid_synth_noteon(synth, 0, D_FLAT__Db3, 100);
//        usleep(1000 * 1000);
//        fluid_synth_noteoff(synth, 0, D_FLAT__Db3);
    }

    // Ball 4-5 Play the intro
	if (strcmp(packet->action, "THROWN") == 0 && packet->ball >= 4 && packet->ball <= 5)
    {
        fluid_synth_noteon(synth, 1, D_FLAT__Db4, 100);
        usleep(1000 * 1000);
        fluid_synth_noteoff(synth, 1, D_FLAT__Db4);

        // TODO: trigger noteoff when the ball has been caught.
        //       but at maximum sound duration (in case caught packet hasn't been received)
        //       Keep table of progress for each player.
    }

	// Ball 6-8: main theme
	if (packet->ball >= 6 && packet->ball <= 8)
	{
		int note = main_theme[main_theme_position].note;

		if (strcmp(packet->action, "THROWN") == 0 && !main_theme_playing)
		{
			main_theme_playing = true;
			fluid_synth_noteoff(synth, 2, note);
			fluid_synth_noteon(synth, 2, note, 100);
		}
		else if (strcmp(packet->action, "CAUGHT*") == 0 && main_theme_playing)
		{
			// When 6-8 are in hand -> stop sound
			if (! juggle_states[6].in_free_fall && ! juggle_states[7].in_free_fall && ! juggle_states[8].in_free_fall)
			{
				main_theme_playing = false;
				fluid_synth_noteoff(synth, 2, note);
			}
		}
		else if (strcmp(packet->action, "CAUGHT") == 0 && main_theme_playing)
		{
			// When 6-8 are in hand -> stop sound, consider caught and increase playing index
			if (! juggle_states[6].in_free_fall && ! juggle_states[7].in_free_fall && ! juggle_states[8].in_free_fall)
			{
				main_theme_playing = false;
				main_theme_position = (main_theme_position + 1) % MAIN_THEME_COUNT;
				fluid_synth_noteoff(synth, 2, note);
			}
		}
	}

    // Free packet (used by this thread only)
    free(packet);
}


