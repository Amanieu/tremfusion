/*
 * ===========================================================================
 * Copyright (C) 2008 Griffon Bowman
 *
 * This file is part of Tremfusion.
 *
 * Tremfusion is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Tremfusion is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tremulous; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * ===========================================================================
 */

/*
 * ncurses version of the quake console
 * src/sys/con_curses.c
 */

/*
 * FIXME: major stability issues ater lots of prints
 *
 * TODO:
 * history buffer
 * tab completion
 * finish input line handeling (like when size_of_string > width_of_window)
 * add a clear function
 * code cleanup
 */

#ifndef __STAND_ALONE__
	#include "../qcommon/q_shared.h"
	#include "../qcommon/qcommon.h"
	#include "sys_local.h"
#endif

#include <curses.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __STAND_ALONE__
	#include <string.h>
	#define Q_strncpyz strncpy
	#define Q_strncmp strncmp
#endif

#ifdef __DEBUG__
FILE *dbg_file;

static void InitDebug ( void )
{
	dbg_file = fopen("./con_curses_trace", "w");
}

static void DbgPrint ( const char *fmt, ... )
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(dbg_file, fmt, ap);
	va_end(ap);
}
#endif

#define LOG_BUF_SIZE 1024

static int con_state = 0; /* has CON_Init been run yet? */

static char logbuf[LOG_BUF_SIZE][1024]; /* buffer that holds all of the log lines */
static int logline = 0; /* current log buffer line */
static int loglinepos = 0; /* current position in the buffer */

static int logwinline = 0;
static int logwinlinepos = 0;

static int reallogline = 0; /* count of total lines printed */

static int scrollline = 0; /* current position in the scroll buffer */

typedef struct
{
	WINDOW *win;

	int lines;
	int cols;

	int x;
	int y;
} _window;

static _window rootwin; /* root window */

/* windows */
static _window win_title;
static _window win_logframe;
static _window win_log;
static _window win_input;

static char title[] = "[TremFusion Console]"; /* console title */

static char input_prompt[] = "-> "; /* input prompt */
static char input_buf[1024]; /* input buffer */
static int input_buf_pos = 0; /* input buffer position */

#ifdef __BUGGY_INPUT__
/* static char input_winbuf[1024]; */ /* input window buffer */
static int input_winbuf_pos = 0; /* input window buffer position */
#endif

/* -- prototypes -- */

/* helper functions */
static void NewWin ( _window * );
static void SetWins ( void );
static void InitWins ( void );
static void RefreshWins ( void );
static void DrawWins ( void );
static void CursResize ( void );
static void ErrPrint ( const char * );
static void InputRedraw ( void );
static void WinRefresh ( _window * );
static void EraseWins ( void );
static void MvWins ( void );

static void LogPrint ( void );
static void LogRedraw ( void );
static void LogScroll ( void );

/* tremulous functions */
/* FIXME: already defined in in sys_local.h */
void CON_Print ( const char * );
void CON_Init ( void );
void CON_Shutdown ( void );
char *CON_Input ( void );

/* create a new window */
static void NewWin ( _window *win )
{
	#ifdef __DEBUG__
	DbgPrint("NewWin()\n");
	#endif
	win->win = newwin(win->lines, win->cols, win->y, win->x);
}

/* set the size of the windows from the root window size */
static void SetWins ( void )
{
	#ifdef __DEBUG__
	DbgPrint("SetWins()\n");
	#endif

	/* i think i found this trick in alsamixer */
	struct winsize winsz = { 0, };

	ioctl(fileno (stdout), TIOCGWINSZ, &winsz);

	rootwin.lines = winsz.ws_row;
	rootwin.cols = winsz.ws_col;

	/* the above function replaces this one: getmaxyx(rootwin.win, rootwin.lines, rootwin.cols); */

	#ifdef __LOG_ONLY__
	win_title.lines = 1;
	#else
	win_title.lines = 2;
	#endif
	win_title.cols = rootwin.cols;
	win_title.x = 0;
	win_title.y = 0;

	win_logframe.lines = (rootwin.lines - win_title.lines - 1);
	win_logframe.cols = rootwin.cols;
	win_logframe.x = 0;
	win_logframe.y = (win_title.lines);

	win_log.lines = (win_logframe.lines - 2);
	win_log.cols = (win_logframe.cols - 2);
	win_log.x = (win_logframe.x + 1);
	win_log.y = (win_logframe.y + 1);

	win_input.lines = 1;
	win_input.cols = rootwin.cols;
	win_input.x = 0;
	win_input.y = (rootwin.lines - 1);
}

