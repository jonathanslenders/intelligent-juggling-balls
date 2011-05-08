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

#ifndef BALL_ID_STR
#define BALL_ID_STR "7" // SHOULD BE BETWEEN 1 and 255
#define BALL_ID 7       // SHOULD BE BETWEEN 1 and 255
#endif
#ifndef VERSION_ID
#define VERSION_ID "0.1.4" // Version number, increment after every change
#endif


// Accellerometer center values (measured in free fall.)

#if BALL_ID == 1
	// Ball 1
	#define X_CENTER 123
	#define Y_CENTER 133
	#define Z_CENTER 127
#endif

#if BALL_ID == 2
	// Ball 2
	#define X_CENTER 125
	#define Y_CENTER 133
	#define Z_CENTER 131
#endif

#if BALL_ID == 3
	// Ball 3
	#define X_CENTER 122
	#define Y_CENTER 133
	#define Z_CENTER 132
#endif

#if BALL_ID == 4
	// Ball 4
	#define X_CENTER 125
	#define Y_CENTER 132
	#define Z_CENTER 132
#endif

#if BALL_ID == 5
	// Ball 5
	#define X_CENTER 124
	#define Y_CENTER 134
	#define Z_CENTER 127
#endif

#if BALL_ID == 6
	// Ball 6
	#define X_CENTER 123
	#define Y_CENTER 133
	#define Z_CENTER 133
#endif

#if BALL_ID == 7
	// Ball 7
	#define X_CENTER 122
	#define Y_CENTER 132
	#define Z_CENTER 137
#endif

#if BALL_ID == 8
	// Ball 8
	#define X_CENTER 124
	#define Y_CENTER 134
	#define Z_CENTER 131
#endif

#if BALL_ID == 9
	// Ball 9
	#define X_CENTER 120
	#define Y_CENTER 133
	#define Z_CENTER 132
#endif

#if BALL_ID == 10
	// Ball 10
	#define X_CENTER 121
	#define Y_CENTER 128
	#define Z_CENTER 128
#endif

#if BALL_ID == 11
	// Ball 11
	#define X_CENTER 122
	#define Y_CENTER 135
	#define Z_CENTER 137
#endif

#if BALL_ID == 13
// Ball 13
	#define X_CENTER 125
	#define Y_CENTER 134
	#define Z_CENTER 133
#endif

#if BALL_ID == 14
	// Ball 14
	#define X_CENTER 121
	#define Y_CENTER 134
	#define Z_CENTER 132
#endif

#ifndef Z_CENTER
	#define Z_CENTER 128
#endif
#ifndef Y_CENTER
	#define Y_CENTER 128
#endif
#ifndef X_CENTER
	#define X_CENTER 128
#endif

// ******** __ end of ball config __ *******


// *** XBee Signal Strength Reader
// http://www.makingthingstalk.com/chapter8/22/#more-22

struct color_t {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};

// decide which clock frequency you want (you need to set the fuse
// bits for this, see Makefile) and then select the right value here. F_CPU is
// needed for the delay loop.
//#define F_CPU 1000000UL  // 1 MHz
//#define F_CPU 8000000UL
#define F_CPU 16000000UL

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

// Minimum duration before being sure that the ball really left our hand.
#define MIN_FREE_FALL_DURATION 60

#define HISTORY_SIZE 10 // Keep 10 samples in  history
#define HISTORY_SKIP 50 // Store 1, every 50 samples
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
volatile int __in_free_fall_counter = 0; // Counter 0..x for counting how many samples we are in free fall.
volatile int __not_in_free_fall_cunter = 0; // Counter 0..x for counting how many samples we are no longer in free fall.

// Moving / on table. We consider the ball to be still on a table when there
// has not been measured any significant difference in accelleration during the
// last X samples and when we are not in free fall.
#define ON_TABLE_TRESHOLD 4 // Required minimal diffence to be considered a movement.

volatile int32_t __on_table_counter = 0; // Counter 0..x for counting how many samples the ball already is on the table.
volatile int32_t __moving_counter = 0; // Counter 0..x for counting how many samples the ball already moving
volatile bool __is_on_table = false;

