#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <curses.h>

#define LOG_BUF_SIZE 2048

char _logbuf[LOG_BUF_SIZE][1024];
int _logline = 0;

char *_logwinbuf[LOG_BUF_SIZE];
int _logwinline = 0;

int _reallogline = 0;

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

void _CON_newwin ( _window *win )
{
	win->win = newwin(win->lines, win->cols, win->y, win->x);
}

void _CON_setwins ( void )
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

void _CON_initwins ( void )
{
	_CON_newwin(&win0);
	_CON_newwin(&win1);
	_CON_newwin(&win2);
	_CON_newwin(&win3);
	_CON_newwin(&win4);
	_CON_newwin(&win5);
}

void _CON_refreshwins ( void )
{
	wrefresh(win0.win);
	wrefresh(win1.win);
	wrefresh(win2.win);
	wrefresh(win3.win);
	wrefresh(win4.win);
	wrefresh(win5.win);
}

void _CON_drawwins ( void )
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

	_CON_refreshwins();
}

void _CON_logprint ( char *msg )
{
	int i;

	if (_logwinline >= win4.lines)
	{
		_logwinline--;

		for(i = 0; i < _logwinline; i++)
		{
			_logwinbuf[i] = _logwinbuf[(i + 1)];
		}
		_logwinbuf[i] = msg;

		wclear(win4.win);
		for(i = _logwinline; i >= 0; i--)
		{
			mvwprintw(win4.win, i, 0, "%s", _logwinbuf[i]);
		}
	}
	else
	{
		_logwinbuf[_logwinline] = msg;
		mvwprintw(win4.win, _logwinline, 0, "%s", _logwinbuf[_logwinline]);
	}

	wrefresh(win4.win);
	_logwinline++;
}

void CON_Print ( const char *msg )
{
	if ( _logline > LOG_BUF_SIZE )
	{
		_logline = 0;
		snprintf(_logbuf[_logline], 1024, "NOTICE: log buffer filled, starting over");
		_CON_logprint(_logbuf[_logline]);
		_logline++;
	}

	/* FIXME: add the whole colorization thing, people seem to like that */

	snprintf(_logbuf[_logline], 1024, "%i: %s", _reallogline, msg );
	_CON_logprint(_logbuf[_logline]);

	_logline++;
	_reallogline++;
}

void CON_Init ( void )
{
	rootwin = initscr();
	if ( rootwin == NULL )
	{
		printf("Fatal Error: initscr()\n");
		exit(1);
	}

	_CON_setwins();
	_CON_initwins();

	_CON_drawwins();
}

void CON_Shutdown ( void )
{
	endwin();
}

char *CON_Input ( void )
{
	return(NULL);
}
