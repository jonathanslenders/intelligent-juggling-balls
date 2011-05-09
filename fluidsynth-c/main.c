#include <signal.h>
#include <stdio.h> /* Standard input/output definitions */
#include <string.h> /* String function definitions */
#include <unistd.h> /* UNIX standard function definitions */
#include <fcntl.h> /* File control definitions */
#include <errno.h> /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <pthread.h> /* posix threading */
#include <stdarg.h> /* for variadic functions */
#include <stdlib.h> /* for malloc, free, exit */
#include <time.h> /* for clock() */

#include <curses.h> /* ncurses interface, also defines boolean types */



#include <fluidsynth.h> /* For the MIDI synthesizer */

#define PORT_NAME "/dev/ttyUSB0"

#include "main.h"
#include "msleep.h"

#include "programs.h"


/* ===============================[ Globals ]============================ */

struct termios options;
struct termios options_original;
int serial_port;


struct juggle_program_t* active_program = NULL;


pthread_t data_read_thread;
WINDOW *serial_window = NULL;
WINDOW *status_window = NULL;
WINDOW *programs_window = NULL;
int programs_window_cursor_position = 0;


/* ===============================[ Serial FIFO queue ]============================ */

/*
 * We need to queue all serial data to be printed in the User interface.
 * -> needs to be handled in the main/GUI thread.
 */

pthread_mutex_t serial_queue_access_mutex = PTHREAD_MUTEX_INITIALIZER;
int serial_line = 0;

struct serial_queue_t
{
	char* string;
    int line;
	struct serial_queue_t* next; // TODO: I don't think we actually need the next pointer
	struct serial_queue_t* prev;
};

struct serial_queue_t * serial_queue_head = NULL;
struct serial_queue_t * serial_queue_tail = NULL;

void print_string(const char* format, ...)
{
	// Lock
	pthread_mutex_lock( &serial_queue_access_mutex);

	// Print into new string
	va_list args;
	va_start(args, format);

	int len = vsnprintf(NULL, 0, format, args);
	char * new_string = (char*) malloc(len + 1);
	vsnprintf(new_string, len + 1, format, args); 

	va_end(args);

	// Create new node
	struct serial_queue_t* new_head = (struct serial_queue_t*) malloc(sizeof(struct serial_queue_t));
	new_head->string = new_string;
    new_head->line = ++ serial_line;

	if (serial_queue_head)
	{
		// Insert at the start of the list
		new_head->next = serial_queue_head;
		new_head->prev = NULL;
		new_head->next->prev = new_head;

		serial_queue_head = new_head;
	}
	else
	{
		// Create new item
		new_head->next = NULL;
		new_head->prev = NULL;

		serial_queue_head = new_head;
		serial_queue_tail = new_head;
	}

	// Unlock
	pthread_mutex_unlock( &serial_queue_access_mutex);
}

char* pop_serial_queue_tail(void)
{
	pthread_mutex_lock( &serial_queue_access_mutex);

	if (serial_queue_tail)
	{
		struct serial_queue_t* new_tail = serial_queue_tail->prev;

		free(serial_queue_tail->string);
		free(serial_queue_tail);

		if (new_tail)
		{
			new_tail->next = NULL;
			serial_queue_tail = new_tail;
		}
		else
		{
			// Last item from the queue has been removed
			serial_queue_tail = NULL;
			serial_queue_head = NULL;
		}
	}

	pthread_mutex_unlock( &serial_queue_access_mutex);
}


/* ===============================[ State register ]============================ */

struct juggle_ball_state_t juggle_states[BALL_COUNT];


/* ===============================[ Packet processing ]============================ */


// Scan input line, and parse packet. Returns true on success
bool parse_packet(char * input, struct juggle_packet_t* packet)
{
	packet->action[0] = '\0';
	packet->ball = 0;
	packet->param1[0] = '\0';
	packet->param2[0] = '\0';

	if (sscanf(input, "%s %i %s %s", packet->action, &packet->ball, packet->param1, packet->param2) >= 2)
		return true;
	else
		return false;
};

