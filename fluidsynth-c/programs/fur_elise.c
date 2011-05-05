#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */
#include <time.h> /* for clock() */
#include "../main.h"


pthread_t fur_elise_thread;

clock_t last_catch = 0;

fluid_player_t* player;

int handle_midi_event(void* data, fluid_midi_event_t* event)
{
	print_string("event type %d\n", fluid_midi_event_get_type(event));
}

void fur_elise_activate(void)
{
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 12); // Marimba

//	fluid_settings_t * settings;
//	fluid_midi_driver_t* mdriver;

//	settings = new_fluid_settings();
//	mdriver = new_fluid_midi_driver(settings, handle_midi_event, NULL);

	player = new_fluid_player(synth);

	fluid_player_add(player, "fur-elise.mid");
	fluid_player_play(player);
	sleep(1);
	fluid_player_set_bpm(player,0); // pause

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

	if (strcmp(packet->action, "CAUGHT") == 0 || strcmp(packet->action, "CAUGHT*") == 0)
	{
		// Time between now and last catch
		if (last_catch) {
			int duration = (now - last_catch) / (CLOCKS_PER_SEC/1000); // millisec
			int bpm = (60*300) / duration;
			fluid_player_set_bpm(player, bpm);
		}
		else
			fluid_player_set_bpm(player, 60);

		last_catch = now;
	}
	if (strcmp(packet->action, "CAUGHT*") == 0)
	{
		

	}
	if (strcmp(packet->action, "THROWN") == 0)
	{
		if (! last_catch)
			fluid_player_set_bpm(player, 60);
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


