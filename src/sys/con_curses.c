#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <curses.h>
#include <signal.h>

#define LOG_BUF_SIZE 2048

char logbuf[LOG_BUF_SIZE][1024];
int logline = 0;

char *logwinbuf[LOG_BUF_SIZE];
int logwinline = 0;

int reallogline = 0;

typedef struct
{
	WINDOW *win;

	int lines;
	int cols;

	int x;
	int y;
} _window;

WINDOW *rootwin;

_window win0;
_window win1;
_window win2;
_window win3;
_window win4;
_window win5;

/* helper functions */
static void NewWin ( _window * );
static void ResizeWin ( _window * );
static void SetWins ( void );
static void InitWins ( void );
static void ResizeWins ( void );
static void RefreshWins ( void );
static void DrawWins ( void );
static void ClearWins ( void );

static void SigTrap ( void );
static void SigHndlr ( int );

static void CursResize ( void );
static void LogPrint ( char * );

/* tremulous functions */
void CON_Print ( const char * );
void CON_Init ( void );
void CON_Shutdown ( void );
char *CON_Input ( void );

/* create a new window */
static void NewWin ( _window *win )
{
	win->win = newwin(win->lines, win->cols, win->y, win->x);
}

/* resize a window */
static void ResizeWin ( _window *win )
{
	wresize(win->win, win->lines, win->cols);
}

/* set the size of the windows from the root window size */
static void SetWins ( void )
{
	int root_y, root_x;

	getmaxyx(rootwin, root_y, root_x);

	win0.lines = 2;
	win0.cols = root_x;
	win0.x = 0;
	win0.y = 0;

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

	win5.lines = 1;
	win5.cols = root_x;
	win5.x = 0;
	win5.y = (root_y - 1);
}

/* make all of the happy windows */
static void InitWins ( void )
{
	NewWin(&win0);
	NewWin(&win1);
	NewWin(&win2);
	NewWin(&win3);
	NewWin(&win4);
	NewWin(&win5);
}

/* resize all windows to their win.lines/cols size */
static void ResizeWins ( void )

{
	ResizeWin(&win0);
	ResizeWin(&win1);
	ResizeWin(&win2);
	ResizeWin(&win3);
	ResizeWin(&win4);
	ResizeWin(&win5);
}


/* refresh all of the windows */
static void RefreshWins ( void )
{
	wrefresh(win0.win);
	wrefresh(win1.win);
	wrefresh(win2.win);
	wrefresh(win3.win);
	wrefresh(win4.win);
	wrefresh(win5.win);
}

/* draw the default stuff on the windows */
static void DrawWins ( void )
{
	char title[] = "[Tremulous Dedicated Server]";
	// window 0
	mvwprintw(win0.win, 0, 0, "%s", title);

	// window 1
	box(win1.win, 0, 0);
	mvwprintw(win1.win, 0, 1, "[game status]");

	// window 2
	box(win2.win, 0, 0);
	mvwprintw(win2.win, 0, 1, "[player status]");

	// window 3
	box(win3.win, 0, 0);
	mvwprintw(win3.win, 0, 1, "[log]");

	// window 4

	// window 5
}

/* clear all of the windows */
static void ClearWins ( void )
{
	wclear(win0.win);
	wclear(win1.win);
	wclear(win2.win);
	wclear(win3.win);
	wclear(win4.win);
	wclear(win5.win);
}

static void SigTrap ( void )
{
	signal( SIGWINCH, SigHndlr );
}

static void SigHndlr ( int signal )
{
	CursResize();
}

/* resize the windows, redraw them, and set the bear trap */
/* FIXME: make this work 100% of the time (works less than 5% of the time right now), and redraw the log window */
static void CursResize ( void )
{
	endwin();
	SetWins();
	ResizeWins();
	ClearWins();
	DrawWins();
	RefreshWins();

	SigTrap();
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
		mvwprintw(win4.win, logwinline, 0, "%s", logwinbuf[logwinline]);
	}

	wrefresh(win4.win);
	logwinline++;
}


/* -- tremulous functions -- */


void CON_Print ( const char *msg )
{
	if ( logline > LOG_BUF_SIZE )
	{
		logline = 0;
		snprintf(logbuf[logline], 1024, "NOTICE: log buffer filled, starting over");
		LogPrint(logbuf[logline]);
		logline++;
	}

	/* FIXME: add the whole colorization thing, people seem to like that */

	snprintf(logbuf[logline], 1024, "%i: %s", reallogline, msg );
	LogPrint(logbuf[logline]);

	
	logline++;
	reallogline++;
}

void CON_Init ( void )
{
	rootwin = initscr();
	if ( rootwin == NULL )
	{
		printf("Fatal Error: initscr()\n");
		exit(1);
	}

	SetWins();
	InitWins();
	CursResize();
}

void CON_Shutdown ( void )
{
	endwin();
}

char *CON_Input ( void )
{
	return(NULL);
}
