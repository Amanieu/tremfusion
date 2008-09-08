/*
 * TODO:
 * 1) console history
 * 2) log scrolling
 * 3) info for the stats windows
 * 4) terminal size checking
 * 5) console tab completion
 * 6) console input line size checking (typing in more than your terminal is wide)
 * 
 * BUGS
 *
 * 1) when resizing the terminal rapidly (sometimes normaly) the window will not be redrawn properly, not quite sure of the cause, but
 * i think its mostly ironed out.
 *
 * 2) part of bug one, the 5th window likes to disappear alot in this bug (indicating wrong terminal size being used?)
 *
 * basicly if you dont abuse the window of the terminal it should be fine, the quick fix is to resize the window again to get it to redraw
 * 
 * 3) the cursor doesnt seem to get placed correctly
 *
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

static int root_y, root_x; /* size of the root window */

/* FIXME: use _window indoesd of WINDOW */
static WINDOW *rootwin; /* root window */

/* windows */
static _window win0;
#if 0
static _window win1;
static _window win2;
#endif
static _window win3;
static _window win4;
static _window win5;

static char title[] = "[TremFusion Dedicated Server]"; /* console title */

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
static void LogPrint ( char * );
static void LogRedraw ( void );
static void LogClear ( void );
static void ErrPrint ( const char * );
static void InputRedraw ( void );
static void WinRefresh ( _window * );

/* tremulous functions */
/* FIXME: already defined in in sys_local.h */
void CON_Print ( const char * );
void CON_Init ( void );
void CON_Shutdown ( void );
char *CON_Input ( void );

/* create a new window */
static void NewWin ( _window *win )
{
	win->win = newwin(win->lines, win->cols, win->y, win->x);
}

/* set the size of the windows from the root window size */
static void SetWins ( void )
{
	/* this function LIES! getmaxyx(rootwin, root_y, root_x); */

	struct winsize winsz = { 0, };

	ioctl (fileno (stdout), TIOCGWINSZ, &winsz);

	root_y = winsz.ws_row;
	root_x = winsz.ws_col;

	win0.lines = 2;
	win0.cols = root_x;
	win0.x = 0;
	win0.y = 0;

	#if 0
	win1.lines = 8;
	win1.cols = root_x;
	win1.x = 0;
	win1.y = win0.lines;

	win2.lines = 12;
	win2.cols = root_x;
	win2.x = 0;
	win2.y = (win0.lines + win1.lines);

	win3.lines = (root_y - (win0.lines + win1.lines + win2.lines) - 1);
	win3.cols = root_x;
	win3.x = 0;
	win3.y = (win0.lines + win1.lines + win2.lines);

	win4.lines = (win3.lines - 2);
	win4.cols = (win3.cols - 2);
	win4.x = (win3.x + 1);
	win4.y = (win3.y + 1);
	#endif

	/* FIXME: remove this code (win3 and win4) when we get those windows setup */
	win3.lines = (root_y - win0.lines - 1);
	win3.cols = root_x;
	win3.x = 0;
	win3.y = (win0.lines);

	win4.lines = (win3.lines - 2);
	win4.cols = (win3.cols - 2);
	win4.x = (win3.x + 1);
	win4.y = (win3.y + 1);

	win5.lines = 1;
	win5.cols = root_x;
	win5.x = 0;
	win5.y = (root_y - 1);
}

/* make all of the happy windows */
static void InitWins ( void )
{
	rootwin = initscr();
	if ( rootwin == NULL )
	{
		ErrPrint("Fatal Error: initscr()\n");
		exit(1); /* FIXME: use a more formal exit, or a fallback? */
	}

	cbreak();
	nodelay(rootwin, TRUE);
	clearok(rootwin, TRUE);
	noecho();

	NewWin(&win0);
	#if 0
	NewWin(&win1);
	NewWin(&win2);
	#endif
	NewWin(&win3);
	NewWin(&win4);
	NewWin(&win5);
}

/* refresh all of the windows */
static void RefreshWins ( void )
{
	wrefresh(rootwin);

	WinRefresh(&win0);
	#if 0
	WinRefresh(&win1);
	WinRefresh(&win2);
	#endif
	WinRefresh(&win3);
	WinRefresh(&win4);
	WinRefresh(&win5);
}

/* draw the default stuff on the windows */
static void DrawWins ( void )
{
	/* window 0: title window */
	mvwprintw(win0.win, 0, 0, "%s", title);

	/* FIXME: turn these windows back on */
	#if 0
	/* window 1: game status window */
	box(win1.win, 0, 0);
	mvwprintw(win1.win, 0, 1, "[game status]");

	/* window 2: player status window */
	box(win2.win, 0, 0);
	mvwprintw(win2.win, 0, 1, "[player status]");
	#endif

	/* window 3: log window (uesd for a boarder, window 4 sits inside this one) */
	box(win3.win, 0, 0);
	mvwprintw(win3.win, 0, 1, "[log]");

	/* window 4: the actual log window, sits inside window 3 */

	/* window 5: the shell window, the command prompt lives here */
	mvwprintw(win5.win, 0, 0, "%s", input_prompt);
}