void process_packet(struct juggle_packet_t* packet)
{
	int ball = packet->ball;
	if (ball >= 0 && ball <= BALL_COUNT)
	{
		// Switch juggle ball state register
		if (ball > 0)
		{
			if (strcmp(packet->action, "CAUGHT") == 0 || strcmp(packet->action, "CAUGHT*") == 0)
			{
				juggle_states[ball-1].in_free_fall = false;
				juggle_states[ball-1].on_table = false;
				juggle_states[ball-1].catches ++;
			}
			else if (strcmp(packet->action, "THROWN") == 0)
			{
				juggle_states[ball-1].in_free_fall = true;
				juggle_states[ball-1].on_table = false;
				juggle_states[ball-1].throws ++;
			}
			else if (strcmp(packet->action, "ON_TABLE") == 0)
			{
				juggle_states[ball-1].on_table = true;
				juggle_states[ball-1].in_free_fall = false;
			}
			else if (strcmp(packet->action, "MOVING") == 0)
			{
				juggle_states[ball-1].on_table = false;
				juggle_states[ball-1].in_free_fall = false;
			}
			else if (strcmp(packet->action, "VOLTAGE") == 0)
			{
				int voltage;
				if (sscanf(packet->param1, "%i", &voltage))
					juggle_states[ball-1].voltage = voltage;
			}
			else if (strcmp(packet->action, "DEBUG") == 0)
			{
				// Debug packets are printed on the terminal
				print_string("DEBUG: %s %s", packet->param1, packet->param2);
			}
		}

		// Send packet to active program
        if (active_program->packet_received)
		    active_program->packet_received(packet);
	}
}

void send_packet(char* command, int ball, char* param1, char* param2)
{
	char ball2[4];
	sprintf(ball2, "%i", ball);
	send_packet2(command, ball2, param1, param2);

		// TODO: move following code to send_packet2, and parse char*ball instead
	// When we sent a 'RUN' command, keep track in the state table
	/*
    char buffer[256];
	if (strcmp(command, "RUN") == 0)
	{
		if (ball > 0)
			strcpy(juggle_states[ball-1].last_run_command, buffer);
		else
		{
			int i;
			for(i = 0; i < BALL_COUNT; i ++)
				strcpy(juggle_states[i].last_run_command, buffer);
		}
	}
	*/
}

void send_packet2(char* command, char* ball, char* param1, char* param2)
{
    char buffer[256];
    if (! param1) param1 = "";
    if (! param2) param2 = "";

    // Show outgoing packet on console
    snprintf(buffer, 256, "[out] %s %s %s %s\n", command, ball, param1, param2);
    print_string("%s", buffer);

    // Sent packet
    int length = snprintf(buffer, 256, "%s %s %s %s\n", command, ball, param1, param2);
    write(serial_port, buffer, length);
}



/* ===============================[ Fluid synth ]============================ */


fluid_settings_t* fluid_settings;
fluid_audio_driver_t* adriver;
fluid_synth_t* synth;
int fluid_font_id;

void init_fluidsynth()
{
	fluid_settings = new_fluid_settings();

    // Set the synthesizer settings, if necessary
    synth = new_fluid_synth(fluid_settings);

	// Initialize audio driver
    fluid_settings_setstr(fluid_settings, "audio.driver", "alsa");
    adriver = new_fluid_audio_driver(fluid_settings, synth);

	// Load sound font
	fluid_font_id = fluid_synth_sfload(synth, "/usr/share/sounds/sf2/FluidR3_GM.sf2", 1);

	// Test sound ;)
	int i;
	for (i = 0; i < 2; i ++)
    {
		fluid_synth_program_select(synth, 0, fluid_font_id, 0, 7);

		fluid_synth_noteon(synth, 1, D_FLAT__Db3, 100);
		usleep(100 * 1000);
		fluid_synth_noteoff(synth, 1, D_FLAT__Db3);
		usleep(200 * 1000);
	}

}

void cleanup_fluidsynth(void)
{
	delete_fluid_synth(synth);
	delete_fluid_settings(fluid_settings);
}


/* ===============================[ Programs ]============================ */

/* *** Self test ***/

void self_test_activate(void*data)
{
    send_packet("SELFTEST", 0, NULL, NULL);
}

/* *** ADC test ***/
void adc_test_activate(void*data)
{
	send_packet("ADCTEST", 0, NULL, NULL);
}

