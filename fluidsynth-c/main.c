
#include <signal.h>
#include <stdio.h> /* Standard input/output definitions */
#include <string.h> /* String function definitions */
#include <unistd.h> /* UNIX standard function definitions */
#include <fcntl.h> /* File control definitions */
#include <errno.h> /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */

#define PORT_NAME "/dev/ttyUSB1"


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


/* ===============================[ Programs ]============================ */


/* **** 1 **** */
void test_app1_setup(void)
{
	printf("Setting up app1");
}
void test_app1_packet_received(struct juggle_packet_t* packet)
{
	printf("Packet received: %s\n", packet->action);
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
			"test app 2",
			test_app1_setup,
			test_app1_packet_received,
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
	serial_port_open();
	activate_program(& PROGRAMS[0]);


	/* Read from serial port */
		#define MAX_COMMAND_LENGTH 50
		char read_buffer[MAX_COMMAND_LENGTH+1] = { 0 };

	int i;
	while(1)
	{
		int count = read(serial_port, read_buffer, MAX_COMMAND_LENGTH);
		//printf("--%i\n", count);

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
			//printf("nothing\n");
			sleep(0);
		}
	}

	signal (SIGINT, (void*)sigint_handler);

	while (1)
	{

		if (read(serial_port, read_buffer, MAX_COMMAND_LENGTH) > 0)
		{
			printf("%s\n", read_buffer);
}	
	}


}
