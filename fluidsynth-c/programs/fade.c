#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */
#include "../main.h"


pthread_t fade_thread;

void fade_thread_start(void* data);

/* **** 1: debug **** */
void fade_activate(void)
{
	pthread_create(&fade_thread, NULL, (void *) &fade_thread_start, (void *) NULL);
}

void interpolate(int pos, int* r, int* g, int* b)
{
	// From red to blue
	if (pos < 256/3)
	{
		pos *= 3;
		*r = 255-pos;
		*g = pos;
		*b = 0;
	}
	else if (pos < 256*2/3)
	{
		pos -= (256/3);
		pos *= 3;
		*r = 0;
		*g = 255-pos;
		*b = pos;
	}
	else
	{
		pos -= (256*2/3);
		pos *= 3;
		*r = pos;
		*g = 0;
		*b = 255-pos;
	}
}

void fade_thread_start(void* data)
{
	int i;
	for(i = 0; i < 2; i ++)
	{
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

		// Rainbox
		int offset;
		for(offset = 0; offset < 256; offset += 50)
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
	}
}


