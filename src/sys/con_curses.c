/*
===========================================================================
Copyright (C) 2008 Amanieu d'Antras & Griffon Bowman

This file is part of Tremfusion.

Tremfusion is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremfusion is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremfusion; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/


#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <curses.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define TITLE "---[ Tremfusion Console ]---"
#define PROMPT "-> "

// Functions from con_tty.c for fallback
void CON_Shutdown_tty(void);
void CON_Init_tty(void);
char *CON_Input_tty(void);
void CON_Print_tty(const char *message);

// History functions from con_tty.c
void Hist_Add(field_t *field);
field_t *Hist_Next(void);
field_t *Hist_Prev(void);

static qboolean ncurses_on = qfalse;
static field_t input_field;
static WINDOW *rootwin;
static WINDOW *borderwin;
static WINDOW *logwin;
static WINDOW *inputwin;

#define NUM_LOG_LINES 1024
static char logbuf[NUM_LOG_LINES][1024];
static int nextline = 0;

/*
==================
CON_Resize

The window has just been resized, move everything back into place
==================
*/
static void CON_Resize(void)
{
	struct winsize winsz = {0, };

	ioctl(fileno(stdout), TIOCGWINSZ, &winsz);
	keypad(rootwin, FALSE);
	endwin();
	resizeterm(winsz.ws_row + 1, winsz.ws_col + 1);
	resizeterm(winsz.ws_row, winsz.ws_col);
	clear();
	CON_Init();
}

/*
==================
CON_Shutdown

Never exit without calling this, or your terminal will be left in a pretty bad state
==================
*/
void CON_Shutdown(void)
{
	if (ncurses_on)
		endwin();
	else
		CON_Shutdown_tty();
}

/*
==================
CON_Init

Initialize the console in ncurses mode, fall back to tty mode on failure
==================
*/
void CON_Init(void)
{
	// If the process is backgrounded (running non interactively)
	// then SIGTTIN or SIGTOU is emitted, if not caught, turns into a SIGSTP
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	// Initialize ncurses and create the root window
	rootwin = initscr();
	if (!rootwin) {
		CON_Print_tty("Couldn't initialize ncurses, falling back to tty\n");
		CON_Init_tty();
		return;
	}
	cbreak();
	noecho();
	refresh();
	nodelay(rootwin, TRUE);
	keypad(rootwin, TRUE);

	// Create the border
	borderwin = newwin(LINES - 2, COLS, 1, 0);
	box(borderwin, 0, 0);
	wrefresh(borderwin);

	// Create the log
	logwin = newwin(LINES - 4, COLS - 2, 2, 1);
	scrollok(logwin, TRUE);
	idlok(logwin, TRUE);
	leaveok(logwin, TRUE);
	if (ncurses_on) {
		int i = nextline;
		do {
			if (logbuf[i][0])
				wprintw(logwin, logbuf[i]);
			if (++i >= NUM_LOG_LINES)
				i = 0;
		} while (i != nextline);
	}
	wrefresh(logwin);

	// Create the input field
	inputwin = newwin(1, COLS - strlen(PROMPT) + 1, LINES - 1, strlen(PROMPT));
	if (!ncurses_on)
		wmove(inputwin, 0, 0);
	else
		mvwprintw(inputwin, 0, 0, "%s", input_field.buffer);
	wrefresh(inputwin);

	// Display the title and input prompt
	mvprintw(0, (COLS - strlen(TITLE)) / 2, TITLE);
	mvprintw(LINES - 1, 0, PROMPT);
	refresh();

	signal(SIGWINCH, (void *)CON_Resize);

	ncurses_on = qtrue;
}

/*
==================
CON_Input
==================
*/
char *CON_Input(void)
{
	int chr;
	static char text[MAX_EDIT_LINE];
	field_t *history;

	if (!ncurses_on)
		return CON_Input_tty();

	while (1) {
		chr = getch();

		// Special characters
		switch (chr) {
		case ERR:
			wclear(inputwin);
			mvwprintw(inputwin, 0, 0, "%s", input_field.buffer);
			wmove(inputwin, 0, input_field.cursor);
			wrefresh(inputwin);
			return NULL;
		case '\n':
			if (!input_field.buffer[0])
				continue;
			input_field.cursor = strlen(input_field.buffer);
			Hist_Add(&input_field);
			strcpy(text, input_field.buffer);
			Field_Clear(&input_field);
			return text;
		case '\t':
			Field_AutoComplete(&input_field);
			input_field.cursor = strlen(input_field.buffer);
			continue;
		case KEY_LEFT:
			if (input_field.cursor > 0)
				input_field.cursor--;
			continue;
		case KEY_RIGHT:
			if (input_field.cursor < strlen(input_field.buffer))
				input_field.cursor++;
			continue;
		case KEY_UP:
			history = Hist_Prev();
			if (history)
				input_field = *history;
			continue;
		case KEY_DOWN:
			history = Hist_Next();
			if (history)
				input_field = *history;
			else if (input_field.buffer[0]) {
				input_field.cursor = strlen(input_field.buffer);
				Hist_Add(&input_field);
				Field_Clear(&input_field);
			}
			continue;
		case KEY_HOME:
			input_field.cursor = 0;
			continue;
		case KEY_END:
			input_field.cursor = strlen(input_field.buffer);
			continue;
		case KEY_NPAGE:
			wscrl(logwin, 2);
			wrefresh(logwin);
			continue;
		case KEY_PPAGE:
			wscrl(logwin, -2);
			wrefresh(logwin);
			continue;
		case KEY_BACKSPACE:
			if (input_field.cursor <= 0)
				continue;
			input_field.cursor--;
			// Fall through
		case KEY_DC:
			if (input_field.cursor < strlen(input_field.buffer)) {
				memmove(input_field.buffer + input_field.cursor,
				        input_field.buffer + input_field.cursor + 1,
				        strlen(input_field.buffer) - input_field.cursor);
			}
			continue;
		}

		// Normal characters
		if (chr >= ' ' && chr < 256 && strlen(input_field.buffer) + 1 < sizeof(input_field.buffer)) {
			memmove(input_field.buffer + input_field.cursor + 1,
			        input_field.buffer + input_field.cursor,
			        strlen(input_field.buffer) - input_field.cursor);
			input_field.buffer[input_field.cursor] = chr;
			input_field.cursor++;
		}
	}
}

/*
==================
CON_Print
==================
*/
void CON_Print(const char *message)
{
	if (!ncurses_on) {
		CON_Print_tty(message);
		return;
	}

	// Print the message in the log window
	wprintw(logwin, message);
	wrefresh(logwin);

	// Add the message to the log buffer
	Q_strncpyz(logbuf[nextline], message, sizeof(logbuf[0]));
	nextline++;
	if (nextline >= NUM_LOG_LINES)
		nextline = 0;

	// Move the cursor back to the input field
	wmove(inputwin, 0, input_field.cursor);
	wrefresh(inputwin);
}
