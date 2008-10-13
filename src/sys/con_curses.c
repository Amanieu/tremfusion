/*
 * ===========================================================================
 * Copyleft (]) 2008 Griffon Bowman
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

#define LOG_BUF_SIZE 2048

static int con_state = 0; /* has CON_Init been run yet? */

static char logbuf[LOG_BUF_SIZE][1024]; /* buffer that holds all of the log lines */
static int logline = 0; /* current log buffer line */
static int loglinepos = 0; /* current position in the buffer */

static char *logwinbuf[LOG_BUF_SIZE]; /* buffer for the log window (points to the log buffer) */
static int logwinline = 0; /* current log window line */
static int logwinlinepos = 0; /* current position on the current line */

static int reallogline = 0; /* count of total lines printed */

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
static _window win0;
static _window win1;
static _window win2;
static _window win3;

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

static void LogPrint ( char * );
static void LogRedraw ( void );
static void InputRedraw ( void );

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
	struct winsize winsz = { 0, };

	ioctl(fileno (stdout), TIOCGWINSZ, &winsz);

	rootwin.lines = winsz.ws_row;
	rootwin.cols = winsz.ws_col;

	/* NEVER FUCKING TRUST THIS FUNCTION: getmaxyx(rootwin.win, rootwin.lines, rootwin.cols); */

	#ifdef __DEBUG__
	DbgPrint("%s(): rootwin.lines: '%i' rootwin.cols: '%i'\n", __FUNCTION__, rootwin.lines, rootwin.cols);
	#endif

	win0.lines = 2;
	win0.cols = rootwin.cols;
	win0.x = 0;
	win0.y = 0;

	win1.lines = (rootwin.lines - win0.lines - 1);
	win1.cols = rootwin.cols;
	win1.x = 0;
	win1.y = (win0.lines);

	#ifdef __DEBUG__
	DbgPrint("%s(): win1.lines: '%i' win1.cols: '%i' win1.x: '%i' win1.y: '%i'\n", __FUNCTION__, win1.lines, win1.cols, win1.x, win1.y);
	#endif

	win2.lines = (win1.lines - 2);
	win2.cols = (win1.cols - 2);
	win2.x = (win1.x + 1);
	win2.y = (win1.y + 1);

	#ifdef __DEBUG__
	DbgPrint("%s(): win2.lines: '%i' win2.cols: '%i' win2.x: '%i' win2.y: '%i'\n", __FUNCTION__, win2.lines, win2.cols, win2.x, win2.y);
	#endif

	win3.lines = 1;
	win3.cols = rootwin.cols;
	win3.x = 0;
	win3.y = (rootwin.lines - 1);

	#ifdef __DEBUG__
	DbgPrint("%s(): win3.lines: '%i' win3.cols: '%i' win3.x: '%i' win3.y: '%i'\n", __FUNCTION__, win3.lines, win3.cols, win3.x, win3.y);
	#endif
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

	NewWin(&win0);
	NewWin(&win1);
	NewWin(&win2);
	NewWin(&win3);
}

/* refresh all of the windows */
static void RefreshWins ( void )
{
	#ifdef __DEBUG__
	DbgPrint("RefreshWins()\n");
	#endif
	/* FIXME: if this is giving you problems KILL IT */
	wnoutrefresh(rootwin.win);

	wnoutrefresh(win0.win);
	wnoutrefresh(win1.win);
	wnoutrefresh(win2.win);
	wnoutrefresh(win3.win);
	doupdate();
}

/* draw the default stuff on the windows */
static void DrawWins ( void )
{
	#ifdef __DEBUG__
	DbgPrint("DrawWins()\n");
	#endif
	/* window 0: title window */
	mvwprintw(win0.win, 0, 0, "%s", title);

	/* window 1: log window (uesd for a boarder, window 4 sits inside this one) */
	box(win1.win, 0, 0);
	mvwprintw(win1.win, 0, 1, "[log]");

	/* window 2: the actual log window, sits inside window 1 */

	/* window 3: the shell window, the command prompt lives here */
	mvwprintw(win3.win, 0, 0, "%s", input_prompt);
}

static void ResizeWins ( void )
{
	#ifdef __DEBUG__
	DbgPrint("ResizeWins()\n");
	#endif
	/* read this was needed out of the alsamixer source */
	/* resizeterm(rootwin.lines, rootwin.cols); */

	wresize(win0.win, win0.lines, win0.cols);
	wresize(win1.win, win1.lines, win1.cols);
	wresize(win2.win, win2.lines, win2.cols);
	wresize(win3.win, win3.lines, win3.cols);
}

