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

#include <curses.h> /* ncurses interface */



#include <fluidsynth.h> /* For the MIDI synthesizer */

#define PORT_NAME "/dev/ttyUSB0"

#include "main.h"
#include "programs/charriots_of_fire.h"

/* ===============================[ Globals ]============================ */

struct termios options;
struct termios options_original;
int serial_port;


struct juggle_program_t* active_program = NULL;


pthread_t data_read_thread;
WINDOW *serial_window = NULL;
WINDOW *status_window = NULL;
WINDOW *programs_window = NULL;


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

	int len = vsnprintf(NULL, 0, format, args );
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

#define BALL_COUNT 20

struct juggle_ball_state_t
{
	double voltage;
	bool in_free_fall;
	bool on_table;
	int throws;
	int catches;
	//char current_program[256];
};

struct juggle_ball_state_t juggle_states[BALL_COUNT];


/* ===============================[ Packet processing ]============================ */

struct juggle_packet_t parse_packet(char * input)
{
	struct juggle_packet_t packet;
	packet.param1[0] = '\0';
	packet.param2[0] = '\0';

	sscanf(input, "%s %i %s %s", packet.action, &packet.ball, packet.param1, packet.param2);
	return packet;
};


void process_packet(struct juggle_packet_t* packet)
{
	int ball = packet->ball;
	if (ball >= 0 && ball <= BALL_COUNT)
	{
		// Switch juggle ball state register
		if (ball > 0)
		{
			if (strcmp(packet->action, "CAUGHT") == 0 || strcmp(packet->action, "CAUGHT") == 0)
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
		}

		// Send packet to active program
        if (active_program->packet_received)
		    active_program->packet_received(packet);
	}
}

