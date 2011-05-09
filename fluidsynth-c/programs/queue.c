#include <stdio.h> /* Standard input/output definitions */
#include <string.h> /* String function definitions */

#include "../main.h"


#define BUTTON_ID 14

// Queue configuration

struct queue_entry_t {
	void(*program_func)(void*data);
	void* data;
};


struct juggle_program_t* active_proxied_program = NULL;

int queue_position = 0;

void proxy_program(void*data);
void intermezzo(void*data);
void intermezzo2(void*data);
void intermezzo3(void*data);

#define QUEUE_LENGTH 13

struct queue_entry_t queue[] = {
	// We will start by having all lights red, having the balls in a row,
	// a single touch will make the balls light up white. TODO: add some sound.
	{ proxy_program, "light-up-on-movement"},

	// Every next throw of the 'button' will change the other balls, still
	// lying in a row.
	{ proxy_program, "green"},
	{ proxy_program, "blue"},
	{ proxy_program, "red"},
	{ proxy_program, "yellow"},

	// Play 'broeder jacob', followed by playing tetris tune.

	{ intermezzo, NULL },
	{ intermezzo2, NULL },
	{ intermezzo3, NULL },

	{ proxy_program, "c-major"}, 

	{ proxy_program, "fur-elise"},

	// The end
	{ proxy_program, "green"},
	{ proxy_program, "blue"},
	{ proxy_program, "red"},
	{ proxy_program, "the-end"},
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

	fluid_synth_cc(synth, 0, 7, 127); /* 7=volume, between 0 and 127 */
	fluid_synth_cc(synth, 10, 7, 127); /* 7=volume, between 0 and 127 */
}


// Before fur elise
void intermezzo(void*data)
{
	// All black
	send_packet("RUN", 0, "fixed", "000000");

	// Fade first to red
	fluid_synth_program_select(synth, 10, fluid_font_id, 128, 0);
	int sound = 61; // Low Bongo
	fluid_synth_noteon(synth, 10, sound, 127);
	send_packet2("RUN", "2,3", "pulse", "ffffff_aa0044:200");
}
void intermezzo2(void*data)
{
	// Fade second to blue
	fluid_synth_program_select(synth, 10, fluid_font_id, 128, 0);
	int sound = 61; // Low Bongo
	fluid_synth_noteon(synth, 10, sound, 127);
	send_packet2("RUN", "4,5,1", "pulse", "ffffff_44aa00:200");
}
void intermezzo3(void*data)
{
	// Fade third to green 
	fluid_synth_program_select(synth, 10, fluid_font_id, 128, 0);
	int sound = 61; // Low Bongo
	fluid_synth_noteon(synth, 10, sound, 127);
	send_packet2("RUN", "6,7,8", "pulse", "ffffff_4400aa:200");
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

			// next sound
			fluid_synth_program_select(synth, 10, fluid_font_id, 128, 0);
			int sound = 58; // Vibra Slap
        	fluid_synth_noteon(synth, 10, sound, 127);

			activate_queue_entry(queue_position);
		}
	}
	
	if (active_proxied_program && active_proxied_program->packet_received)
		active_proxied_program->packet_received(data);
}