/* resize the windows, redraw them, and set the bear trap */
static void CursResize ( void )
{
	#ifdef __DEBUG__
	DbgPrint("CursResize()\n");
	#endif
	signal(SIGWINCH, (void*) CursResize);

	endwin();
	EraseWins();
	//refresh();
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
	mvwprintw(win3.win, 0, strlen(input_prompt), "%s", input_buf);
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
	werase(win0.win);
	werase(win1.win);
	werase(win2.win);
	werase(win3.win);
}

static void MvWins ( void )
{
	mvwin(win0.win, win0.y, win0.x);
	mvwin(win1.win, win1.y, win1.x);
	mvwin(win2.win, win2.y, win2.x);
	mvwin(win3.win, win3.y, win3.x);
}

/* print to the log screen */
static void LogPrint ( char *msg )
{
	int i;

	if (logwinline >= win2.lines)
	{
		logwinline--;

		for(i = 0; i < logwinline; i++)
		{
			logwinbuf[i] = logwinbuf[(i + 1)];
		}
		logwinbuf[i] = msg;

		logwinline++;

		wclear(win2.win);
		for(i = logwinline; i >= 0; i--)
		{
			mvwprintw(win2.win, i, 0, "%s", logwinbuf[i]);
		}
	}
	else
	{
		logwinbuf[logwinline] = msg;
		/* mvwprintw(win2.win, logwinline, 0, "%s", logwinbuf[logwinline]); */

		int msglen = strlen(msg);
		int i;

		for (i=0; i <= msglen; i++, logwinlinepos++)
		{

			if ( msg[i] == '\0' )
			{
				continue;
			}
			else if ( msg[i] == '\n' )
			{
				logwinline++;
				/* FIXME: wtf, why is this wanting to be -2 to work!? */
				logwinlinepos = -2;
				continue;
			}
			else
			{
				mvwaddch(win2.win, logwinline, logwinlinepos, msg[i]);
			}
		}
	}

	WinRefresh(&win2);
        //logwinline++;
}

/* redraw the log window after a resize or some such event */
static void LogRedraw ( void )
{
	int i;

	wclear(win2.win);
	for(i = (logwinline - 1); i >= 0; i--)
	{
		mvwprintw(win2.win, i, 0, "%s", logwinbuf[i]);
	}
}


/* -- tremulous functions -- */

#if 0
void CON_Print ( const char *msg )
{
	#ifdef __DEBUG__
	DbgPrint("CON_Print()\n");
	#endif
	if ( con_state )
	{
		wprintw(win2.win, "%s", msg);
		WinRefresh(&win2);
	}
	else
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), "WARNING: CON_Print() called before CON_Init()\nCon_Print(\"%s\")\n", msg);
		ErrPrint(buf);
	}
}
#endif

void CON_Print ( const char *msg )
{
	if ( con_state )
	{
		if ( logline > LOG_BUF_SIZE )
		{
			logline = 0;
			snprintf(logbuf[logline], 41, "NOTICE: log buffer filled, starting over");
			LogPrint(logbuf[logline]);
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

				LogPrint(logbuf[logline]);
				logline++;
				reallogline++;
				loglinepos = -1;

				continue;
			}

			logbuf[logline][loglinepos] = msg[i];
			logbuf[logline][(loglinepos + 1)] = '\0';
		}

		//LogPrint(logbuf[logline]);
		//logline++;
		//reallogline++;
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

		/* FIXME: add terminal checking (to make sure we have a usable TTY of sorts (would this be redundant since ncurses does this(it does do this right?)) */
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
					mvwdelch(win3.win, 0, (strlen(input_prompt) + i));
				}

				WinRefresh(&win3);

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
				mvwdelch(win3.win, 0, (strlen(input_prompt) + (input_winbuf_pos - 1)));
#else
				mvwdelch(win3.win, 0, (strlen(input_prompt) + (input_buf_pos - 1)));
#endif
				WinRefresh(&win3);

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
			if ( input_buf_pos >= (win3.cols - strlen(input_prompt)) )
			{
				int i;

				for (i = input_winbuf_pos; i >= 0; i--)
				{
					mvwdelch(win3.win, 0, (strlen(input_prompt) + i));
				}

				input_winbuf_pos = 0;
			}

			mvwaddch(win3.win, 0, (strlen(input_prompt) + input_winbuf_pos), buf);
			WinRefresh(&win3);

			input_buf_pos++;
			input_winbuf_pos++;
#else
			mvwaddch(win3.win, 0, (strlen(input_prompt) + input_buf_pos), buf);
			WinRefresh(&win3);

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
