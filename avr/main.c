/*********************************************
* vim: set sw=8 ts=8 si :
* Author: Jonathan Slenders
* Chip type           : ATMEGA168
* Clock frequency     : Internal clock 8 Mhz
*********************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


// ******** __ Ball config __ *******

#define BALL_ID 1 // SHOULD BE BETWEEN 1 and 255
#define BALL_ID_STR "1"

// ******** __ end of ball config __ *******


// *** XBee Signal Strength Reader
// http://www.makingthingstalk.com/chapter8/22/#more-22


// decide which clock frequency you want (you need to set the fuse
// bits for this, see Makefile) and then select the right value here. F_CPU is
// needed for the delay loop.
//#define F_CPU 1000000UL  // 1 MHz
#define F_CPU 8000000UL

#include <util/delay.h>

#define bool unsigned char
#define false 0
#define true 1

void delay_ms(unsigned int ms)
{
	while(ms)
	{
		_delay_ms(0.96);
		ms--;
	}
}

#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1

// ===========================[ Globals ]===================================

#define HISTORY_SIZE 10 // Keep 10 samples in  history
#define HISTORY_SKIP 50 // Store 1, every 100 samples
volatile unsigned char __x_history[HISTORY_SIZE];
volatile unsigned char __y_history[HISTORY_SIZE];
volatile unsigned char __z_history[HISTORY_SIZE];
volatile int __history_index = 0; // Position where the following entry should be saved.
volatile int __history_counter = 0; // Count from 0 to HISTORY_SKIP

// Free fall
volatile unsigned int __last_free_fall_duration = 0;
volatile unsigned int __free_fall_duration = 0;
volatile unsigned int __time_since_last_catch = 0;
volatile bool __in_free_fall = false;

// Moving / on table. We consider the ball to be still on a table when there
// has not been measured any significant difference in accelleration during the
// last X samples and when we are not in free fall.
#define ON_TABLE_TRESHOLD 2 // Required minimal diffence to be considered a movement.

volatile int32_t __on_table_counter = 0; // Counter 0..x for counting how long the ball already is on the table.
volatile bool __is_on_table = false;

volatile unsigned char __last_measurement_x = 0;
volatile unsigned char __last_measurement_y = 0;
volatile unsigned char __last_measurement_z = 0;

// Function pointers to the current light-program.
// Every effect should implement these callbacks.
void dummy_callback(void) { }
volatile void (*__enter_hand_callback)(bool) = &dummy_callback;
volatile void (*__leave_hand_callback)(void) = &dummy_callback;
volatile void (*__timer_tick_callback)(void) = &dummy_callback;
	// The enter-callback gets a boolean parameter: it's true when we can
	// consider the throw high enough for not being noise.


// Accellerometer center values (measured in free fall.)
#define Z_CENTER 120
#define Y_CENTER 128
#define X_CENTER 128


// ===========================[ USART ]===================================

inline void usart_init(unsigned int ubrr)
{
	// Set baud rate
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;

	// Enable receiver and transmitter
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);

	// Set frame format: 8data, 2stop bit
	UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

void usart_transmit( unsigned char data)
{
	// Wait for empty transmit buffer
	while ( !( UCSR0A & (1<<UDRE0)) )
		;

	// Put data into buffer, sends the data
	UDR0 = data;
}

bool usart_receive_ready()
{
	// Wait for data to be received
	if ( !(UCSR0A & (1<<RXC0)) )
		return false;
	else
		return true;
}


unsigned char usart_receive( void )
{
	// Wait for data to be received
	while ( !(UCSR0A & (1<<RXC0)) )
		;

	// Get and return received data from buffer
	return UDR0;
}

void usart_send_byte(unsigned char c)
{
	if (c >= 10)
		usart_send_byte(c / 10);
	usart_transmit('0' + (c % 10));
}

void usart_send_int(unsigned int i)
{
	if (i >= 10)
		usart_send_byte(i / 10);
	usart_transmit('0' + (i % 10));
}


// Send null-terminated string over usart
void usart_send_string(char * s)
{
	while(*s)
	{
		usart_transmit(*s);
		s ++;
	}
}

// Send command over Xbee
void usart_send_packet(char * command, char* param1, char* param2)
{
	usart_transmit('\n');
	usart_send_string(command);
	usart_transmit(' ');
	usart_send_string(BALL_ID_STR);
	if (param1)
	{
		usart_transmit(' ');
		usart_send_string(param1);
	}
	if (param2)
	{
		usart_transmit(' ');
		usart_send_string(param2);
	}
	usart_transmit('\n');
}


// ===========================[ ADC ]===================================

inline unsigned char do_adc_conversion()
{
	ADCSRA |= _BV(ADSC); // Start conversion
	while bit_is_set(ADCSRA, ADSC) { } // Wait for result
	return ADCH;
}

inline unsigned char get_z_accelero()
{
	// z -axis
	ADMUX = _BV(REFS0) | _BV(ADLAR) | 0; // Read first channel
	return do_adc_conversion();
}

inline unsigned char get_y_accelero()
{
	// y -axis
	ADMUX = _BV(REFS0) | _BV(ADLAR) | 1; // Read second channel
	return do_adc_conversion();
}


inline unsigned char get_x_accelero()
{
	// x -axis
	ADMUX = _BV(REFS0) | _BV(ADLAR) | 2; // Read third channel
	return do_adc_conversion();
}

inline bool adc_main_loop()
{
	// z-axis is in free fall, when it's value floats around 120
	// y-axis is in free fall, when it's value floats around 128
	// x-axis is in free fall, when it's value floats around 128
	#define PRECISION 12

	#define Z_MIN (Z_CENTER - PRECISION)
	#define Z_MAX (Z_CENTER + PRECISION)
	#define Y_MIN (Y_CENTER - PRECISION)
	#define Y_MAX (Y_CENTER + PRECISION)
	#define X_MIN (X_CENTER - PRECISION)
	#define X_MAX (X_CENTER + PRECISION)

	// Read values from accelero
	unsigned char z = get_z_accelero();
	unsigned char y = get_y_accelero();
	unsigned char x = get_x_accelero();

	// Do we have a significant difference compared to two history windows ago.
	int index = (__history_index + HISTORY_SIZE - 4) % HISTORY_SIZE;
	bool on_table = (
				// TODO NOTE: overflow may be possible but should be exceptional.
		x < __x_history[index] + ON_TABLE_TRESHOLD &&
		x > __x_history[index] - ON_TABLE_TRESHOLD &&

		y < __y_history[index] + ON_TABLE_TRESHOLD &&
		y > __y_history[index] - ON_TABLE_TRESHOLD &&

		z < __z_history[index] + ON_TABLE_TRESHOLD &&
		z > __z_history[index] - ON_TABLE_TRESHOLD);

	if (on_table)
		__on_table_counter ++;
	else
		__on_table_counter = 0;

	if (__is_on_table)
	{
		// We are on the table. Any significant movement will cause the ball
		// not to be considered on the table.
		if (! on_table)
		{
			usart_send_packet("MOVING", NULL, NULL);
			__is_on_table = false;
		}
	}
	else
	{
		// We are moving. Need to have X times on table.
		if (__on_table_counter > 2000)
		{
			usart_send_packet("ON_TABLE", NULL, NULL);
			__is_on_table = true;
		}
	}

	// TODO: calculate impact. (pytagoras distance to center.)
	//int delta_x = (x > X_CENTER ? x - X_CENTER : X_CENTER - x);
	//int delta_y = (y > Y_CENTER ? y - Y_CENTER : Y_CENTER - y);
	//int delta_z = (z > Z_CENTER ? z - Z_CENTER : Z_CENTER - z);

	// Calculate: impact*impact
	//int impact_2 = (delta_x * delta_x + delta_y * delta_y + delta_z * delta_z);

	//return (delta_x < PRECISION && delta_y < PRECISION & delta_z < PRECISION);


	// Save last measurement
	__last_measurement_x = x;
	__last_measurement_y = y;
	__last_measurement_z = z;

	// Save history
	if (__history_counter == HISTORY_SKIP)
	{
		__x_history[__history_index] = x;
		__y_history[__history_index] = y;
		__z_history[__history_index] = z;

		// Reset counter, and go to next index;
		__history_index = (__history_index + 1) % HISTORY_SIZE;
		__history_counter = 0;
	}
	else
		__history_counter ++;

	// In free fall? (All values within boundaries.)
	bool in_free_fall = (
			z > Z_MIN && z < Z_MAX &&
			y > Y_MIN && y < Y_MAX &&
			x > X_MIN && x < X_MAX);

	if (in_free_fall)
	{
		if (! __in_free_fall)
		{
			__in_free_fall = true;
			leave_hand();
		}

		// Free fall doesn't count as being on the table. Even if this
		// is measured as no change in applied force. Reset counter to avoid
		// on-table to be triggered.
		__on_table_counter = 0;
	}
	else
	{
		if (__in_free_fall)
		{
			__in_free_fall = false;
			enter_hand();
		}
	}
}


// ===========================[ Event handlers ]===================================

#define DISTANCE(a, b) ( (a < b) ? (b - a) : (a - b) )

void leave_hand()
{
	// Reset timer
	__free_fall_duration = 0;

	// Calculate leave 'force'.
	uint32_t force = 0;

	int index = (__history_index + HISTORY_SIZE - 1) % HISTORY_SIZE;
	int index2 = (__history_index + HISTORY_SIZE - 2) % HISTORY_SIZE;

	force += DISTANCE(__x_history[index], X_CENTER);
	force += DISTANCE(__y_history[index], Y_CENTER);
	force += DISTANCE(__z_history[index], Z_CENTER);

	force += DISTANCE(__x_history[index2], X_CENTER);
	force += DISTANCE(__y_history[index2], Y_CENTER);
	force += DISTANCE(__z_history[index2], Z_CENTER);

	force += DISTANCE(__last_measurement_x, X_CENTER);
	force += DISTANCE(__last_measurement_y, Y_CENTER);
	force += DISTANCE(__last_measurement_z, Z_CENTER);

	char buffer[32];
	snprintf(buffer, 8, "%u", force);

	// USART Feedback
	usart_send_packet("THROWN", buffer, NULL);


	// Program callback
	__leave_hand_callback();
}

void enter_hand()
{
	char buffer[8];
	snprintf(buffer, 8, "%u", __free_fall_duration);

	// Any decent flight should take more than 60 counts.
	// Ignore others. (Probably some noise from holding the balls
	// in your hand.)
	if (__free_fall_duration > 10)
	{
		// USART Feedback
		usart_send_packet("CAUGHT", buffer, NULL);

		// Remember duration of flight.
		__last_free_fall_duration = __free_fall_duration;
		__time_since_last_catch = 0;

		// Program callback
		__enter_hand_callback(true);
	}
	else
	{
		// USART Feedback (maybe we should disable this packets, to much polution in the air...)
		usart_send_packet("CAUGHT*", buffer, NULL);

		// Program callback
		__enter_hand_callback(false);
	}
}

// Interrupt for measuring the flight duration
ISR(TIMER2_COMPB_vect)
{
	// Increase counter
	__free_fall_duration += 1;
	__time_since_last_catch += 1;

	// Program callback
	__timer_tick_callback();
}


// ===========================[ PWM lookup tables ]===================================

// >>> for x in range(0, 32):
// ...    print str(int(1.430**x)) + ', //', x

// mapping from 0..32  to 0..65536
int lookup_table_16_bit [] = {
	0, // 0
	1, // 1
	2, // 2
	2, // 3
	4, // 4
	5, // 5
	8, // 6
	12, // 7
	17, // 8
	25, // 9
	35, // 10
	51, // 11
	73, // 12
	104, // 13
	149, // 14
	213, // 15
	305, // 16
	437, // 17
	625, // 18
	894, // 19
	1278, // 20
	1828, // 21
	2614, // 22
	3738, // 23
	5346, // 24
	7645, // 25
	10932, // 26
	15634, // 27
	22356, // 28
	31970, // 29
	45717, // 30
	65375, // 31
};

// >>> for x in range(0,16):
// ...   print str(int(1.447**x)) + ', //', x

// mapping from 0..16  to 0..256
int lookup_table_8_bit [] = {
	255-0, // 0
	255-1, // 1
	255-2, // 2
	255-3, // 3
	255-4, // 4
	255-6, // 5
	255-9, // 6
	255-13, // 7
	255-19, // 8
	255-27, // 9
	255-40, // 10
	255-58, // 11
	255-84, // 12
	255-121, // 13
	255-176, // 14
	255-255, // 15
};



// ===========================[ Utilities ]===================================

#define LED_ON_VALUE 255
#define LED_OFF_VALUE 0

#define BLUE_LED OCR2A
#define RED_LED OCR1B
#define GREEN_LED OCR1A

inline void green_led_on() { GREEN_LED = lookup_table_8_bit[15]; }
inline void blue_led_on() { BLUE_LED = lookup_table_8_bit[15]; }
inline void red_led_on() { RED_LED = lookup_table_8_bit[15]; }

inline void green_led_off() { GREEN_LED = lookup_table_8_bit[0]; }
inline void blue_led_off() { BLUE_LED = lookup_table_8_bit[0]; }
inline void red_led_off() { RED_LED = lookup_table_8_bit[0]; }

void all_leds_off()
{
	green_led_off();;
	blue_led_off();
	red_led_off();
}

void all_leds_on()
{
	green_led_on();;
	blue_led_on();
	red_led_on();
}

	// intensity = 0 .. 15
void green_led_intensity(unsigned char intensity)
{
	GREEN_LED = lookup_table_8_bit[intensity];
}
void blue_led_intensity(unsigned char intensity)
{
	BLUE_LED = lookup_table_8_bit[intensity];
}
void red_led_intensity(unsigned char intensity)
{
	RED_LED = lookup_table_8_bit[intensity];
}


	// percentage = 0 .. 255
void green_led_percentage(unsigned char percentage)
{
	green_led_intensity(percentage / 16);
}
void blue_led_percentage(unsigned char percentage)
{
	blue_led_intensity(percentage / 16);
}
void red_led_percentage(unsigned char percentage)
{
	red_led_intensity(percentage / 16);
}

void all_leds_percentage(unsigned char percentage)
{
	red_led_intensity(percentage);
	green_led_intensity(percentage);
	blue_led_intensity(percentage);
}


// ===========================[ Programs ]===================================

// utility for converting a hex_to_int char to an integer
unsigned char hex_to_int(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return 0;
}


// -0- fixed color
void pr_fixed_color_install(char * param)
{
	__enter_hand_callback = &dummy_callback;
	__leave_hand_callback = &dummy_callback;
	__timer_tick_callback = &dummy_callback;

	// Param is supposed to be a hex_to_int color, like FF0044
	unsigned char red = hex_to_int(param[0]) * 16 + hex_to_int(param[1]);
	unsigned char green = hex_to_int(param[2]) * 16 + hex_to_int(param[3]);
	unsigned char blue = hex_to_int(param[4]) * 16 + hex_to_int(param[5]);

	red_led_percentage(red);
	green_led_percentage(green);
	blue_led_percentage(blue);
}


// -1- Alternate every throw between red, green and blue

int __pr_alternate = 0;

void pr_alternate_enter(bool was_throw)
{
	// DIM leds
	if (__pr_alternate == 0)
		green_led_intensity(1);
	else if (__pr_alternate == 1)
		blue_led_intensity(1);
	else
		red_led_intensity(1);

	if (was_throw)
		__pr_alternate = (__pr_alternate + 1) % 3;
}

void pr_alternate_leave()
{
	all_leds_off();
	if (__pr_alternate == 0)
		green_led_on();
	else if (__pr_alternate == 1)
		blue_led_on();
	else
		red_led_on();
}

void pr_alternate_install(char* param)
{
	__enter_hand_callback = &pr_alternate_enter;
	__leave_hand_callback = &pr_alternate_leave;
	__timer_tick_callback = &dummy_callback;
}

// -2- Interpolate from green to blue during a throw
//     (not completely tested, but algorithm should work.)

void pr_interpolate_tick_callback()
{
	if (__in_free_fall)
	{
		if (__free_fall_duration > __last_free_fall_duration /2)
		{
			green_led_on();
			blue_led_off();
		}
		else
		{
			blue_led_on();
			green_led_off();
		}
	}
	else
		all_leds_off();
}

void pr_interpolate_install(char* param)
{
	__enter_hand_callback = &dummy_callback;
	__leave_hand_callback = &dummy_callback;
	__timer_tick_callback = pr_interpolate_tick_callback;
}


// -3- invisible up, comes down in blue.

int pr_rain_drop_timer = 0;

void pr_rain_enter(bool was_throw)
{
	if (was_throw)
	{
		all_leds_on();
		pr_rain_drop_timer = __last_free_fall_duration / 10 + 1;
	}
}

void pr_rain_leave()
{
	all_leds_off();
}

void pr_rain_tick_callback()
{
	if (__in_free_fall)
	{
		unsigned int halfway = __last_free_fall_duration/2;

		if (__free_fall_duration > halfway)
		{
			unsigned int percentage;
			if (__free_fall_duration > __last_free_fall_duration)
				percentage = 0;
			else
				percentage = (unsigned char) ((__free_fall_duration - halfway) *255 / halfway);

			if (__pr_alternate == 0)
				green_led_percentage(percentage);
			else if (__pr_alternate == 1)
				blue_led_percentage(percentage);
			else
				red_led_percentage(percentage);
		}
	}
	else
	{
		if (__time_since_last_catch < pr_rain_drop_timer)
		{
			unsigned int percentage = 255 - __time_since_last_catch * 255 / pr_rain_drop_timer;
			all_leds_percentage(percentage);
		}
		else
		{
			all_leds_off();
		}
	}
}

void pr_rain_install(char* param)
{
	__enter_hand_callback = &pr_rain_enter;
	__leave_hand_callback = &pr_rain_leave;
	__timer_tick_callback = &pr_rain_tick_callback;
}


// -4- table test

void pr_table_test_tick_callback()
{
	if (__is_on_table)
	{
		blue_led_off();
		green_led_off();
		red_led_on();
	}
	else if (__in_free_fall)
	{
		blue_led_on();
		green_led_off();
		red_led_off();
	}
	else
	{
		blue_led_off();
		green_led_on();
		red_led_off();
	}
}

void pr_table_test_install(char* param)
{
	__enter_hand_callback = &dummy_callback;
	__leave_hand_callback = &dummy_callback;
	__timer_tick_callback = &pr_table_test_tick_callback;
}

// List of installed programs

#define PROGRAM_COUNT 5
const char* program_names[] = {
	"fixed",
	"alternate",
	"rain",
	"interpolate",
	"tabletest",
 };

void (*program_pointers [])(char*) = {
	&pr_fixed_color_install,
	&pr_alternate_install,
	&pr_rain_install,
	&pr_interpolate_install,
	&pr_table_test_install,
};



// ===========================[ Input data parser ]===================================


void process_command(char* action, char* ball, char* input_param, char* input_param2)
{
	int i;

	if (strcmp(ball, "0") == 0 || strcmp(ball, BALL_ID_STR) == 0)
	{
		// RUN: Run program
		if (strcmp(action, "RUN") == 0)
		{
			for (i = 0; i < PROGRAM_COUNT; i ++)
				if (strcmp(input_param, program_names[i]) == 0)
					program_pointers[i](input_param2);
		}

		// PING: answer with PONG
		else if (strcmp(action, "PING") == 0)
		{
			usart_send_packet("PONG", NULL, NULL);
		}

		// IDENTIFY: blink leds white for 2 seconds, while ignoring everything else.
		else if (strcmp(action, "IDENTIFY") == 0)
		{
			// Disable all interrupts
			cli();

			// TODO: read current led intensity

			for (i = 0; i < 10; i ++)
			{
				all_leds_on();
				_delay_ms(50);
				all_leds_off();
				_delay_ms(50);
			}

			// TODO: restore current led intensity

			// Enable all interrupts again
			sei();
		}
	}
}


// Protocol definition:
//    action  :=   [a-z]+
//    ball    :=   [0-9]+
//    param   :=   [a-z0-9]+
//    frame   :=  <action>  ' ' <ball> ' ' <param>


#define max_input_length 32
char input_action[max_input_length]; // max length 
char input_ball[max_input_length]; // max length 
char input_param1[max_input_length]; // max length 
char input_param2[max_input_length]; // max length 

volatile int input_pos = 0;
volatile char * input_part = NULL;

void initialize_input()
{
	input_pos = 0;
	*input_action = 0;
	*input_ball = 0;
	*input_param1 = 0;
	*input_param2 = 0;
	input_part = input_action;
}

void parse_input_char(char c)
{
	// Newline as new command
	if (c == '\n' || c == '\r')
	{
		input_part[input_pos] = '\0'; // terminate command by zero
		process_command(input_action, input_ball, input_param1, input_param2);

		// Initialize
		initialize_input();
	}
	// Space as separator
	else if (c == ' ')
	{
		input_part[input_pos] = '\0'; // terminate command by zero
		input_pos = 0;

		if (input_part == input_action)
			input_part = input_ball;
		else if (input_part == input_ball)
			input_part = input_param1;
		else if (input_part == input_param1)
			input_part = input_param2;
	}
	else
	{
		// Save input char
		input_part[input_pos] = c;
		input_pos += 1;

		// Avoid buffer overflow
		if (input_pos >= max_input_length - 1)
			initialize_input();
	}
}





// ===========================[ Main loop ]===================================

// Extension for 8-bit timer
/*
int pwm3_value = 4048; 

int overflow_2;
ISR(overflow_timer2)
{
	overflow_2 ++;
	if (overflow_2 < pwm3_value)
		OCR2A = 0;
	else if (overflow_2 > pwm3_value & 0xff00 + 0x0100)
		OCR2A = 0xff;
	else
		OCR2A = (char)pwm3_value; // lowest byte only
}
*/