void adc_test_packet_received(struct juggle_packet_t* packet)
{
	if (strcmp(packet->action, "ADC") == 0)
		print_string("ball %i: ADC %s %s", packet->ball, packet->param1, packet->param2);
}

/* *** 3: Identify *** */
void identify_activate(void*data)
{
    send_packet("IDENTIFY", 0, NULL, NULL);
}

/* *** Standby *** */

void standby_activate(void*data)
{
	send_packet("STANDBY", 0, NULL, NULL);
}

/* *** basic colors * ***/

void purple_activate(void*data)
{
	send_packet("RUN", 0, "fade", "ff22cc:200");
}
void pink_activate(void*data)
{
	send_packet("RUN", 0, "fade", "ff0088:200");
}
void red_activate(void*data)
{
	send_packet("RUN", 0, "fade", "ff0000:200");
}
void green_activate(void*data)
{
	send_packet("RUN", 0, "fade", "00ff00:200");
}
void blue_activate(void*data)
{
	send_packet("RUN", 0, "fade", "0000ff:200");
}
void yellow_activate(void*data)
{
	send_packet("RUN", 0, "fade", "ffff00:200");
}
void white_activate(void*data)
{
	send_packet("RUN", 0, "fade", "ffffff:200");
}

void blue_in_air_magenta_otherwise(void*data)
{
	send_packet("RUN", 0, "fixed", "aa00aa_aa00aa_0000ff_0000ff");
}

void map_force_to_intensity_activate(void*data)
{
	send_packet("RUN", 0, "force_to_i", "ffffff");
}

/* *** Only colors in air *** */

void only_colors_in_air_activate(void*data)
{
	send_packet("RUN", 1, "fixed",  "000000_000000_ff0000_ff0000_ffffff");
	send_packet("RUN", 2, "fixed",  "000000_000000_ff0000_ff0000_ffffff");
	send_packet("RUN", 3, "fixed",  "000000_000000_ff0000_ff0000_ffffff");

	send_packet("RUN", 4, "fixed",  "000000_000000_00ff00_00ff00_ffffff");
	send_packet("RUN", 5, "fixed",  "000000_000000_00ff00_00ff00_ffffff");

	send_packet("RUN", 6, "fixed",  "000000_000000_ffff00_ffff00_ffffff");
	send_packet("RUN", 7, "fixed",  "000000_000000_ffff00_ffff00_ffffff");

	send_packet("RUN", 8, "fixed",  "000000_000000_0000ff_0000ff_ffffff");
	send_packet("RUN", 9, "fixed",  "000000_000000_0000ff_0000ff_ffffff");

	send_packet("RUN", 10, "fixed", "000000_000000_ff00ff_ff00ff_ffffff");
	send_packet("RUN", 11, "fixed", "000000_000000_ff00ff_ff00ff_ffffff");
	send_packet("RUN", 12, "fixed", "000000_000000_ff00ff_ff00ff_ffffff");

	send_packet("RUN", 13, "fixed", "000000_000000_00ffff_00ffff_ffffff");
	send_packet("RUN", 14, "fixed", "000000_000000_00ffff_00ffff_ffffff");
	send_packet("RUN", 15, "fixed", "000000_000000_00ffff_00ffff_ffffff");
}

void deactivate_active_program(void)
{
    if (active_program && active_program->deactivate)
	    active_program->deactivate();
}

// Activate program
void activate_program(struct juggle_program_t* program)
{
	deactivate_active_program();
	print_string("Activating program %s\n", program->description);

    if (program->activate)
	    program->activate(NULL);

	active_program = program;
}

// List of all available programs
#define PROGRAMS_COUNT 28
struct juggle_program_t PROGRAMS[] = {
        {
            "Ping",
			"ping",
            ping_activate,
            NULL,
            ping_packet_received,
        },
		{
			"Queue (for the show",
			"queue",
			queue_activate,
			NULL,
			queue_packet_received,
		},
		{
			"Test fade",
			"test-fade",
			fade_activate,
			fade_deactivate,
			NULL,
		},
		{
			"Battery test",
			"battery-test",
			battery_test_activate,
			NULL,
			battery_test_packet_received,
		},
		{
			"Self test",
			"self-test",
			self_test_activate,
			NULL,
			NULL,
		},
		{
			"ADC test",
			"adc-test",
			adc_test_activate,
			NULL,
			adc_test_packet_received,
		},
        {
            "Identify",
			"identify",
            identify_activate,
            NULL,
            NULL,
        },
		{
			"Standby",
			"standby",
			standby_activate,
			NULL,
			NULL,
		},

