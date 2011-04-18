#ifndef __MAIN_H__
#define __MAIN_H__

#include <fluidsynth.h>

#include <curses.h> /* ncurses interface */

// Music notes definitions



//  D Flat scale

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

// C sharp Scale

#define G_SHARP__Cs4 61
#define G_SHARP__D4 62
#define G_SHARP__E4 64
#define G_SHARP__Fs4 66
#define G_SHARP__G4 67
#define G_SHARP__A4 69
#define G_SHARP__B4 71

#define G_SHARP__Cs5 73
#define G_SHARP__D5 74
#define G_SHARP__E5 76
#define G_SHARP__Fs5 78
#define G_SHARP__G5 79
#define G_SHARP__A5 81
#define G_SHARP__B5 83


// C majar scale

#define G_MAJOR__C4 60
#define G_MAJOR__D4 62
#define G_MAJOR__E4 64
#define G_MAJOR__F4 65
#define G_MAJOR__G4 67
#define G_MAJOR__A4 69
#define G_MAJOR__B4 71

#define G_MAJOR__C5 72
#define G_MAJOR__D5 74
#define G_MAJOR__E5 76
#define G_MAJOR__F5 77
#define G_MAJOR__G5 79
#define G_MAJOR__A5 81
#define G_MAJOR__B5 83



// State machine

#define BALL_COUNT 16

struct juggle_ball_state_t
{
	double voltage;
	bool in_free_fall;
	bool on_table;
	int throws;
	int catches;
	char last_run_command[256];
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


void send_packet(char* command, int ball, char* param1, char* param2);


#endif