/* make all of the happy windows */
static void InitWins ( void )
{
	#ifdef __DEBUG__
	DbgPrint("InitWins()\n");
	#endif
	rootwin.win = initscr();
	if ( rootwin.win == NULL )
	{
		ErrPrint("Fatal Error: initscr()\n");
		exit(1); /* FIXME: use a more formal exit, or a fallback? */
	}

	cbreak();
	nodelay(rootwin.win, TRUE);
	noecho();

	NewWin(&win_title);
	NewWin(&win_logframe);
	NewWin(&win_log);
	NewWin(&win_input);

	leaveok(win_title.win, FALSE);
	leaveok(win_logframe.win, FALSE);
	leaveok(win_log.win, FALSE);
	leaveok(win_input.win, FALSE);
}

/* refresh all of the windows */
static void RefreshWins ( void )
{
	#ifdef __DEBUG__
	DbgPrint("RefreshWins()\n");
	#endif
	/* FIXME: if this is giving you problems KILL IT */
	wnoutrefresh(rootwin.win);

	wnoutrefresh(win_title.win);
	wnoutrefresh(win_logframe.win);
	wnoutrefresh(win_log.win);
	wnoutrefresh(win_input.win);
	doupdate();
}

/* draw the default stuff on the windows */
static void DrawWins ( void )
{
	#ifdef __DEBUG__
	DbgPrint("DrawWins()\n");
	#endif
	/* title window */
	mvwprintw(win_title.win, 0, 0, "%s", title);

	/* log window frame (uesd for a boarder, the log window sits ontop this one) */
	box(win_logframe.win, 0, 0);
	#ifndef __LOG_ONLY__
	mvwprintw(win_logframe.win, 0, 1, "[log]");
	#endif

	/* the actual log window, sits ontop of window_logframe */

	/* the input window, the command prompt lives here */
	mvwprintw(win_input.win, 0, 0, "%s", input_prompt);
}

static void ResizeWins ( void )
{
	#ifdef __DEBUG__
	DbgPrint("ResizeWins()\n");
	#endif
	/* read this was needed out of the alsamixer source */
	/* resizeterm(rootwin.lines, rootwin.cols); */

	wresize(win_title.win, win_title.lines, win_title.cols);
	wresize(win_logframe.win, win_logframe.lines, win_logframe.cols);
	wresize(win_log.win, win_log.lines, win_log.cols);
	wresize(win_input.win, win_input.lines, win_input.cols);
}

/* resize the windows, redraw them, and set the bear trap */
static void CursResize ( void )
{
	#ifdef __DEBUG__
	DbgPrint("CursResize()\n");
	#endif
	signal(SIGWINCH, (void*) CursResize);

	scrollline = 0;

	endwin();
	EraseWins();
	RefreshWins();
	SetWins();
	ResizeWins();
	MvWins();
	DrawWins();
	LogRedraw();
	InputRedraw();
	RefreshWins();
}

/* use this when we cant trust CON_Print() */
static void ErrPrint ( const char * msg )
{
	fputs(msg, stderr);
}

/* redraw the input prompt after a resize or some such event */
static void InputRedraw ( void )
{
	#ifdef __DEBUG__
	DbgPrint("InputRedraw()\n");
	#endif
	mvwprintw(win_input.win, 0, strlen(input_prompt), "%s", input_buf);
}

/* refresh a window */
static void WinRefresh ( _window * win )
{
	#ifdef __DEBUG__
	DbgPrint("WinRefresh()\n");
	#endif
	wrefresh(win->win);
}


/* erase all windows */
static void EraseWins ( void )
{
	werase(win_title.win);
	werase(win_logframe.win);
	werase(win_log.win);
	werase(win_input.win);
}