		{ "* Purple", "purple", purple_activate, NULL, NULL, },
		{ "* Pink", "pink", pink_activate, NULL, NULL, },
		{ "* Red", "red", red_activate, NULL, NULL, },
		{ "* Green", "green", green_activate, NULL, NULL, },
		{ "* Blue", "blue", blue_activate, NULL, NULL, },
		{ "* Yellow", "yellow", yellow_activate, NULL, NULL, },
		{ "* White", "white", white_activate, NULL, NULL, },
		{
			"Blue in air, magenta otherwise",
			"blue-in-air-magenta-otherwise",
			blue_in_air_magenta_otherwise,
			NULL,
			NULL,
		},
		{
			"Only colors in air",
			"only-colors-in-air",
			only_colors_in_air_activate,
			NULL,
			NULL,
		},
		{
			"Map force to intensity",
			"map-force-to-intensity",
			map_force_to_intensity_activate,
			NULL,
			NULL,
		},
		{
			"Random light on fall",
			"random-light-on-fall",
			light_random_on_fall_activate,
			NULL,
			light_random_on_fall_packet_received,
		},
		{
			"Light up on movement",
			"light-up-on-movement",
			light_up_on_movement_activate,
			NULL,
			light_up_on_movement_packet_received,
		},
		{
			"Wild mountain thyme",
			"wild-mountain-thyme",
			wild_mountain_thyme_activate,
            wild_mountain_thyme_deactivate,
			wild_mountain_packet_received,
		},
		{
			"Charriots of Fire",
			"charriots-of-fire",
			charriots_activate,
            charriots_deactivate,
			charriots_packet_received,
		},
		{
			"The hobbit theme",
			"the-hobbit",
			hobbit_activate,
            hobbit_deactivate,
			hobbit_packet_received,
		},
		{
			"Percussion",
			"percussion",
			percussion_activate,
            percussion_deactivate,
			percussion_packet_received,
		},
		{
			"C Major",
			"c-major",
			c_major_activate,
            c_major_deactivate,
			c_major_packet_received,
		},
		{
			"Color mixer",
			"color-mixer", // Don't call through API -> will open window
			color_mixer_activate,
            NULL,
			NULL,
		},
		{
			"Fur elise",
			"fur-elise",
			fur_elise_activate,
            fur_elise_deactivate,
			fur_elise_packet_received,
		},
		{
			"The end",
			"the-end",
			the_end_activate,
			NULL,
			NULL,
		},
};


struct juggle_program_t* get_program_from_name(char* name)
{
	int i;
	for (i = 0; i < PROGRAMS_COUNT; i ++)
		if (strcmp(PROGRAMS[i].name, name) == 0)
			return & PROGRAMS[i];
	return NULL;
}


/* =============================[ Serial port ]================================= */


int serial_port_open(void)
{
	struct termios options;
	serial_port = open(PORT_NAME, O_RDWR | O_NONBLOCK);

	if (serial_port != -1)
	{
		printf("Serial Port open\n");
		tcgetattr(serial_port,&options_original);
		bzero(&options, sizeof(options));
		tcgetattr(serial_port, &options);
		cfsetispeed(&options, B9600);
		options.c_cflag |= (CLOCAL | CREAD);
		options.c_lflag |= ICANON;
		options.c_cc[VMIN] = 1;   // blocking read until 1 char received
//		options.c_cflag &= ~ CSTOPB;
		options.c_cflag &= ~ PARENB;
		tcsetattr(serial_port, TCSANOW, &options);
	}
	else
		printf("Unable to open %s\n", PORT_NAME);
	return serial_port;
}

void serial_port_close(void)
{
	 tcsetattr(serial_port,TCSANOW,&options_original);
	 close(serial_port);
}


/* =============================[ Engine ]================================= */

void  sigint_handler(int sig)
{
	serial_port_close();
	exit(0);
}

bool running = true;

