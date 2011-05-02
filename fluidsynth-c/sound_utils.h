
#ifndef __SOUND_UTILS_H__
#define __SOUND_UTILS_H__

#include "main.h"

struct theme_note_t {
	int note;
};


struct theme_status_t {
	bool playing;
	bool go_to_next;
	int position;
};

void one_ball_engine(struct juggle_packet_t* packet, int ball, int channel, int note);


void three_ball_engine(struct juggle_packet_t* packet, int ball1, int ball2, int ball3,
			int channel, struct theme_note_t* theme, struct theme_status_t* status, int theme_count);

void on_catch_engine(struct juggle_packet_t* packet, int ball, int channel, struct theme_note_t* theme,
		struct theme_status_t* status, int theme_count);


#endif
