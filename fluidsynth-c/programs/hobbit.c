#include <stdlib.h> /* for malloc, free, exit */

#include "../main.h"
#include "hobbit.h"
#include "../sound_utils.h"

// Violin / Piano
#define THEME_COUNT 39
struct theme_status_t hobbit_theme_status =
{
	false,
	0,
};

struct theme_note_t hobbit_theme[] = 
{
		{ C_MAJOR__C4, },
		{ C_MAJOR__D4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__G4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__D4, },
		{ C_MAJOR__C4, },

		{ C_MAJOR__D4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__G4, },
		{ C_MAJOR__A4, },
		{ C_MAJOR__C5, },
		{ C_MAJOR__B4, },
		{ C_MAJOR__G4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__F4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__D4, },


		{ C_MAJOR__C4, },
		{ C_MAJOR__D4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__G4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__D4, },
		{ C_MAJOR__C4, },

		{ C_MAJOR__D4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__G4, },
		{ C_MAJOR__A4, },
		{ C_MAJOR__C5, },
		{ C_MAJOR__B4, },
		{ C_MAJOR__G4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__F4, },
		{ C_MAJOR__E4, },
		{ C_MAJOR__D4, },

		{ C_MAJOR__C4, },
		{ C_MAJOR__D4, },
		{ C_MAJOR__C4, },
};


void hobbit_activate(void * data)
{
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 74);

	fluid_synth_cc(synth, 0, 10, 64); /* 10=pan, between 0 and 127 */
	fluid_synth_cc(synth, 0, 7, 127); /* 7=volume, between 0 and 127 */
	fluid_synth_cc(synth, 0, 64, 30); /* 64=sustain */
	fluid_synth_cc(synth, 0, 91, 100); /* 91=reverb */
}

void hobbit_deactivate(void)
{
	fluid_synth_cc(synth, 0, 123, 0); // 123 == all notes off
}


void hobbit_packet_received(struct juggle_packet_t* packet)
{
	// Ball 1-3: theme
	three_ball_engine(packet, 1, 2, 3, 0, hobbit_theme, &hobbit_theme_status, THEME_COUNT);
}