static void MvWins ( void )
{
	mvwin(win_title.win, win_title.y, win_title.x);
	mvwin(win_logframe.win, win_logframe.y, win_logframe.x);
	mvwin(win_log.win, win_log.y, win_log.x);
	mvwin(win_input.win, win_input.y, win_input.x);
}

/* print to the log screen */
static void LogPrint ( void )
{
	if ( logwinline >= win_log.lines )
	{
		logwinline--;

		int j;
		werase(win_log.win);

		j = (logline - win_log.lines);

		for ( logwinline = 0; j <= logline; logwinline++, j++ )
		{
			mvwprintw(win_log.win, logwinline, 0, "%s", logbuf[j]);
		}
	}
	else
	{
		int msglen = strlen(logbuf[logline]);
		int i;

		for (i=0; i <= msglen; i++, logwinlinepos++)
		{
			if ( logbuf[logline][i] == '\0' )
			{
				break;
			}
			else if ( logbuf[logline][i] == '\n' )
			{
				logwinline++;
				/* FIXME: wtf, why is this wanting to be -2 to work!? */
				logwinlinepos = -2;
				continue;
			}
			else
			{
				mvwaddch(win_log.win, logwinline, logwinlinepos, logbuf[logline][i]);
			}
		}
	}

	WinRefresh(&win_log);
}

/* redraw the log window after a resize or some such event */
static void LogRedraw ( void )
{
	int i, j;

	werase(win_log.win);

	if ( logline > win_log.lines )
	{
		j = (logline - win_log.lines);
	}
	else
	{
		j = 0;
	}

	for ( i = 0; ((i <= win_log.lines || j < logline) && i < logline); i++, j++ )
	{
		mvwprintw(win_log.win, i, 0, "%s", logbuf[j]);
	}
}

/* draw the log window from the scroll point */
static void LogScroll ( void )
{
	int i, j;

	wclear(win_log.win);

	if ( logline > win_log.lines )
	{
		j = ((logline - scrollline) - win_log.lines);
	}
	else
	{
		j = 0;
	}

	if ( j < 0 )
	{
		scrollline--;
		return;
	}

	for ( i = 0; ((i <= win_log.lines || j < logline) && i < logline); i++, j++ )
	{
		mvwprintw(win_log.win, i, 0, "%s", logbuf[j]);
	}

	WinRefresh(&win_log);
}




/* -- tremulous functions -- */




void CON_Print ( const char *msg )
{
	if ( con_state )
	{
		if ( logline >= 100 )
		{
			logline = 0;
			logwinline = 0;
			snprintf(logbuf[logline], 41, "NOTICE: log buffer filled, starting over");
			LogRedraw();
			LogPrint();
			logline++;
		}

		int msglen = strlen(msg);
		int i;

		for ( i = 0; i < msglen; i++, loglinepos++ )
		{
			if ( msg[i] == '\n' )
			{
				logbuf[logline][loglinepos] = msg[i];
				logbuf[logline][(loglinepos + 1)] = '\0';

				/* dont print new data when we are scrolling through the backlog */
				if ( !scrollline )
				{
					LogPrint();
				}
				logline++;
				reallogline++;
				loglinepos = -1;

				continue;
			}

			logbuf[logline][loglinepos] = msg[i];
			logbuf[logline][(loglinepos + 1)] = '\0';
		}
	}
	else
	{
		char buf[1024];

		snprintf(buf, sizeof(buf), "WARNING: CON_Print() called before CON_Init()\nCon_Print(\"%s\")\n", msg);
		ErrPrint(buf);
	}
}

void CON_Init ( void )
{
	#ifdef __DEBUG__
	InitDebug();
	#endif

	#ifdef __DEBUG__
	DbgPrint("CON_Init()\n");
	#endif

	if ( !con_state )
	{
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);

		fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) | O_NONBLOCK );

		if ( isatty(STDIN_FILENO) != 1 )
		{
			ErrPrint("WARNING: stdin is not a tty\n");
		}

		SetWins();
		InitWins();
		DrawWins();
		RefreshWins();

		signal(SIGWINCH, (void*) CursResize);

		con_state++;
	}
	else
	{
		ErrPrint("WARNING: CON_Init() called after a previous CON_Init() and before CON_Shutdown()\n");
	}
}