/* Read input from serial port and process */
void data_read_loop(void*ptr)
{
	#define READ_BUFFER_LENGTH 256
	char read_buffer[READ_BUFFER_LENGTH+1] = { 0 };

	while(running)
	{
		// Position in the buffer where the next read should be saved
		int pos = strlen(read_buffer);
		if (pos >= READ_BUFFER_LENGTH) // Ignore chunk when no \n has been found for a while...
			pos = 0;

		int count = read(serial_port, read_buffer+pos, READ_BUFFER_LENGTH-pos);

		// Terminate by zero
		read_buffer[pos+count] = '\0';

		if (count > 0)
		{
			// When a newline appears in the queue
			char * newline_pos = strstr(read_buffer, "\n");

					// TODO: this still has some trouble with two concecutive newlines

			if (newline_pos)
			{
				// Copy everything to second buffer.
				char buffer2[READ_BUFFER_LENGTH+1];
				memcpy(buffer2, read_buffer, READ_BUFFER_LENGTH+1);
				strcpy(read_buffer, newline_pos+1);
				*strstr(buffer2, "\n") = '\0';

				// Process packet
				struct juggle_packet_t packet;

				if (parse_packet(buffer2, &packet))
					process_packet(&packet);
			}

			// Flush stdout for debugging
			fflush(0);
		}
		else
		{
			// Idle
			sleep(0);
		}
	}
}

void print_status_window(void)
{
	// Print ball statusses
	int i;
	for (i = 0; i < BALL_COUNT; i++)
	{
		char buffer[256];
		snprintf(buffer, 256, "%3i %5imv     %-3s     %-3s    %5i     %5i  %5ims %8s  %s",
						i+1, 
						juggle_states[i].voltage,
						(juggle_states[i].in_free_fall ? "Yes": "No"),
						(juggle_states[i].on_table ? "Yes": "No"),
						juggle_states[i].throws,
						juggle_states[i].catches,
						juggle_states[i].ping_time,
						juggle_states[i].firmware_version,
						juggle_states[i].last_run_command 
						);
		mvwprintw(status_window, 2+i, 1, buffer);
	}

    // Print box and title
	box(status_window, 0, 0);
	wattron(status_window, COLOR_PAIR(1));
	mvwprintw(status_window, 0, 4, "Juggling balls");
	wattroff(status_window, COLOR_PAIR(1));
	mvwprintw(status_window, 1, 1, "Ball | Power | In air | On table | Throws | Catches | Ping | Firmware | Program");

	wrefresh(status_window);
}

void print_programs_window(void)
{
    // Print programs
    int i;
    for(i = 0; i < PROGRAMS_COUNT; i ++)
    {
		char buffer[256];

		if (i == programs_window_cursor_position)
		{
			snprintf(buffer, 256, "> %3i  %s", i+1, PROGRAMS[i].description);

			wattron(programs_window, COLOR_PAIR(2));
			mvwprintw(programs_window, 1+i, 1, buffer);
			wattroff(programs_window, COLOR_PAIR(2));
		}
		else
		{
			snprintf(buffer, 256, "  %3i  %s", i+1, PROGRAMS[i].description);
			mvwprintw(programs_window, 1+i, 1, buffer);
		}
    }

    // Print box and title
	box(programs_window, 0, 0);
	wattron(programs_window, COLOR_PAIR(1));
	mvwprintw(programs_window, 0, 4, "Programs");
	wattroff(programs_window, COLOR_PAIR(1));

    wrefresh(programs_window);
}

void simulate_throw(int ball)
{
	struct juggle_packet_t dummy_throw_packet;
	dummy_throw_packet.ball = ball;
	strcpy(dummy_throw_packet.action, "THROWN");
	dummy_throw_packet.param1[0] = '\0';
	dummy_throw_packet.param2[0] = '\0';
	process_packet(&dummy_throw_packet);
}

void simulate_catch(int ball)
{
	struct juggle_packet_t dummy_caught_packet;
	dummy_caught_packet.ball = ball;
	strcpy(dummy_caught_packet.action, "CAUGHT");
	dummy_caught_packet.param1[0] = '\0';
	dummy_caught_packet.param2[0] = '\0';
	process_packet(&dummy_caught_packet);
}