volatile unsigned char __last_measurement_x = 0;
volatile unsigned char __last_measurement_y = 0;
volatile unsigned char __last_measurement_z = 0;

volatile bool button_mode = false;

// Function pointers to the current light-program.
// Every effect should implement these callbacks.
void dummy_callback(void) { }
volatile void (*__enter_hand_callback)(bool) = &dummy_callback;
volatile void (*__leave_hand_callback)(void) = &dummy_callback;
volatile void (*__timer_tick_callback)(void) = &dummy_callback;
	// The enter-callback gets a boolean parameter: it's true when we can
	// consider the throw high enough for not being noise.


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

#include <stdarg.h> /* for variadic functions */

// Send null-terminated string over usart
void usart_printf(const char* format, ...) __attribute__ ((format (printf, 1, 2)));

void usart_printf(const char* format, ...)
{
	// Print into new string
	va_list args;
	va_start(args, format);

	int len = vsnprintf(NULL, 0, format, args);
	char * new_string = (char*) malloc(len + 1);
	vsnprintf(new_string, len + 1, format, args);

	va_end(args);

	// Send new string
	usart_send_string(new_string);

	free(new_string);
}


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
	ADMUX = _BV(REFS0) | _BV(ADLAR) | 2; // Read first channel
	return do_adc_conversion();
}

inline unsigned char get_y_accelero()
{
	// y -axis
	ADMUX = _BV(REFS0) | _BV(ADLAR) | 3; // Read second channel
	return do_adc_conversion();
}


inline unsigned char get_x_accelero()
{
	// x -axis
	ADMUX = _BV(REFS0) | _BV(ADLAR) | 1; // Read third channel
	return do_adc_conversion();
}

void read_voltage()
{
	// battery voltage
	ADMUX = _BV(REFS0) | _BV(ADLAR) | 6; // Read sixth channel
	unsigned char adc6 = do_adc_conversion();

	float aref = 3.6; // From voltage regulator
	int result = 1000.0 * 43.0 * adc6 * aref / (10.0 * 255);

	char buffer[256];
	sprintf(buffer, "%imV", result);
	usart_send_packet("VOLTAGE", buffer, NULL);
}

inline bool adc_main_loop()
{
	// z-axis is in free fall, when it's value floats around 120
	// y-axis is in free fall, when it's value floats around 128
	// x-axis is in free fall, when it's value floats around 128
	#define PRECISION 8

	#define Z_MIN (Z_CENTER - PRECISION)
	#define Z_MAX (Z_CENTER + PRECISION)
	#define Y_MIN (Y_CENTER - PRECISION)
	#define Y_MAX (Y_CENTER + PRECISION)
	#define X_MIN (X_CENTER - PRECISION)
	#define X_MAX (X_CENTER + PRECISION)

	// Read values from accelero
	//unsigned char z = get_z_accelero();
	//unsigned char y = get_y_accelero();
	//unsigned char x = get_x_accelero();

	unsigned char x = ((int) get_x_accelero() + (int)get_x_accelero() + (int)get_x_accelero()) / 3;
	unsigned char y = ((int) get_y_accelero() + (int)get_y_accelero() + (int)get_y_accelero()) / 3;
	unsigned char z = ((int) get_z_accelero() + (int)get_z_accelero() + (int)get_z_accelero()) / 3;

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
	{
		__on_table_counter ++;
		__moving_counter = 0;
	}
	else
	{
		__moving_counter ++;
		__on_table_counter = 0;
	}

	if (__is_on_table)
	{
		// We are on the table. Need to have X samples moving
		if (__moving_counter > 3)
		{
			usart_send_packet("MOVING", NULL, NULL);
			__is_on_table = false;
		}
	}
	else
	{
		// We are moving. Need to have X samples on table.
		if (__on_table_counter > 100)
		{
			usart_send_packet("ON_TABLE", NULL, NULL);
			__is_on_table = true;
		}
	}

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
		__in_free_fall_counter ++;
		__not_in_free_fall_cunter = 0;

		// Free fall doesn't count as being on the table. Even if this
		// is measured as no change in applied force. Reset counter to avoid
		// on-table to be triggered.
		__on_table_counter = 0;
	}
	else
	{
		__not_in_free_fall_cunter ++;
		__in_free_fall_counter = 0; 
	}

	if (__in_free_fall)
	{
		if (__not_in_free_fall_cunter > 3) // Need to have 5 samples not in free fall before being sure.
		{
			__in_free_fall = false;
			enter_hand();
		}
	}
	else
	{
		if (__in_free_fall_counter > 5) // Need to have 5 samples in free fall before being sure...
		{
			__in_free_fall = true;
			leave_hand();
		}
	}
}


