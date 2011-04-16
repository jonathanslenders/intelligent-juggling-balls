#ifndef __MAIN_H__
#define __MAIN_H__

#include <fluidsynth.h>

#include <curses.h> /* ncurses interface */

// Music notes definitions

#define D_FLAT__Db3 49

#define D_FLAT__C4 60
#define D_FLAT__Db4 61
#define D_FLAT__Eb4 63
#define D_FLAT__F4 65
#define D_FLAT__Gb4 66
#define D_FLAT__Ab4 68
#define D_FLAT__Bb4 70

#define D_FLAT__C5 72
#define D_FLAT__Db5 73
#define D_FLAT__Eb5 75
#define D_FLAT__F5 77
#define D_FLAT__Gb5 78
#define D_FLAT__Ab5 80
#define D_FLAT__Bb5 82



// State machine

#define BALL_COUNT 16

struct juggle_ball_state_t
{
	double voltage;
	bool in_free_fall;
	bool on_table;
	int throws;
	int catches;
	//char current_program[256];
};

extern struct juggle_ball_state_t juggle_states[BALL_COUNT];


// Juggle data packet
struct juggle_packet_t
{
	int ball; // Zero means addressing all balls, otherwise ball number
	char action[256];
	char param1[256];
	char param2[256];
};


// Program meta information
struct juggle_program_t
{
	char description[256];
	void (*activate)(void);
	void (*deactivate)(void);
	void (*packet_received)(struct juggle_packet_t* packet);
};


void print_string(const char* format, ...) __attribute__ ((format (printf, 1, 2)));


extern fluid_synth_t* synth;
extern int fluid_font_id;




#endif
