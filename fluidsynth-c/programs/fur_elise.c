#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */
#include <time.h> /* for clock() */
#include "../main.h"


pthread_t fur_elise_thread;

clock_t last_catch = 0;
clock_t last_note = 0;
int last_ball = 0;
int next_color = 0;

fluid_player_t* player;


#define DEFAULT_BPM 40


int midi_callback(void *data, fluid_midi_event_t* event)
{
print_string("Channel %i", fluid_midi_event_get_channel(event));
	fluid_synth_program_select(synth, fluid_midi_event_get_channel(event), fluid_font_id, 0, 18); // accordeon/organ
	fluid_synth_cc(synth, fluid_midi_event_get_channel(event), 10, 127); /* 10=pan, between 0 and 127 */


	// Calculate duration between last note and now
	clock_t now = clock();
	int duration = (now - last_note) / (CLOCKS_PER_SEC/1000); // millisec

	char* color[] = {
		"FF0000:50",
		"FFFF00:50",
		"00FF00:50",
		"00FFFF:50",
		"0000FF:50",
		"FF00FF:50",
	};

	//print_string("event type %d\n", fluid_midi_event_get_type(event));
	if (duration && fluid_midi_event_get_type(event) == 0x90) // NOTE_ON
	{
		print_string("note on %i", fluid_midi_event_get_key(event));
		int ball = 0; // last_ball

		// White on really quick movements
		if (duration < 100)
			send_packet("RUN", ball, "fixed", "ffffff");
		// Fade on slow movements
		else if (duration > 200)
			send_packet("RUN", ball, "fade", color[next_color]);
		else
			send_packet("RUN", ball, "fixed", color[next_color]);

		next_color = (next_color + 1) % 6;

		// Print duration
		print_string("duration %i", duration);
		last_note = now;
	}

	//print_string("event type %d channel %c\n", fluid_midi_event_get_type(event), event->channel);
}

void fur_elise_activate(void)
{
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 12); // Marimba


//	fluid_settings_t * settings;
//	fluid_midi_driver_t* mdriver;

//	settings = new_fluid_settings();
//	mdriver = new_fluid_midi_driver(settings, handle_midi_event, NULL);

	player = new_fluid_player(synth);
	player = new_fluid_player2(synth, midi_callback, NULL);

	//fluid_player_add(player, "fur-elise.mid");
	fluid_player_add(player, "mb01.mid");
	fluid_player_play(player);
	sleep(1);
	fluid_player_set_bpm(player,0); // pause
//	fluid_player_set_bpm(player,DEFAULT_BPM); // pause

//int j;
//for (j = 0; j < 255; j ++)
//	fluid_synth_cc(synth, j, 10, 120); /* 10=pan, between 0 and 127 */

	last_ball = 0;
	last_catch = 0;
	last_note = 0;

	//fluid_player_set_midi_tempo(player, 5000);
//	fluid_player_join(player);


	// delete_fluid_midi_driver(mdriver);
}

void fur_elise_deactivate(void)
{
	fluid_player_stop(player);
	delete_fluid_player(player);
}

void fur_elise_packet_received(struct juggle_packet_t* packet)
{
	clock_t now = clock();

	if (strcmp(packet->action, "CAUGHT") == 0) // || strcmp(packet->action, "CAUGHT*") == 0)
	{
		// Time between now and last catch
		if (last_catch) {
			int duration = (now - last_catch) / (CLOCKS_PER_SEC/1000); // millisec
			int bpm = (60*300) / duration;
//			fluid_player_set_bpm(player, bpm);
		}
		else
			fluid_player_set_bpm(player, 60);

		last_catch = now;
	}
	if (strcmp(packet->action, "CAUGHT*") == 0)
	{
		last_catch = now;
	}
	if (strcmp(packet->action, "THROWN") == 0)
	{
		if (! last_catch)
			fluid_player_set_bpm(player, DEFAULT_BPM);
		last_ball = packet->ball;
	}

	// When all balls are caught, pause music
	int i;
	bool play = false;
	for (i = 0; i < BALL_COUNT; i ++)
		if (juggle_states[i].in_free_fall)
			play = true;

	if (! play)
	{
		fluid_player_set_bpm(player, 0);
		last_catch = 0;
	}
}


