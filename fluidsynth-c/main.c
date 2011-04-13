#define ENABLE_CURSES 1

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

#ifdef ENABLE_CURSES
#include <curses.h> /* ncurses interface */
#endif


#include <fluidsynth.h> /* For the MIDI synthesizer */

#define PORT_NAME "/dev/ttyUSB0"


#define D_FLAT__Db3 49

#define D_FLAT__C4 60
#define D_FLAT__Db4 61
#define D_FLAT__Eb4 63
#define D_FLAT__F4 65
#define D_FLAT__Gb4 66
#define D_FLAT__Ab4 68
#define D_FLAT__Bb4 70


/* ===============================[ Globals ]============================ */

struct termios options;
struct termios options_original;
int serial_port;


/* Juggle data packet */
struct juggle_packet_t
{
	int ball; // Zero means addressing all balls, otherwise ball number
	char action[256];
	char param1[256];
	char param2[256];
};


/* Program meta information */
struct juggle_program_t
{
	char description[256];
	void (*activate)(void);
	void (*packet_received)(struct juggle_packet_t* packet);
};

struct juggle_program_t* active_program;


#ifdef ENABLE_CURSES
pthread_t data_read_thread;
WINDOW *serial_window = NULL;
WINDOW *status_window = NULL;
#endif

/* ===============================[ Method signatures ]============================ */

void print(char *data);
// ...


/* ===============================[ Serial FIFO queue ]============================ */

/*
 * We need to queue all serial data to be printed in the User interface.
 * -> needs to be handled in the main/GUI thread.
 */

pthread_mutex_t serial_queue_access_mutex = PTHREAD_MUTEX_INITIALIZER;

struct serial_queue_t
{
	char* string;
	struct serial_queue_t* next; // TODO: I don't think we actually need the next pointer
	struct serial_queue_t* prev;
};

struct serial_queue_t * serial_queue_head = NULL;
struct serial_queue_t * serial_queue_tail = NULL;

void print_string(const char* format, ...) __attribute__ ((format (printf, 1, 2)));

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
		active_program->packet_received(packet);
	}
}


/* ===============================[ Fluid synth ]============================ */


fluid_settings_t* fluid_settings;
fluid_synth_t* synth;
fluid_audio_driver_t* adriver;
int fluid_font_id;

void init_fluidsynth()
{
	fluid_settings = new_fluid_settings();

    /* Set the synthesizer settings, if necessary */
    synth = new_fluid_synth(fluid_settings);

	/* Initialize audio driver */
    fluid_settings_setstr(fluid_settings, "audio.driver", "alsa");
    adriver = new_fluid_audio_driver(fluid_settings, synth);

	/* Load sound font */
	fluid_font_id = fluid_synth_sfload(synth, "/usr/share/sounds/sf2/FluidR3_GM.sf2", 1);

	/* Test sound ;) */
	int i;
	for (i = 0; i < 2; i ++){
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


/* *** 2: Charriots of Fire *** */

void charriots_activate(void)
{
	fluid_synth_program_select(synth, 0, fluid_font_id, 0, 7);
	//fluid_synth_program_select(synth, 0, fluid_font_id, 0, 18); // accordeon/organ
}
void charriots_packet_received(struct juggle_packet_t* packet)
{
	print_string("Packet received: %i %s %s %s\n", packet->ball, packet->action, packet->param1, packet->param2);

/*
	//printf("action=%s\n", packet->action);
	if (strcmp(packet->action, "CAUGHT") == 0)
		fluid_synth_noteoff(synth, 0, D_FLAT__Db3);

	else if (strcmp(packet->action, "CAUGHT*") == 0)
		fluid_synth_noteoff(synth, 0, D_FLAT__Db3);

	else if (strcmp(packet->action, "THROWN") == 0)
		fluid_synth_noteon(synth, 0, D_FLAT__Db3, 100);
*/

	if (strcmp(packet->action, "CAUGHT") == 0)
		fluid_synth_noteon(synth, 0, D_FLAT__F4, 100);
}




// Activate program
void activate_program(struct juggle_program_t* program)
{
	program->activate();
	print_string("Activating program %s\n", program->description);
	active_program = program;
}

// List of all available programs
struct juggle_program_t PROGRAMS[] = {
		{
			"test app 1",
			test_app1_activate,
			test_app1_packet_received,
		},
		{
			"Charriots of Fire",
			charriots_activate,
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
void data_read_loop(void)
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

void start_data_read_thread(void *ptr)
{
	data_read_loop();
}


#ifdef ENABLE_CURSES
void print_status_window(void)
{
	// Print header
	attron(COLOR_PAIR(1));
	mvwprintw(status_window, 0, 4, "Juggling balls");
	attroff(COLOR_PAIR(1));
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

#endif



int main(void)
{
	/* Initialize everything */
	serial_port_open();
	init_fluidsynth();
	activate_program(& PROGRAMS[1]);

	/* Signal handlers */
	signal (SIGINT, (void*)sigint_handler);

#ifdef ENABLE_CURSES
	/* Ncurses initialization */
	initscr();			/* Start curses mode 		*/

	if(has_colors() == FALSE)
	{
		endwin();
		printf("You terminal does not support color\n");
		exit(1);
	}

	raw();				/* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			/* Don't echo() while we do getch */
	start_color();			/* Start color 			*/
	init_pair(1, COLOR_RED, COLOR_BLACK);
	//halfdelay(1); /* Nonblocking getch */
	timeout(0); /* nonblocking getch */

	refresh();

	// Serial window
	serial_window = newwin(20, 60, 22, 0);
	scrollok(serial_window, 1);
	//wsetscrreg(serial_window, 1, 14);
	box(serial_window, 0, 0);

	// Status window
	status_window = newwin(20, 78, 1, 1);
	box(status_window, 0, 0);

	/* Start data read thread */
	pthread_create(&data_read_thread, NULL, (void *) &start_data_read_thread, (void *) NULL);

	/* ncurses GUI loop */
	while(true)
	{
		int ch = getch();
		if (ch != -1)
			print_string("pressed key: %i", ch);

		//attron(COLOR_PAIR(1));
		//mvwprintw(serial_window, 3, 3, "test");
		//attroff(COLOR_PAIR(1));
		//wrefresh(serial_window);

		if (ch == 'q')
		{
			running = false;
			break;
		}


		/* Update serial window */
		while(serial_queue_tail)
		{
			/* Scroll one line up */
			wscrl(serial_window, 1);

			/* Print tail from queue */
			int y, x;
			getmaxyx(serial_window, y, x);

			char* string = (serial_queue_tail->string);
			mvwprintw(serial_window, y-2, 1, string);
			pop_serial_queue_tail();
			wrefresh(serial_window);
		}

		print_status_window();

		// Idle
		sleep(0);
	}

	/* Join data read thread */
	pthread_join(data_read_thread, NULL);

	/* Data in read loop */
	// data_read_loop();

	/* Exit ncurses */
	delwin(serial_window);
	endwin();
#endif
}