void send_packet(char* command, int ball, char* param1, char* param2)
{
    char buffer[256];
    if (! param1) param1 = "";
    if (! param2) param2 = "";

    // Show outgoing packet on console
    snprintf(buffer, 256, "[out] %s %i %s %s\n", command, ball, param1, param2);
    print_string("%s", buffer);

    // Sent packet
    int length = snprintf(buffer, 256, "%s %i %s %s\n", command, ball, param1, param2);
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

    /* Set the synthesizer settings, if necessary */
    synth = new_fluid_synth(fluid_settings);

	/* Initialize audio driver */
    fluid_settings_setstr(fluid_settings, "audio.driver", "oss");
    adriver = new_fluid_audio_driver(fluid_settings, synth);

	/* Load sound font */
	fluid_font_id = fluid_synth_sfload(synth, "/usr/share/sounds/sf2/FluidR3_GM.sf2", 1);

	/* Test sound ;) */
	int i;
	for (i = 0; i < 2; i ++)
    {
		fluid_synth_program_select(synth, 0, fluid_font_id, 0, 7);

		fluid_synth_cc(synth, 0, 10, 120); /* 10=pan, between 0 and 127 */
		fluid_synth_cc(synth, 0, 7, 90); /* 7=volume, between 0 and 127 */
		fluid_synth_cc(synth, 0, 64, 30); /* 64=sustain */
		//fluid_synth_cc(synth, 0, 91, 100); /* 91=reverb */

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

/* **** 1: debug **** */
void test_app1_activate(void)
{
}
void test_app1_packet_received(struct juggle_packet_t* packet)
{
	print_string("Packet received: %s\n", packet->action);
}


/* *** 2: Ping *** */
void ping_activate(void)
{
    send_packet("PING", 0, NULL, NULL);
}

/* *** 3: Identify *** */
void identify_activate(void)
{
    send_packet("IDENTIFY", 0, NULL, NULL);
}

/* *** 4: Charriots of Fire *** */

// Activate program
void activate_program(struct juggle_program_t* program)
{
    if (active_program && active_program->deactivate)
	    active_program->deactivate();

	print_string("Activating program %s\n", program->description);

    if (program->activate)
	    program->activate();

	active_program = program;
}

// List of all available programs
#define PROGRAMS_COUNT 4
struct juggle_program_t PROGRAMS[] = {
		{
			"test app 1",
			test_app1_activate,
            NULL,
			test_app1_packet_received,
		},
        {
            "Ping",
            ping_activate,
            NULL,
            NULL,
        },
        {
            "Identify",
            identify_activate,
            NULL,
            NULL,
        },
		{
			"Charriots of Fire",
			charriots_activate,
            NULL,
			charriots_packet_received,
		}
};




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
		options.c_cc[VMIN] = 1;   /* blocking read until 1 char received */
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

			if (newline_pos)
			{
				// Copy everything to second buffer.
				char buffer2[READ_BUFFER_LENGTH+1];
				memcpy(buffer2, read_buffer, READ_BUFFER_LENGTH+1);
				strcpy(read_buffer, newline_pos+1);
				*strstr(buffer2, "\n") = '\0';

				// Process packet
				struct juggle_packet_t packet =  parse_packet(buffer2);
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
	box(status_window, 0, 0);

	// Print header
	wattron(status_window, COLOR_PAIR(1));
	mvwprintw(status_window, 0, 4, "Juggling balls");
	wattroff(status_window, COLOR_PAIR(1));
	mvwprintw(status_window, 1, 1, "Ball | Power | In air | On table | Throws | Catches | Program");

	// Print ball statusses
	int i;
	for (i = 0; i < BALL_COUNT; i++)
	{
		char buffer[256];
		snprintf(buffer, 256, "%3i  %5fv     %-3s     %-3s        %5i     %5i", i, 
						juggle_states[i].voltage,
						(juggle_states[i].in_free_fall ? "Yes": "No"),
						(juggle_states[i].on_table ? "Yes": "No"),
						juggle_states[i].throws,
						juggle_states[i].catches);
		mvwprintw(status_window, 2+i, 1, buffer);
	}
	wrefresh(status_window);
}

void print_programs_window(void)
{
	box(programs_window, 0, 0);

    // Print header
	wattron(programs_window, COLOR_PAIR(1));
	mvwprintw(programs_window, 0, 4, "Programs");
	wattroff(programs_window, COLOR_PAIR(1));

    // Print programs
    int i;
    for(i = 0; i < PROGRAMS_COUNT; i ++)
    {
		char buffer[256];
		snprintf(buffer, 256, "%3i  %s", i+1, PROGRAMS[i].description);
		mvwprintw(programs_window, 2+i, 1, buffer);
    }
    wrefresh(programs_window);
}

int main(void)
{
	/* Initialize everything */
	serial_port_open();
	init_fluidsynth();
	activate_program(& PROGRAMS[1]);

	/* Signal handlers */
	signal (SIGINT, (void*)sigint_handler);

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
    print_string("Colours counted: %i", COLORS);

    // Terminal options
	raw();				        // Line buffering disabled
	keypad(stdscr, TRUE);		// We get F1, F2 etc..
	noecho();			        // Don't echo() while we do getch
    curs_set(0);                // Make cursor invisible
	timeout(0);                 // nonblocking getch
	//halfdelay(1);             // Nonblocking getch

	// Status window
	status_window = newwin(20, 78, 1, 1);

	// Serial window
	serial_window = newwin(15, 60, 22, 0);
	scrollok(serial_window, 1);
	//wsetscrreg(serial_window, 1, 14);

    // Programs window
    programs_window = newwin(10, 60, 40, 0);

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

        // THROW packet simulation
        else if (ch == 't')
        {
            struct juggle_packet_t dummy_throw_packet;
            dummy_throw_packet.ball = 1;
            strcpy(dummy_throw_packet.action, "THROWN");
            dummy_throw_packet.param1[0] = '\0';
            dummy_throw_packet.param2[0] = '\0';
            process_packet(&dummy_throw_packet);
        }
        // CAUGHT packet simulation
        else if (ch == 'c')
        {
            struct juggle_packet_t dummy_caught_packet;
            dummy_caught_packet.ball = 1;
            strcpy(dummy_caught_packet.action, "CAUGHT");
            dummy_caught_packet.param1[0] = '\0';
            dummy_caught_packet.param2[0] = '\0';
            process_packet(&dummy_caught_packet);
        }

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
        wborder(serial_window, '|', '|', ' ', ' ', ' ', ' ', ' ', ' ');
        print_programs_window();
		print_status_window();

		// Idle
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

