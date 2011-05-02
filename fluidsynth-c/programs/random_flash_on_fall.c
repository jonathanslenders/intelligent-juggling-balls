#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */
#include "../main.h"

/* *** When falling, random ball lights up *** */


int fall_count = 6;

void light_random_on_fall_activate(void)
{
	send_packet("RUN", 0, "fade", "111111:500");
	fall_count = 0;

}

void light_random_on_fall_packet_received_thread(void*data)
{
	int ball = (int)data;

	if (fall_count < 6)
	{
		// Now flash 'random' other ball
		send_packet("RUN", BALL_COUNT - ball, "fade", "ffffff:100");
		sleep(1);
		send_packet("RUN", BALL_COUNT - ball, "fade", "111111:500");
	}
	else if (fall_count == 6)
	{
		// Flash all red
		send_packet("RUN", 0, "fade", "ff0000:200");
		sleep(5);
		send_packet("RUN", 0, "fade", "111111:500");
	}

	fall_count ++;
	print_string("Fall count= %i", fall_count);
}

void light_random_on_fall_packet_received(struct juggle_packet_t* packet)
{
	if (strcmp(packet->action, "CAUGHT") == 0)
	{
		pthread_t thread;
		pthread_create(&thread, NULL, (void *) &light_random_on_fall_packet_received_thread, (void *) packet->ball);
	}
}