// ===========================[ Event handlers ]===================================

#define DISTANCE(a, b) ( (a < b) ? (b - a) : (a - b) )

void leave_hand()
{
	// Reset timer
	__free_fall_duration = 0;

	// Calculate leave 'force'. NOTE: this parameter is probably absoluty useless and should be dropped...
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

	char buffer[8];
	snprintf(buffer, 8, "%u", force);

	// USART Feedback
	if (!button_mode)
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
	if (__free_fall_duration > MIN_FREE_FALL_DURATION)
	{
		// USART Feedback
		if (!button_mode)
			usart_send_packet("CAUGHT", buffer, NULL);
		else
			usart_send_packet("BUTTON_PRESSED", NULL, NULL);

		// Remember duration of flight.
		__last_free_fall_duration = __free_fall_duration;
		__time_since_last_catch = 0;

		// Program callback
		__enter_hand_callback(true);
	}
	else
	{
		// USART Feedback (maybe we should disable this packets, to much polution in the air...)
		if (!button_mode)
			usart_send_packet("CAUGHT*", buffer, NULL);

		// Program callback
		__enter_hand_callback(false);
	}
}

// Interrupt for measuring the flight duration
ISR(TIMER2_COMPB_vect)
{
	adc_main_loop();

	// Increase counter
	__free_fall_duration += 1;
	__time_since_last_catch += 1;

	// When we reached the min free fall duration, we can be sure that
	// the ball really left our hand.
	if (__free_fall_duration == MIN_FREE_FALL_DURATION)
		usart_send_packet("IN_FREE_FALL", NULL, NULL);

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

#define RED_LED OCR0B
#define GREEN_LED OCR1B
#define BLUE_LED OCR1A

volatile unsigned char __red_intensity = 0;
volatile unsigned char __green_intensity = 0;
volatile unsigned char __blue_intensity = 0;

inline void red_led_on() { RED_LED = lookup_table_8_bit[15]; __red_intensity = 255; }
inline void green_led_on() { GREEN_LED = lookup_table_8_bit[15]; __green_intensity = 255; }
inline void blue_led_on() { BLUE_LED = lookup_table_8_bit[15]; __blue_intensity = 255; }

inline void red_led_off() { RED_LED = lookup_table_8_bit[0]; __red_intensity = 0; }
inline void green_led_off() { GREEN_LED = lookup_table_8_bit[0]; __green_intensity = 0; }
inline void blue_led_off() { BLUE_LED = lookup_table_8_bit[0]; __blue_intensity = 0; }

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

	// percentage = 0 .. 255
void red_led_percentage(unsigned char percentage)
{
	RED_LED = lookup_table_8_bit[percentage / 16];
	__red_intensity = percentage;
}
void green_led_percentage(unsigned char percentage)
{
	GREEN_LED = lookup_table_8_bit[percentage / 16];
	__green_intensity = percentage;
}
void blue_led_percentage(unsigned char percentage)
{
	BLUE_LED = lookup_table_8_bit[percentage / 16];
	__blue_intensity = percentage;
}

void all_leds_percentage(unsigned char percentage)
{
	red_led_percentage(percentage);
	green_led_percentage(percentage);
	blue_led_percentage(percentage);
}

void set_color(const struct color_t color)
{
	red_led_percentage(color.red);
	green_led_percentage(color.green);
	blue_led_percentage(color.blue);
}

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

void parse_color(struct color_t* color, char* string)
{
	color->red   = hex_to_int(string[0]) * 16 + hex_to_int(string[1]);
	color->green = hex_to_int(string[2]) * 16 + hex_to_int(string[3]);
	color->blue  = hex_to_int(string[4]) * 16 + hex_to_int(string[5]);
}


// ===========================[ Programs ]===================================

// *****  fixed color
volatile struct color_t __pr_fixed_color_on_table;
volatile struct color_t __pr_fixed_color_in_hand;
volatile struct color_t __pr_fixed_color_up;
volatile struct color_t __pr_fixed_color_down;
volatile struct color_t __pr_fixed_color_catch_flash;


volatile int pr_fixed_color_timer = 0;

void pr_fixed_color_enter(bool was_throw)
{
	if (was_throw)
		pr_fixed_color_timer = __last_free_fall_duration / 10 + 1;
}

void pr_fixed_color_leave(void)
{
	// Going up (don't wait for the timer.)
	set_color(__pr_fixed_color_up);
}

void pr_fixed_color_tick_callback(char * param)
{
	// TODO: add some nicer interpolation
	if (__is_on_table)
	{
		// On table
		set_color(__pr_fixed_color_on_table);
	}
	else if (__in_free_fall)
	{
		unsigned int halfway = __last_free_fall_duration/2;

		// Going up
		if (__free_fall_duration < halfway)
			set_color(__pr_fixed_color_up);

		// Going down
		else
			set_color(__pr_fixed_color_down);
	}
	else
	{
		// Catch flash
		if (__time_since_last_catch < pr_fixed_color_timer)
			set_color(__pr_fixed_color_catch_flash);

		// In hand
		else
			set_color(__pr_fixed_color_in_hand);
	}
}


void pr_fixed_color_install(char * param)
{
	// The parameter looks like:
	//pr_fixed_color_install("004400_880088_ff0000_0000ff_ffffff");
	int length = strlen(param);
	__enter_hand_callback = &pr_fixed_color_enter;
	__leave_hand_callback = &pr_fixed_color_leave;
	__timer_tick_callback = &pr_fixed_color_tick_callback;;

	// Param is supposed to be a hex_to_int color, like FF0044

	// On table
	__pr_fixed_color_on_table.red   = hex_to_int(param[0]) * 16 + hex_to_int(param[1]);
	__pr_fixed_color_on_table.green = hex_to_int(param[2]) * 16 + hex_to_int(param[3]);
	__pr_fixed_color_on_table.blue  = hex_to_int(param[4]) * 16 + hex_to_int(param[5]);

	// In hand
	if (length>7)
	{
		__pr_fixed_color_in_hand.red   = hex_to_int(param[7]) * 16 + hex_to_int(param[8]);
		__pr_fixed_color_in_hand.green = hex_to_int(param[9]) * 16 + hex_to_int(param[10]);
		__pr_fixed_color_in_hand.blue  = hex_to_int(param[11]) * 16 + hex_to_int(param[12]);
	}	
	else
		__pr_fixed_color_in_hand = __pr_fixed_color_on_table;

	// Up
	if (length>14)
	{
		__pr_fixed_color_up.red   = hex_to_int(param[14]) * 16 + hex_to_int(param[15]);
		__pr_fixed_color_up.green = hex_to_int(param[16]) * 16 + hex_to_int(param[17]);
		__pr_fixed_color_up.blue  = hex_to_int(param[18]) * 16 + hex_to_int(param[19]);
	}	
	else
		__pr_fixed_color_up = __pr_fixed_color_in_hand;

	// Down
	if (length>21)
	{
		__pr_fixed_color_down.red   = hex_to_int(param[21]) * 16 + hex_to_int(param[22]);
		__pr_fixed_color_down.green = hex_to_int(param[23]) * 16 + hex_to_int(param[24]);
		__pr_fixed_color_down.blue  = hex_to_int(param[25]) * 16 + hex_to_int(param[26]);
	}
	else
		__pr_fixed_color_down = __pr_fixed_color_up;

	// Catch flash
	if (length>28)
	{
		__pr_fixed_color_catch_flash.red   = hex_to_int(param[28]) * 16 + hex_to_int(param[29]);
		__pr_fixed_color_catch_flash.green = hex_to_int(param[30]) * 16 + hex_to_int(param[31]);
		__pr_fixed_color_catch_flash.blue  = hex_to_int(param[32]) * 16 + hex_to_int(param[33]);
	}
	else
		__pr_fixed_color_catch_flash = __pr_fixed_color_in_hand; // If not given -> same as in hand
}

// ***** Fade to color
volatile struct color_t __pr_fade_to_color_start;
volatile struct color_t __pr_fade_to_color_end;

volatile unsigned int __pr_fade_to_color_time;
volatile unsigned int __pr_fade_to_color_ticks;

void pr_fade_to_color_tick_callback(void)
{
	if (__pr_fade_to_color_ticks < __pr_fade_to_color_time)
	{
		__pr_fade_to_color_ticks ++;

		// Now iterpolate between start and end color 
		float percentage = (float)__pr_fade_to_color_ticks / (float)__pr_fade_to_color_time;

		red_led_percentage((int)__pr_fade_to_color_start.red + (float)(__pr_fade_to_color_end.red - __pr_fade_to_color_start.red) * percentage);
		green_led_percentage((int)__pr_fade_to_color_start.green + (float)(__pr_fade_to_color_end.green - __pr_fade_to_color_start.green) * percentage);
		blue_led_percentage((int)__pr_fade_to_color_start.blue + (float)(__pr_fade_to_color_end.blue - __pr_fade_to_color_start.blue) * percentage);
	}
}

void pr_fade_to_color_install(char * param)
{
	// Remember start value
	__pr_fade_to_color_start.red = __red_intensity;
	__pr_fade_to_color_start.green = __green_intensity;
	__pr_fade_to_color_start.blue = __blue_intensity;

	// parameter like FF0044:100
	__pr_fade_to_color_end.red = hex_to_int(param[0]) * 16 + hex_to_int(param[1]);
	__pr_fade_to_color_end.green = hex_to_int(param[2]) * 16 + hex_to_int(param[3]);
	__pr_fade_to_color_end.blue = hex_to_int(param[4]) * 16 + hex_to_int(param[5]);

	// Fade speed
	if (! sscanf(param+7, "%i", &__pr_fade_to_color_time))
		__pr_fade_to_color_time = 0;
	__pr_fade_to_color_ticks = 0;

	// Callbacks
	__enter_hand_callback = &dummy_callback;
	__leave_hand_callback = &dummy_callback;
	__timer_tick_callback = &pr_fade_to_color_tick_callback;
}


// *****  Force to intensity: free fall=white, lot of force -> dark

volatile struct color_t __pr_force_to_intensity_color;

void pr_force_to_intensity_tick_callback(void)
{
	// Calculate 'force' distance
	float distance = 0;
	distance += ((signed int)__last_measurement_x - 128) * (__last_measurement_x > 128 ? 1 : -1);
	distance += ((signed int)__last_measurement_y - 128) * (__last_measurement_y > 128 ? 1 : -1);
	distance += ((signed int)__last_measurement_z - 128) * (__last_measurement_z > 128 ? 1 : -1);

	// Interpolate 0..distance between custom_color..black
	float factor = 1.f - distance / 40;
	if (factor < 0) factor = 0;
	red_led_percentage(__pr_force_to_intensity_color.red * factor);
	green_led_percentage(__pr_force_to_intensity_color.green * factor);
	blue_led_percentage(__pr_force_to_intensity_color.blue * factor);
}

void pr_force_to_intensity_install(char * param)
{
	// Colors
	__pr_force_to_intensity_color.red = hex_to_int(param[0]) * 16 + hex_to_int(param[1]);
	__pr_force_to_intensity_color.green = hex_to_int(param[2]) * 16 + hex_to_int(param[3]);
	__pr_force_to_intensity_color.blue = hex_to_int(param[4]) * 16 + hex_to_int(param[5]);

	// Callbacks
	__enter_hand_callback = &dummy_callback;
	__leave_hand_callback = &dummy_callback;
	__timer_tick_callback = &pr_force_to_intensity_tick_callback;
}

// *****  Alternate every throw between red, green and blue

int __pr_alternate = 0;

void pr_alternate_enter(bool was_throw)
{
	// DIM leds
	if (__pr_alternate == 0)
		green_led_percentage(20);
	else if (__pr_alternate == 1)
		blue_led_percentage(20);
	else
		red_led_percentage(20);

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


// ****** Pulse color, fade out afterwards

volatile struct color_t __pr_pulse_color1;
volatile struct color_t __pr_pulse_color2;
volatile int __pr_pulse_time;
volatile int __pr_pulse_ticks;

void pr_pulse_tick_callback(void)
{
	// Interpolate 0..distance between custom_color..black
	if (__pr_pulse_ticks < __pr_pulse_time)
	{
		__pr_pulse_ticks ++;

		// Now iterpolate between start and end color 
		float percentage = (float)__pr_pulse_ticks / (float)__pr_pulse_time;

		red_led_percentage((int)__pr_pulse_color1.red + (float)(__pr_pulse_color2.red - __pr_pulse_color1.red) * percentage);
		green_led_percentage((int)__pr_pulse_color1.green + (float)(__pr_pulse_color2.green - __pr_pulse_color1.green) * percentage);
		blue_led_percentage((int)__pr_pulse_color1.blue + (float)(__pr_pulse_color2.blue - __pr_pulse_color1.blue) * percentage);
	}
}


// RUN pulse ffffff_ffffff_150
void pr_pulse_install(char* param)
{
	parse_color(&__pr_pulse_color1, param);
	parse_color(&__pr_pulse_color2, param + 7);

	// Fade speed
	if (! sscanf(param+14, "%i", &__pr_pulse_time))
		__pr_pulse_time = 0;
	__pr_pulse_ticks = 0;

	set_color(__pr_pulse_color1);

	__enter_hand_callback = &dummy_callback;
	__leave_hand_callback = &dummy_callback;
	__timer_tick_callback = &pr_pulse_tick_callback;
}

// ****** invisible up, comes down in blue.

volatile int pr_rain_drop_timer = 0;

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

// ****** Standby: dim all LEDs.

void pr_standby_install(char*param)
{
	__enter_hand_callback = &dummy_callback;
	__leave_hand_callback = &dummy_callback;
	__timer_tick_callback = &dummy_callback;
	all_leds_off();
}

// List of installed programs

#define PROGRAM_COUNT 6
const char* program_names[] = {
	"fixed",
	"fade",
	"pulse",
	"alternate",
	"rain",
	"standby",
	"force_to_i",
 };

void (*program_pointers [])(char*) = {
	&pr_fixed_color_install,
	&pr_fade_to_color_install,
	&pr_pulse_install,
	&pr_alternate_install,
	&pr_rain_install,
	&pr_standby_install,
	&pr_force_to_intensity_install,
};

// Test accellerometer data (for debugging hardware)
void adc_test(void)
{
	cli();
	int i;
	all_leds_off();
	_delay_ms(200);
	all_leds_on();

	for (i = 0; i < 200; i ++)
	{
		_delay_ms(50);
		unsigned char x = get_x_accelero();
		unsigned char y = get_y_accelero();
		unsigned char z = get_z_accelero();
		char buffer[265];
		sprintf(buffer, "x=%i,y=%i,z=%i\r\n", x, y, z);
		usart_send_packet("ADC", buffer, NULL);
	}
	sei();
}

// Self test: test LEDs (for debugging hardware)
void self_test(void)
{
	// Disable interrupts
	cli();

	// Blink all colors, 5sec each
	int i;
	for (i = 0; i < 3; i ++)
	{
		// Red
		all_leds_off();
		red_led_on();
		_delay_ms(5000);

		// Green
		all_leds_off();
		green_led_on();
		_delay_ms(5000);

		// Blue
		all_leds_off();
		blue_led_on();
		_delay_ms(5000);
	}

	// Enable interrupts again
	sei();
}


// ===========================[ Input data parser ]===================================

bool meant_for_us(char* ball)
{
	// Is this meant for us?
	// Yes if ball == "0" or our_id in ball. It is a comma separated list.
	if (strcmp(ball, "0") == 0)
		return true;

	if (strcmp(ball, BALL_ID_STR) == 0)
		return true;

	// Comma separated list?
	char buffer[8];
	while(*ball)
	{
		int i = 0;
		while(*ball && *ball != ',' && i < 8)
		{
			buffer[i] = *ball;
			ball ++;
			i ++;
		}
		buffer[i] = NULL;

		if (strcmp(buffer, BALL_ID_STR) == 0)
			return true;
	}

	return false;
}

void process_command(char* action, char* ball, char* input_param, char* input_param2)
{
	int i;
	if (meant_for_us(ball))
	{
		// RUN: Run program
		if (strcmp(action, "RUN") == 0)
		{
			if (!button_mode)
				for (i = 0; i < PROGRAM_COUNT; i ++)
					if (strcmp(input_param, program_names[i]) == 0)
						program_pointers[i](input_param2);
		}

		// PING: answer with PONG
		else if (strcmp(action, "PING") == 0)
		{
			usart_send_packet("PONG", VERSION_ID, NULL);
		}

		// LED test
		else if (strcmp(action, "SELFTEST") == 0)
		{
			self_test();
		}
		
		// Accellero test
		else if (strcmp(action, "ADCTEST") == 0)
		{
			adc_test();
		}

		// Test battery
		else if (strcmp(action, "BATTTEST") == 0)
		{
			read_voltage();
		}

		else if (strcmp(action, "STANDBY") == 0)
		{
			if (!button_mode)
				pr_standby_install("");
		}
		
		// BUTTON:
		// - light don't run any commands when button mode is on.
		else if (strcmp(action, "BUTTON") == 0)
		{
			if (strcmp(input_param, "ON") == 0)
			{
				button_mode = true;
				pr_fixed_color_install("888888");
			}
			else if (strcmp(input_param, "OFF") == 0)
			{
				button_mode = false;
				pr_fixed_color_install("222222");
			}
		}

		// IDENTIFY: blink leds white for 2 seconds, while ignoring everything else.
		else if (strcmp(action, "IDENTIFY") == 0)
		{
			if (!button_mode)
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
}


// Protocol definition:
//    action  :=   [a-z]+
//    ball    :=   [0-9]+
//    param   :=   [a-z0-9]+
//    frame   :=  <action>  ' ' <ball> ' ' <param>


#define max_input_length 64
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

	// Timer 0 (red - 8bit)
	DDRD |= _BV(PD5);
	TCCR0A = _BV(COM0B1) | _BV(COM0B0) | _BV(WGM00) | _BV(WGM01);
	TCCR0B = _BV(CS02);
			// Fast PWM / 8 bit
			// Set on compare (COM0A1)
			// 256 prescaling

	// Timer 1 (green and blue leds - 16bit)
	DDRB |= _BV(PB1) | _BV(PB2); // (PWM led dimmers)
	TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1A0) | _BV(COM1B0) | _BV(WGM10);
	TCCR1B = _BV(WGM12) | _BV(CS12);
			// Fast PWM / 8 bit
			// Set on compare (both OC1A and OC1B)
			// 256 prescaling

	// Timer 2 (interrupt timer - 8bit)
	//DDRB |= _BV(PB3); // OC2A (PWM led dimmer)
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

	// Take 1000 samples for filling in a little history.
	int i;
	for(i = 0; i < 1000; i ++)
		adc_main_loop();

	// Default program
		// light green on table
		// purple in hand
		// red going up
		// blue going down
		// catch white
	pr_fixed_color_install("004400_550055_880000_000088_ffffff");

	// Send booted packet
	usart_send_packet("BOOTED", VERSION_ID, NULL);

	// Main loop
	while (1)
	{
		// Parse incoming data
		while (usart_receive_ready())
			parse_input_char(usart_receive());

		// Juggle ball program loop
//		adc_main_loop();
	}

	// Simple 5sec blinking light (speed test)
	while(1)
	{
		all_leds_off();
		red_led_on();
		_delay_ms(5000);
		red_led_off();
		_delay_ms(5000);
	}

	// Test accellero loop
	while(0)
	{
		_delay_ms(100);
		unsigned char x = get_x_accelero();
		unsigned char y = get_y_accelero();
		unsigned char z = get_z_accelero();
		char buffer[265];
		sprintf(buffer, "x=%i y=%i z=%i\r\n", x, y, z);
		usart_send_string(buffer);
	}

	// Test code: dimmer example loop
	all_leds_off();
	while(1)
	{
		for (i = 0; i < 256; i ++)
		{
			red_led_percentage(i);
			delay_ms(8);
		}
		delay_ms(80);
	}

	return 0;
}
