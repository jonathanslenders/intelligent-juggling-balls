#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */
#include "../main.h"
#include "../utils.h"


pthread_t fade_thread;
bool fade_running = false;

void fade_thread_start(void* data);

/* **** 1: debug **** */
void fade_activate(void * data)
{
	fade_running = true;
	pthread_create(&fade_thread, NULL, (void *) &fade_thread_start, (void *) NULL);
}

void fade_deactivate(void)
{
	fade_running = false;
}

void fade_thread_start(void* data)
{
	int i;
	for(i = 0; i < 2 && fade_running; i ++)
	{
		// fade all together
		send_packet("RUN", 0, "fade", "FFFF00:150");
		sleep(1);
		send_packet("RUN", 0, "fade", "00FF00:150");
		sleep(1);
		send_packet("RUN", 0, "fade", "00FFFF:150");
		sleep(1);
		send_packet("RUN", 0, "fade", "0000FF:150");
		sleep(1);
		send_packet("RUN", 0, "fade", "FF00FF:150");
		sleep(1);
		send_packet("RUN", 0, "fade", "FF0000:150");

		// Rainbow effect
		int offset;
		for(offset = 0; offset < 256 && fade_running; offset += 50)
		{
			for(i = 0; i < 15; i ++)
			{
				int pos = ( 255*i/15 + offset) % 256;
				int r,g,b;
				interpolate(pos, &r, &g, &b);
				char code[128];
				sprintf(code, "%02x%02x%02x:250", r, g, b);
				send_packet("RUN", i, "fade", code);
			}
			sleep(2);
		}

		// Loop light
		int j;
		for (j = 0; j < 80 && fade_running; j ++)
		{
			send_packet("RUN", (j+1) % 16, "fixed", "FFFFFF");
			send_packet("RUN", j % 16, "fixed", "000000");
			sleep(1);
		}

	}
}


