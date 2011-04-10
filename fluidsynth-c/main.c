
#include <signal.h>
#include <stdio.h> /* Standard input/output definitions */
#include <string.h> /* String function definitions */
#include <unistd.h> /* UNIX standard function definitions */
#include <fcntl.h> /* File control definitions */
#include <errno.h> /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */


#include <fluidsynth.h> /* For the MIDI synthesizer */

#define PORT_NAME "/dev/ttyUSB1"


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
	char action[256];
	int ball;
	char param1[256];
	char param2[256];
};


/* Program meta information */
struct juggle_program_t
{
	char description[256];
	void (*setup)(void);
	void (*packet_received)(struct juggle_packet_t* packet);
};

struct juggle_program_t* active_program;



/* ===============================[ Xbee interface ]============================ */



struct juggle_packet_t parse_packet(char * input)
{
	struct juggle_packet_t packet;
	sscanf(input, "%s %i %s %s", packet.action, &packet.ball, packet.param1, packet.param2);
	return packet;
}

void process_packet(struct juggle_packet_t* packet)
{
	active_program->packet_received(packet);
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
	for (i = 0; i < 3; i ++){
		fluid_synth_program_select(synth, 0, fluid_font_id, 0, 7);

		fluid_synth_cc(synth, 0, 10, 120); /* 10=pan, between 0 and 127 */
		fluid_synth_cc(synth, 0, 7, 90); /* 7=volume, between 0 and 127 */
		fluid_synth_cc(synth, 0, 64, 30); /* 64=sustain */
		//fluid_synth_cc(synth, 0, 91, 100); /* 91=reverb */

		fluid_synth_noteon(synth, 1, D_FLAT__Db3, 100);
		usleep(100 * 1000);
		fluid_synth_noteoff(synth, 1, D_FLAT__Db3);
		usleep(400 * 1000);
	}

}

void cleanup_fluidsynth(void)
{
	delete_fluid_synth(synth);
	delete_fluid_settings(fluid_settings);
}


/* ===============================[ Programs ]============================ */


/* **** 1: debug **** */
void test_app1_setup(void)
{
	printf("Setting up app1\n");
}
void test_app1_packet_received(struct juggle_packet_t* packet)
{
	printf("Packet received: %s\n", packet->action);
}


/* *** 2: Charriots of Fire *** */

void charriots_setup(void)
{
	printf("Setting up Charriots of Fire\n");
}
void charriots_packet_received(struct juggle_packet_t* packet)
{
	printf("Packet received: %s\n", packet->action);

	//printf("action=%s\n", packet->action);
	if (strcmp(packet->action, "CAUGHT") == 0)
		fluid_synth_noteoff(synth, 0, D_FLAT__Db3);

	else if (strcmp(packet->action, "CAUGHT*") == 0)
		fluid_synth_noteoff(synth, 0, D_FLAT__Db3);

	else if (strcmp(packet->action, "THROWN") == 0)
		fluid_synth_noteon(synth, 0, D_FLAT__Db3, 100);
}




// Activate program
void activate_program(struct juggle_program_t* program)
{
	program->setup();
	printf("Activating program ");
	printf("%s", program->description);
	printf("\n");
	active_program = program;
}

// List of all available programs
struct juggle_program_t PROGRAMS[] = {
		{
			"test app 1",
			test_app1_setup,
			test_app1_packet_received,
		},
		{
			"Charriots of Fire",
			charriots_setup,
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
		cfsetospeed(&options, B9600);
		options.c_cflag |= (CLOCAL | CREAD);
		options.c_lflag |= ICANON;
//		options.c_cflag &= ~ CSTOPB;
		options.c_cflag &= ~ PARENB;
		tcsetattr(serial_port, TCSANOW, &options);
	}
	else
		printf("Unable to open /dev/ttyUSB0\n");
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

int main()
{
	/* Initialize everything */
	serial_port_open();
	init_fluidsynth();
	activate_program(& PROGRAMS[1]);

	/* Signal handlers */
	signal (SIGINT, (void*)sigint_handler);


	/* Read from serial port */
	#define MAX_COMMAND_LENGTH 50
	char read_buffer[MAX_COMMAND_LENGTH+1] = { 0 };

	/* Data in read loop */
	int i;
	while(1)
	{
		int count = read(serial_port, read_buffer, MAX_COMMAND_LENGTH);

		if (count > 0)
		{
			// Terminate by zero
			read_buffer[count] = '\0';

			// Process incoming data
			struct juggle_packet_t packet =  parse_packet(read_buffer);
			process_packet(&packet);

			// Flush stdout for debugging
			fflush(0);
		}
		else
		{
			//printf("Idle...\n");
			sleep(0);
		}
	}
}
