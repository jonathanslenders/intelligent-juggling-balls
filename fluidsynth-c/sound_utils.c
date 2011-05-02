
#include "sound_utils.h"
#include "main.h"

void one_ball_engine(struct juggle_packet_t* packet, int ball, int channel, int note)
{
	if (packet->ball == ball)
	{
		if (strcmp(packet->action, "THROWN") == 0)
			fluid_synth_noteon(synth, 1, note, 100);

		if (strcmp(packet->action, "CAUGHT") == 0)
			fluid_synth_noteoff(synth, 1, note);

		if (strcmp(packet->action, "CAUGHT*") == 0)
			fluid_synth_noteoff(synth, 1, note);
	}
}

// Play a theme on these three balls. Every following throw will sound the next note.
void three_ball_engine(struct juggle_packet_t* packet, int ball1, int ball2, int ball3,
			int channel, struct theme_note_t* theme, struct theme_status_t* status, int theme_count)
{
	if (packet->ball == ball1 || packet->ball == ball2 || packet->ball == ball3)
	{
		int note = theme[status->position].note;

		if (strcmp(packet->action, "THROWN") == 0)
		{
			status->playing = true;

			fluid_synth_noteoff(synth, channel, note);

			if (status->go_to_next)
			{
				status->position = (status->position + 1) % theme_count;
				note = theme[status->position].note;
				fluid_synth_noteoff(synth, channel, note);
				status->go_to_next = false;
			}
			fluid_synth_noteon(synth, channel, note, 100);
		}
		else if (strcmp(packet->action, "CAUGHT*") == 0 && status->playing)
		{
			// When 6-8 are in hand -> stop sound
			if (! juggle_states[ball1].in_free_fall && ! juggle_states[ball2].in_free_fall && ! juggle_states[ball3].in_free_fall)
			{
				status->playing = false;
				fluid_synth_noteoff(synth, channel, note);
			}
		}
		else if (strcmp(packet->action, "CAUGHT") == 0 && status->playing)
		{
			// When 6-8 are in hand -> stop sound, consider caught and increase playing index
			if (! juggle_states[ball1].in_free_fall && ! juggle_states[ball2].in_free_fall &&
							! juggle_states[ball3].in_free_fall)
			{
				status->go_to_next = true;
				status->playing = false;
				fluid_synth_noteoff(synth, channel, note);
			}
		}
		else if (strcmp(packet->action, "IN_FREE_FALL") == 0 && status->playing)
		{
			status->go_to_next = true;
		}
	}
}


void on_catch_engine(struct juggle_packet_t* packet, int ball, int channel, struct theme_note_t* theme,
		struct theme_status_t* status, int theme_count)
{
	if (packet->ball == ball && strcmp(packet->action, "CAUGHT") == 0)
	{
		int note = theme[status->position].note;
		fluid_synth_noteon(synth, channel, note, 100);
		status->position = (status->position + 1) % theme_count;
	}
}
