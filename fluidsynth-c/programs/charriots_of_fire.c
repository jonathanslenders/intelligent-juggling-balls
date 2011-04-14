#include <stdlib.h> /* for malloc, free, exit */

#include "../main.h"
#include "charriots_of_fire.h"


void charriots_packet_received_thread(void* data);

void charriots_packet_received(struct juggle_packet_t* packet)
{
    /*
        fluid_synth_noteon(synth, 0, D_FLAT__Db3, 127);
        usleep(2000 * 1000);
        fluid_synth_noteoff(synth, 0, D_FLAT__Db3);
        return;
    */

    // Allocate memory for using packet in thread
    struct juggle_packet_t* packet_copy = (struct juggle_packet_t*) malloc(sizeof(struct juggle_packet_t));
    *packet_copy = *packet;

    // Start thread for handling packets (new thread allows us to use sleep.)
    pthread_t thread;
    pthread_create(&thread, NULL, (void *) &charriots_packet_received_thread, (void *) packet_copy);
}

void charriots_activate(void)
{
    // Load the instruments at the right channel
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 7);
	fluid_synth_program_select(synth, 1, fluid_font_id, 0, 18);
	//fluid_synth_program_select(synth, 0, fluid_font_id, 0, 18); // accordeon/organ
}

void charriots_packet_received_thread(void* data)
{
    struct juggle_packet_t* packet = (struct juggle_packet_t*) data;

	print_string("Packet received: %i %s %s %s\n", packet->ball, packet->action, packet->param1, packet->param2);

    // Ball 1-3 caught: guitar strings for intro
	if (strcmp(packet->action, "CAUGHT") == 0 && packet->ball >= 1 && packet->ball <= 3)
    {
        fluid_synth_noteon(synth, 0, D_FLAT__Db3, 100);
        usleep(1000 * 1000);
        fluid_synth_noteoff(synth, 0, D_FLAT__Db3);
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

/*
	//printf("action=%s\n", packet->action);
	if (strcmp(packet->action, "CAUGHT") == 0)
		fluid_synth_noteoff(synth, 0, D_FLAT__Db3);

	else if (strcmp(packet->action, "CAUGHT*") == 0)
		fluid_synth_noteoff(synth, 0, D_FLAT__Db3);

	else if (strcmp(packet->action, "THROWN") == 0)
		fluid_synth_noteon(synth, 0, D_FLAT__Db3, 100);
*/

	else if (strcmp(packet->action, "CAUGHT") == 0)
		fluid_synth_noteon(synth, 0, D_FLAT__F4, 100);

    // Free packet (used by this thread only)
    free(packet);
}