void CON_Shutdown ( void )
{
	#ifdef __DEBUG__
	DbgPrint("CON_Shutdown()\n");
	#endif
	if ( con_state )
	{
		con_state--;
		endwin();
		fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) & ~O_NONBLOCK );
	}
	else
	{
		ErrPrint("WARNING: CON_Shutdown called before CON_Init()\n");
	}
}

char *CON_Input ( void )
{
	if ( con_state )
	{
		int buf;

		buf = wgetch(rootwin.win);

		if ( buf == ERR )
		{
			return(NULL);
		}

		/* detect enter */
		if ( buf == '\n' )
		{
			if ( input_buf_pos > 0 )
			{
				int i;
				static char text[1024];

				Q_strncpyz(text, input_buf, sizeof(text));

				for (i = input_buf_pos; i >= 0; i--)
				{
					mvwdelch(win_input.win, 0, (strlen(input_prompt) + i));
				}

				WinRefresh(&win_input);

				input_buf[0] = '\0';
				input_buf_pos = 0;

				/* i wont tell about this command if you wont >.>  */
				#if 0
				if ( Q_strncmp("clear", text, strlen(text)) == 0 )
				{
					/* LogClear(); */
					return(NULL);
				}
				#endif

				return(text);
			}
			else
			{
				return(NULL);
			}
		}

		/* backspace */
		if ( buf == 127 || buf == '\b' )
		{
			if ( input_buf_pos > 0 )
			{
				input_buf[(input_buf_pos - 1)] = '\0';
#ifdef __BUGGY_INPUT__
				mvwdelch(win_input.win, 0, (strlen(input_prompt) + (input_winbuf_pos - 1)));
#else
				mvwdelch(win_input.win, 0, (strlen(input_prompt) + (input_buf_pos - 1)));
#endif
				WinRefresh(&win_input);

				input_buf_pos--;
#ifdef __BUGGY_INPUT__
				input_winbuf_pos--;
#endif
			}

			return(NULL);
		}

		/* ^L */
		if ( buf == '\f' )
		{
			CursResize();
			return(NULL);
		}

		/* escape codes */
		if ( buf == 27 )
		{
			buf = wgetch(rootwin.win);

			if ( buf == '[' || buf == 'O' )
			{
				buf = wgetch(rootwin.win);

				switch (buf)
				{
					case 'A':
						return(NULL);
					break;

					case 'B':
						return(NULL);
					break;

					case 'C':
						return(NULL);
					break;

					case 'D':
						return(NULL);
					break;

					case '5':
						/* throw away the trailing ~ */
						buf = wgetch(rootwin.win);

						scrollline++;
						LogScroll();

						return(NULL);
					break;

					case '6':
						/* throw away the trailing ~ */
						buf = wgetch(rootwin.win);

						if ( scrollline )
						{
							scrollline--;
							LogScroll();
						}
						else
						{
							LogRedraw();
						}

						return(NULL);
					break;

					default:
						return(NULL);
					break;
				}
			}
		}

		if ( buf > 31 && buf < 128 )
		{
			input_buf[input_buf_pos] = buf;
			input_buf[(input_buf_pos + 1)] = '\0';

#ifdef __BUGGY_INPUT__
			if ( input_buf_pos >= (win_input.cols - strlen(input_prompt)) )
			{
				int i;

				for (i = input_winbuf_pos; i >= 0; i--)
				{
					mvwdelch(win_input.win, 0, (strlen(input_prompt) + i));
				}

				input_winbuf_pos = 0;
			}

			mvwaddch(win_input.win, 0, (strlen(input_prompt) + input_winbuf_pos), buf);
			WinRefresh(&win_input);

			input_buf_pos++;
			input_winbuf_pos++;
#else
			mvwaddch(win_input.win, 0, (strlen(input_prompt) + input_buf_pos), buf);
			WinRefresh(&win_input);

			input_buf_pos++;
#endif

		}

		return(NULL);
	}
	else
	{
		return(NULL);
	}
}
