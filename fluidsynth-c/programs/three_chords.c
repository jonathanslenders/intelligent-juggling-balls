#include <time.h> /* for clock() */
#include <pthread.h> /* posix threading */

#include "../main.h"

#define CHORD1_1 67 // G4 |
#define CHORD1_2 71 // B4 |- G
#define CHORD1_3 74 // D5 |

#define CHORD2_1 60 // C4 |
#define CHORD2_2 64 // E4 |- C
#define CHORD2_3 67 // G4 |

#define CHORD3_1 62 // D4 |
#define CHORD3_2 66 // Fs4 |- D
#define CHORD3_3 69 // A4 |

#define BALL1_1 2
#define BALL1_2 3
#define BALL1_3 4
#define BALL1 "2,3,4"

#define BALL2_1 1
#define BALL2_2 5
#define BALL2_3 6
#define BALL2 "1,5,6"

#define BALL3_1 7
#define BALL3_2 8
#define BALL3_3 9
#define BALL3 "7,8,9"

volatile bool initialized = false;

void three_chords_activate_thread(void* data);

void three_chords_activate(void* data)
{

	// chord 1: channel 1
	fluid_synth_program_select(synth, 1, fluid_font_id, 0, 2); // eletric piano
	fluid_synth_cc(synth, 1, 10, 0); /* 10=pan, between 0 and 127 */

	// chord 2: channel 2
	fluid_synth_program_select(synth, 2, fluid_font_id, 0, 2); // eletric piano
	fluid_synth_cc(synth, 2, 10, 64); /* 10=pan, between 0 and 127 */
	//fluid_synth_cc(synth, 2, 7, 80); /* 7=volume, between 0 and 127 */

	// chord 3: channel 3
	fluid_synth_program_select(synth, 3, fluid_font_id, 0, 2); // eletric piano
	fluid_synth_cc(synth, 3, 10, 127); /* 10=pan, between 0 and 127 */

	// Effect channel 10: percussion
	fluid_synth_program_select(synth, 10, fluid_font_id, 128, 0);

	// Turn all lights off, to begin
	send_packet("RUN", 0, "fixed", "222222");


	initialized = false;
	pthread_t activate_thread;
	pthread_create(&activate_thread, NULL, (void *) &three_chords_activate_thread, (void *) NULL);
}


void three_chords_activate_thread(void* data)
{
	sleep(2);
	send_packet("RUN", 0, "fixed", "000000");
	sleep(1);

	// Ready for part 1

	fluid_synth_noteon(synth, 1, CHORD1_1, 127);
	fluid_synth_noteon(synth, 1, CHORD1_2, 127);
	fluid_synth_noteon(synth, 1, CHORD1_3, 127);
	send_packet2("RUN", BALL1, "pulse", "ffffff_888800:200");

	sleep(1);
	fluid_synth_noteoff(synth, 1, CHORD1_1);
	fluid_synth_noteoff(synth, 1, CHORD1_2);
	fluid_synth_noteoff(synth, 1, CHORD1_3);

	fluid_synth_noteon(synth, 2, CHORD2_1, 127);
	fluid_synth_noteon(synth, 2, CHORD2_2, 127);
	fluid_synth_noteon(synth, 2, CHORD2_3, 127);
	send_packet2("RUN", BALL2, "pulse", "ffffff_008888:200");

	sleep(1);
	fluid_synth_noteoff(synth, 2, CHORD2_1);
	fluid_synth_noteoff(synth, 2, CHORD2_2);
	fluid_synth_noteoff(synth, 2, CHORD2_3);

	fluid_synth_noteon(synth, 3, CHORD3_1, 127);
	fluid_synth_noteon(synth, 3, CHORD3_2, 127);
	fluid_synth_noteon(synth, 3, CHORD3_3, 127);
	send_packet2("RUN", BALL3, "pulse", "ffffff_880088:200");

	sleep(1);
	fluid_synth_noteoff(synth, 3, CHORD3_1);
	fluid_synth_noteoff(synth, 3, CHORD3_2);
	fluid_synth_noteoff(synth, 3, CHORD3_3);

	initialized = true;

return;

	// Ready for part 2
	sleep(18);
	initialized = false;

	fluid_synth_program_select(synth, 1, fluid_font_id, 0, 66); // 66
	fluid_synth_program_select(synth, 2, fluid_font_id, 0, 66); // 66
	fluid_synth_program_select(synth, 3, fluid_font_id, 0, 66); // 66

	fluid_synth_noteon(synth, 1, CHORD1_1, 127);
	fluid_synth_noteon(synth, 1, CHORD1_2, 127);
	fluid_synth_noteon(synth, 1, CHORD1_3, 127);
	send_packet2("RUN", BALL1, "pulse", "ffffff_888800:200");

	sleep(1);
	fluid_synth_noteoff(synth, 1, CHORD1_1);
	fluid_synth_noteoff(synth, 1, CHORD1_2);
	fluid_synth_noteoff(synth, 1, CHORD1_3);

	fluid_synth_noteon(synth, 2, CHORD2_1, 127);
	fluid_synth_noteon(synth, 2, CHORD2_2, 127);
	fluid_synth_noteon(synth, 2, CHORD2_3, 127);
	send_packet2("RUN", BALL2, "pulse", "ffffff_008888:200");

	sleep(1);
	fluid_synth_noteoff(synth, 2, CHORD2_1);
	fluid_synth_noteoff(synth, 2, CHORD2_2);
	fluid_synth_noteoff(synth, 2, CHORD2_3);

	fluid_synth_noteon(synth, 3, CHORD3_1, 127);
	fluid_synth_noteon(synth, 3, CHORD3_2, 127);
	fluid_synth_noteon(synth, 3, CHORD3_3, 127);
	send_packet2("RUN", BALL3, "pulse", "ffffff_880088:200");

	sleep(1);
	fluid_synth_noteoff(synth, 3, CHORD3_1);
	fluid_synth_noteoff(synth, 3, CHORD3_2);
	fluid_synth_noteoff(synth, 3, CHORD3_3);


	initialized = true;
}


void three_chords_packet_received(struct juggle_packet_t* packet)
{
	int ball = packet->ball;
	int chord = 0;
	int channel= 0;

	if (ball == BALL1_1) { chord = CHORD1_1; channel = 1; }
	if (ball == BALL1_2) { chord = CHORD1_2; channel = 1; }
	if (ball == BALL1_3) { chord = CHORD1_3; channel = 1; }

	if (ball == BALL2_1) { chord = CHORD2_1; channel = 2; }
	if (ball == BALL2_2) { chord = CHORD2_2; channel = 2; }
	if (ball == BALL2_3) { chord = CHORD2_3; channel = 2; }

	if (ball == BALL3_1) { chord = CHORD3_1; channel = 3; }
	if (ball == BALL3_2) { chord = CHORD3_2; channel = 3; }
	if (ball == BALL3_3) { chord = CHORD3_3; channel = 3; }

	if (chord && initialized)
	{
		if (strcmp(packet->action, "CAUGHT") == 0)
			fluid_synth_noteoff(synth, channel, chord);

		if (strcmp(packet->action, "CAUGHT*") == 0)
			fluid_synth_noteoff(synth, channel, chord);

		if (strcmp(packet->action, "THROWN") == 0)
			fluid_synth_noteon(synth, channel, chord, 100);
	}
}

