#include <stdio.h> /* Standard input/output definitions */
#include <string.h> /* String function definitions */

#include "../main.h"


#define BUTTON_ID 11

// Queue configuration

struct queue_entry_t {
	void(*program_func)(void*data);
	void* data;
};


struct juggle_program_t* active_proxied_program = NULL;

int queue_position = 0;

void proxy_program(void*data);

#define QUEUE_LENGTH 5

struct queue_entry_t queue[] = {
	// We will start by having all lights red, having the balls in a row,
	// a single touch will make the balls light up white. TODO: add some sound.
	{ proxy_program, "light-up-on-movement"},

	// Every next throw of the 'button' will change the other balls, still
	// lying in a row.
	{ proxy_program, "green"},
	{ proxy_program, "blue"},
	{ proxy_program, "red"},

	//
	{ proxy_program, "fur-elise"},
	{ 0, NULL },
};



void deactivate_proxied_program(void)
{
	if (active_proxied_program && active_proxied_program->deactivate)
		active_proxied_program->deactivate();
	active_proxied_program = NULL;
}

// Proxy for other normal programs to appear in the queue
void proxy_program(void*data)
{
	deactivate_proxied_program();
	active_proxied_program = get_program_from_name((char*)data);
	active_proxied_program->activate(NULL);
	print_string("Proxy program: %s", (char*)data);
}

void activate_queue_entry(int position)
{
	deactivate_proxied_program();
	queue[position].program_func(queue[position].data);
}


// Queue program

void queue_activate(void*data)
{
	// Make ball 14 a button
	send_packet("BUTTON", BUTTON_ID, "ON", NULL);

	queue_position = 0;
	active_proxied_program = NULL;
	activate_queue_entry(0);
}

void queue_deactivate(void*data)
{
	send_packet("BUTTON", BUTTON_ID, "OFF", NULL);
}

void queue_packet_received(struct juggle_packet_t * data)
{
	if (strcmp(data->action, "BUTTON_PRESSED") == 0)
	{
		print_string("Button pressed, next part of the show");

		if (queue_position + 1 < QUEUE_LENGTH)
		{
			queue_position ++;
			activate_queue_entry(queue_position);
		}
	}
	
	if (active_proxied_program && active_proxied_program->packet_received)
		active_proxied_program->packet_received(data);
}
