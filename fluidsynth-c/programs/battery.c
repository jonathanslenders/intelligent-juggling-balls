#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */
#include "../main.h"


pthread_t battery_test_thread;

void battery_test_start(void* data);


/* *** Battery test ***/
void battery_test_activate(void*data)
{
	pthread_create(&battery_test_thread, NULL, (void *) &battery_test_start, (void *) NULL);
}

void battery_test_start(void* data)
{
	// Reset voltage 
	int i;
	for (i = 0; i < BALL_COUNT; i ++)
	{
		juggle_states[i].voltage = 0;

		// Query voltage again
    	send_packet("BATTTEST", i, NULL, NULL);

		// Wait 1sec beforing pinging the next. (avoid wireless network
		// collisions during all the answers.)
		sleep(1);
	}
}

void battery_test_packet_received(struct juggle_packet_t* packet)
{
	print_string("ball %i: %s %s", packet->ball, packet->param1, packet->param2);
}

