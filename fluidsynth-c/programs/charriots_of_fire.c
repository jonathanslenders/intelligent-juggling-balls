#include "../main.h"
#include "charriots_of_fire.h"

void charriots_activate(void)
{
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 7);
	//fluid_synth_program_select(synth, 0, fluid_font_id, 0, 18); // accordeon/organ
}
void charriots_packet_received(struct juggle_packet_t* packet)
{
	print_string("Packet received: %i %s %s %s\n", packet->ball, packet->action, packet->param1, packet->param2);

/*
	//printf("action=%s\n", packet->action);
	if (strcmp(packet->action, "CAUGHT") == 0)
		fluid_synth_noteoff(synth, 0, D_FLAT__Db3);

	else if (strcmp(packet->action, "CAUGHT*") == 0)
		fluid_synth_noteoff(synth, 0, D_FLAT__Db3);

	else if (strcmp(packet->action, "THROWN") == 0)
		fluid_synth_noteon(synth, 0, D_FLAT__Db3, 100);
*/

	if (strcmp(packet->action, "CAUGHT") == 0)
		fluid_synth_noteon(synth, 0, D_FLAT__F4, 100);
}