int main(void)
{
	// Initialise usart
	usart_init(MYUBRR);

	// Send booting packet
	usart_send_packet("BOOTING", NULL, NULL);

	// Set up PWM

	// Timer 1 (16bit)
	DDRB |= _BV(PB1) | _BV(PB2); // (PWM led dimmers)
	TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM10);
	TCCR1B = _BV(WGM12) | _BV(CS12);
			// Fast PWM / 8 bit
			// Clear on compare (both OC1A and OC1B)
			// 256 prescaling

	// Timer 2 (8bit)
	DDRB |= _BV(PB3); // OC2A (PWM led dimmer)
	DDRD |= _BV(PD3); // OC2B (output for debugging)
	TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM01) | _BV(WGM00);
	TCCR2B = _BV(CS22) | _BV(CS21);
			// Fast PWM / 8 bit 
			// Clear on compare (both OC2A and OC2B)
			// 256 prescaling

	// Use OCR2B for the timer interrupt
	TIMSK2 = _BV(OCIE2B); // Output compare interrupt enable 2B
	OCR2B = 128; // For the interrupts
	OCR2A = 20;

	// Enable ADC

	// The Power Reduction ADC bit, PRADC, in .Minimizing Power Consumption. on page 40 must
	// be disabled by writing a logical zero to enable the ADC.
	// The ADC converts an analog input voltage to a 10-bit digital
	ADMUX = _BV(REFS0) | _BV(ADLAR);
	ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS0);
	ADCSRA |= _BV(ADSC); // Start dummy conversion.
		// Avcc reference voltage, with external capacitor between Aref and GND.
		// 32 prescaling
		// ADC left adjust result.

	get_x_accelero();
	initialize_input();

	// Enable global interrupts
	sei();

	// Test leds
	blue_led_on();
	green_led_on();
	_delay_ms(800);


	// Main program loop

	pr_alternate_install("");
	//pr_fixed_color_install("080000");
//	pr_interpolate_install();
//	pr_rain_install();

	// Take 1000 samples for filling in a little history.
	int i;
	for(i = 0; i < 1000; i ++)
		adc_main_loop();

	// Send booted packet
	usart_send_packet("BOOTED", NULL, NULL);

	// Main loop
	while (1)
	{
		// Parse incoming data
		while (usart_receive_ready())
			parse_input_char(usart_receive());

		// Juggle ball program loop
		adc_main_loop();
	}

	// Test code: dimmer example loop
	all_leds_off();
	while(1)
	{
		//for (i = 10; i < 256; i ++)
		for (i = 0; i < 16; i ++)
		{
			OCR1B = lookup_table_8_bit[i];

			//OCR1B = i;
			delay_ms(80);
		}
			delay_ms(80);
	}

	return 0;
}