/* resize the windows, redraw them, and set the bear trap */
static void CursResize ( void )
{
	signal(SIGWINCH, (void*) CursResize);

	endwin();
	SetWins();
	InitWins();
	DrawWins();
	LogRedraw();
	InputRedraw();
	RefreshWins();
}

/* print to the log screen */
static void LogPrint ( char *msg )
{
	int i;

	if (logwinline >= win4.lines)
	{
		logwinline--;

		for(i = 0; i < logwinline; i++)
		{
			logwinbuf[i] = logwinbuf[(i + 1)];
		}
		logwinbuf[i] = msg;

		wclear(win4.win);
		for(i = logwinline; i >= 0; i--)
		{
			mvwprintw(win4.win, i, 0, "%s", logwinbuf[i]);
		}
	}
	else
	{
		logwinbuf[logwinline] = msg;
		/* mvwprintw(win4.win, logwinline, 0, "%s", logwinbuf[logwinline]); */

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
				mvwaddch(win4.win, logwinline, logwinlinepos, msg[i]);
			}
		}
	}

	WinRefresh(&win4);
	//logwinline++;
}

/* redraw the log window after a resize or some such event */
static void LogRedraw ( void )
{
	int i;

	wclear(win4.win);
	for(i = (logwinline - 1); i >= 0; i--)
	{
		mvwprintw(win4.win, i, 0, "%s", logwinbuf[i]);
	}
}

static void LogClear ( void )
{
	wclear(win4.win);
	WinRefresh(&win4);
	logwinline = 0;
}

/* use this when we cant trust CON_Print() */
static void ErrPrint ( const char * msg )
{
	fputs(msg, stderr);
}

/* redraw the input prompt after a resize or some such event */
static void InputRedraw ( void )
{
	mvwprintw(win5.win, 0, strlen(input_prompt), "%s", input_buf);
}

/* refresh a window */
static void WinRefresh ( _window * win )
{
	wrefresh(win->win);
}


/* -- tremulous functions -- */

#if 0
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

		/* FIXME: add the whole colorization thing, people seem to like that */

		snprintf(logbuf[logline], 1024, "%i: %s", reallogline, msg );
		LogPrint(logbuf[logline]);

		logline++;
		reallogline++;
	}
	else
	{
		char buf[1024];

		snprintf(buf, sizeof(buf), "WARNING: CON_Print() called before CON_Init()\nCon_Print(\"%s\")\n", msg);
		ErrPrint(buf);
	}
}
#endif

/* test code */
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
	if ( !con_state )
	{
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTTOU, SIG_IGN);

		fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) | O_NONBLOCK );

		/* FIXME: add terminal checking (to make sure we have a usable TTY of sorts (would this be redundant since ncurses does this(it does do this right?)) */
		if (isatty(STDIN_FILENO)!=1)
		{
			ErrPrint("WARNING: stdin is not a tty\n");
		}

		SetWins();
		InitWins();
		CursResize();

		con_state++;
	}
	else
	{
		ErrPrint("WARNING: CON_Init() called after a previous CON_Init() and before CON_Shutdown()\n");
	}
}

void CON_Shutdown ( void )
{
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

		buf = wgetch(rootwin);

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
					mvwdelch(win5.win, 0, (strlen(input_prompt) + i));
				}

				WinRefresh(&win5);

				input_buf[0] = '\0';
				input_buf_pos = 0;

				/* i wont tell about this command if you wont >.>  */
				if ( Q_strncmp("clear", text, strlen(text)) == 0 )
				{
					LogClear();
					return(NULL);
				}

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
				mvwdelch(win5.win, 0, (strlen(input_prompt) + (input_winbuf_pos - 1)));
#else
				mvwdelch(win5.win, 0, (strlen(input_prompt) + (input_buf_pos - 1)));
#endif
				WinRefresh(&win5);

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
			buf = wgetch(rootwin);

			if ( buf == '[' || buf == 'O' )
			{
				buf = wgetch(rootwin);

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
			if ( input_buf_pos >= (win5.cols - strlen(input_prompt)) )
			{
				int i;

				for (i = input_winbuf_pos; i >= 0; i--)
				{
					mvwdelch(win5.win, 0, (strlen(input_prompt) + i));
				}

				input_winbuf_pos = 0;
			}

			mvwaddch(win5.win, 0, (strlen(input_prompt) + input_winbuf_pos), buf);
			WinRefresh(&win5);

			input_buf_pos++;
			input_winbuf_pos++;
#else
			mvwaddch(win5.win, 0, (strlen(input_prompt) + input_buf_pos), buf);
			WinRefresh(&win5);

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
