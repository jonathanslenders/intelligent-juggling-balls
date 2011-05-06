#include <curses.h> /* ncurses interface, also defines boolean types */
#include <stdlib.h> /* for malloc, free, exit */

#include "../main.h"

WINDOW *color_window = NULL;

int ball = 0;
int red = 0;
int green = 0;
int blue = 0;


void print_color_window(void)
{
	// Print ball statusses
	char buffer[256];
	snprintf(buffer, 256, "%3i    %3i   %3i   %3i", ball, red, green, blue);
	mvwprintw(color_window, 3, 1, buffer);
	mvwprintw(color_window, 2, 1, "Ball  Red Green Blue");

    // Print box and title
	box(color_window, 0, 0);
	wattron(color_window, COLOR_PAIR(1));
	mvwprintw(color_window, 0, 4, "Color mixer");
	wattroff(color_window, COLOR_PAIR(1));

	wrefresh(color_window);
}



void color_mixer_activate(void * data)
{
	color_window = newwin(6, 50, 10, 2);

	while(true)
	{
		int ch = getch();

		if (ch == '2')
			red = (red + 10) % 255;
		if (ch == '5')
			green = (green + 10) % 255;
		if (ch == '8')
			blue = (blue + 10) % 255;

		if (ch == '1')
			red = (red + 255 - 10) % 255;
		if (ch == '4')
			green = (green + 255 - 10) % 255;
		if (ch == '7')
			blue = (blue + 255 - 10) % 255;

		if (ch == '+')
			ball = (ball + 1) % BALL_COUNT;
		if (ch == '-')
			ball = (ball + BALL_COUNT - 1) % BALL_COUNT;

		if (ch == 10) // Enter -> send command
		{
			char code[255];
			sprintf(code, "%02x%02x%02x:500", red, green, blue);
			send_packet("RUN", ball, "fade", code);
		}

		if (ch == 27 || ch == 'q') // Escape -> go back
			break;

		print_color_window();

		sleep(0);
	}

	delwin(color_window);
}
