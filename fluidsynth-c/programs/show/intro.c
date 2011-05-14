#include <time.h> /* for clock() */
#include "../../main.h"

fluid_player_t* player;


void intro_activate(void * data)
{
	send_packet("RUN", 0, "fixed", "010000");

	// Start background music
	player = new_fluid_player2(synth, NULL, NULL);
	fluid_player_add(player, "2001.mid");
	fluid_player_play(player);

	// Slowly fade in
	send_packet("RUN", 0, "fade", "ff1111:10000");
}

void intro_deactivate(void)
{
	fluid_player_stop(player);
	delete_fluid_player(player);
}
