#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */
#include "../main.h"


pthread_t ping_thread;

void ping_thread_start(void* data);

/* *** 2: Ping *** */
void ping_activate(void)
{
	pthread_create(&ping_thread, NULL, (void *) &ping_thread_start, (void *) NULL);
}

void ping_thread_start(void* data)
{
	int i;
	for (i = 0; i < BALL_COUNT; i ++)
	{
    	send_packet("PING", i, NULL, NULL);
		juggle_states[i].ping_sent = clock();
		juggle_states[i].ping_time = 0;

		// Wait 1sec beforing pinging the next. (avoid wireless network
		// collisions during all the answers.)
		sleep(1);
	}
}

void ping_packet_received(struct juggle_packet_t* packet)
{
	clock_t received = clock();

	if (strcmp(packet->action, "PONG") == 0 && packet->ball >= 1 && packet->ball <= BALL_COUNT)
	{
		print_string("Pong received (ball %i)", packet->ball);
		clock_t sent = juggle_states[packet->ball - 1].ping_sent;
		int duration = (received - sent) / (CLOCKS_PER_SEC/1000); // millisec
		juggle_states[packet->ball - 1].ping_time = duration;
	}
}

