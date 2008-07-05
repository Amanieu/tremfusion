#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <curses.h>
#include <signal.h>

#include <sys/ioctl.h>

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

int root_y, root_x;

WINDOW *rootwin;

_window win0;
_window win1;
_window win2;
_window win3;
_window win4;
_window win5;

/* -- prototypes -- */

/* helper functions */
static void NewWin ( _window * );
static void ResizeWin ( _window * );
static void SetWins ( void );
static void InitWins ( void );
static void ResizeWins ( void );
static void RefreshWins ( void );
static void DrawWins ( void );
static void ClearWins ( void );
static void CursResize ( void );

static void LogPrint ( char * );
static void LogRedraw ( void );

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
	//getmaxyx(rootwin, root_y, root_x);

	struct winsize winsz = { 0, };

	ioctl (fileno (stdout), TIOCGWINSZ, &winsz);

	root_y = winsz.ws_row;
	root_x = winsz.ws_col;

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

	/*FIXME: experamental codes */
	clearok(win0.win, TRUE);
	clearok(win1.win, TRUE);
	clearok(win2.win, TRUE);
	clearok(win3.win, TRUE);
	clearok(win4.win, TRUE);
	clearok(win5.win, TRUE);
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

	/*FIXME: figure this out */
	mvwin(win5.win, win5.y, win5.x);
}


/* refresh all of the windows */
static void RefreshWins ( void )
{
	wrefresh(rootwin);

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
	mvwprintw(win5.win, 0, 0, "# rm -rf /");
}

/* clear all of the windows */
static void ClearWins ( void )
{
	wclear(rootwin);

	wclear(win0.win);
	wclear(win1.win);
	wclear(win2.win);
	wclear(win3.win);
	wclear(win4.win);
	wclear(win5.win);
}

/* resize the windows, redraw them, and set the bear trap */
/* FIXME: make this work 100% of the time (currently at um... 95%?) */
/* FIXME: weerd 5th window bug now */
static void CursResize ( void )
{
	signal(SIGWINCH, (void*) CursResize);

	endwin();
	ClearWins();
	SetWins();
	ResizeWins();
	ClearWins();
	DrawWins();
	LogRedraw();
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
		mvwprintw(win4.win, logwinline, 0, "%s", logwinbuf[logwinline]);
	}

	wrefresh(win4.win);
	logwinline++;
}

static void LogRedraw ( void )
{
	int i;

	wclear(win4.win);
	for(i = (logwinline - 1); i >= 0; i--)
	{
		mvwprintw(win4.win, i, 0, "%s", logwinbuf[i]);
	}
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

	FILE *fd = fopen("/tmp/out", "a");
	fprintf(fd, "CON_Init()\n");
	fclose(fd);
}

void CON_Shutdown ( void )
{
	endwin();
}

char *CON_Input ( void )
{
	return(NULL);
}
