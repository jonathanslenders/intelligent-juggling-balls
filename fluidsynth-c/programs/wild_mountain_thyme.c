#include <stdlib.h> /* for malloc, free, exit */

#include "../main.h"
#include "wild_mountain_thyme.h"
#include "../sound_utils.h"

// Violin / Piano
#define THEME_COUNT 35
struct theme_status_t theme_status =
{
	false,
	0,
};

struct theme_note_t theme[] = 
{
		{ G_SHARP__G4, },
		{ D_FLAT__Eb5, },
		{ D_FLAT__Db5, },

		{ D_FLAT__Db5, },
		{ D_FLAT__Db5, },
		{ D_FLAT__Eb5, },
		{ G_SHARP__G4, },

		{ G_SHARP__G4, },
		{ G_SHARP__G4, },
		{ G_SHARP__B4, },
		{ G_SHARP__D5, },
		{ G_SHARP__E5, },
		{ G_SHARP__D5, },
		{ G_SHARP__E5, },
		{ G_SHARP__D5, },
		{ G_SHARP__B4, },
		{ G_SHARP__D5, },

		{ G_SHARP__B4, },
		{ G_SHARP__D5, },
		{ G_SHARP__E5, },
		{ G_SHARP__D5, },
		{ G_SHARP__B4, },
		{ G_SHARP__A4, },
		{ G_SHARP__G4, },

		{ G_SHARP__A4, },
		{ G_SHARP__B4, },
		{ G_SHARP__Cs5, },
		{ G_SHARP__B4, },
		{ G_SHARP__A4, },
		{ G_SHARP__G4, },
		{ G_SHARP__E4, },
		{ G_SHARP__G4, },

		{ G_SHARP__G4, },
		{ G_SHARP__E4, },
		{ G_SHARP__D4, },
};


void wild_mountain_thyme_activate(void)
{
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 74);

	fluid_synth_cc(synth, 0, 10, 64); /* 10=pan, between 0 and 127 */
	fluid_synth_cc(synth, 0, 7, 127); /* 7=volume, between 0 and 127 */
	fluid_synth_cc(synth, 0, 64, 30); /* 64=sustain */
	fluid_synth_cc(synth, 0, 91, 100); /* 91=reverb */
}

void wild_mountain_thyme_deactivate(void)
{
	fluid_synth_cc(synth, 0, 123, 0);
}


void wild_mountain_packet_received(struct juggle_packet_t* packet)
{
	// Ball 1-3: theme
	three_ball_engine(packet, 1, 2, 3, 0, theme, &theme_status, THEME_COUNT);
}