int main(void)
{
	// Initialize everything
	serial_port_open();
	init_fluidsynth();
	activate_program(& PROGRAMS[0]);

	// Be sure that balls don't act as buttons yet
	send_packet("BUTTON", 0, "OFF", NULL);

	// Signal handlers
	signal(SIGINT, (void*)sigint_handler);

	// Start data read thread
	pthread_create(&data_read_thread, NULL, (void *) &data_read_loop, (void *) NULL);

	// Initialize ncurses 
	initscr();

    // Enable colors
	if(has_colors() == FALSE)
	{
		endwin();
		printf("You terminal does not support color\n");
		exit(1);
	}
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_MAGENTA);
	init_pair(2, COLOR_WHITE, COLOR_BLUE);
    print_string("Colours counted: %i", COLORS);

    // Terminal options
	raw();				        // Line buffering disabled
	keypad(stdscr, TRUE);		// We get F1, F2 etc..
	noecho();			        // Don't echo() while we do getch
    curs_set(0);                // Make cursor invisible
	timeout(0);                 // nonblocking getch
	//halfdelay(1);             // Nonblocking getch

	// Status window
	status_window = newwin(18, 94, 0, 1);

	// Serial window
	serial_window = newwin(14, 60, 18, 0);
	scrollok(serial_window, 1);
	//wsetscrreg(serial_window, 1, 14);

    // Programs window
    programs_window = newwin(31, 60, 31, 0);

	// Curses GUI loop
	while(true)
	{
		int ch = getch();
		if (ch != -1)
			print_string("key: %c (int=%i)", ch, ch);

        // Quit
		if (ch == 'q')
		{
			running = false;
			break;
		}
        // Start program
        else if (ch >= '1' && ch-'1' < PROGRAMS_COUNT)
        {
	        activate_program(& PROGRAMS[ch - '1']);
        }

		else if (ch == 259) //  key up
		{
			programs_window_cursor_position = (programs_window_cursor_position + PROGRAMS_COUNT - 1) % PROGRAMS_COUNT;
		}
		else if (ch == 258) //  key down
		{
			programs_window_cursor_position = (programs_window_cursor_position + 1) % PROGRAMS_COUNT;
		}
		else if (ch == 10) //  Enter
		{
	        activate_program(& PROGRAMS[programs_window_cursor_position]);
		}

        // THROW packet simulation
        else if (ch == 'a') simulate_throw(1);
        else if (ch == 'o') simulate_throw(2);
        else if (ch == 'e') simulate_throw(3);
        else if (ch == 'u') simulate_throw(4);
        else if (ch == 'i') simulate_throw(5);
        else if (ch == 'd') simulate_throw(6);
        else if (ch == 'h') simulate_throw(7);
        else if (ch == 't') simulate_throw(8);
        else if (ch == 'n') simulate_throw(9);
        else if (ch == 's') simulate_throw(10);

        // CAUGHT packet simulation
        else if (ch == '\'') simulate_catch(1);
        else if (ch == ',') simulate_catch(2);
        else if (ch == '.') simulate_catch(3);
        else if (ch == 'p') simulate_catch(4);
        else if (ch == 'y') simulate_catch(5);
        else if (ch == 'f') simulate_catch(6);
        else if (ch == 'g') simulate_catch(7);
        else if (ch == 'c') simulate_catch(8);
        else if (ch == 'r') simulate_catch(9);
        else if (ch == 'l') simulate_catch(10);

		// Update serial window
		while(serial_queue_tail)
		{
			// Scroll up
			wscrl(serial_window, 1);

			// Print tail from serial queue
			int y, x;
			getmaxyx(serial_window, y, x);

            char buffer[256];
            snprintf(buffer, 256, "%i %s", serial_queue_tail->line, serial_queue_tail->string);
			mvwprintw(serial_window, y-2, 1, buffer);

			pop_serial_queue_tail();
			wrefresh(serial_window);
		}

        // Refresh
        wborder(serial_window, '.', '.', ' ', ' ', ' ', ' ', ' ', ' ');
        print_programs_window();
		print_status_window();

		// Idle
		//sleep(0);
        //msleep(100);
		sleep(0);
	}

	// Clean up windows
	delwin(serial_window);
	delwin(programs_window);
	delwin(status_window);
	endwin();

	// Wait for data thread
	pthread_join(data_read_thread, NULL);
}

