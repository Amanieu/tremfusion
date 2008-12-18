/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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
#include "windows.h"

/* fallbacks for con_curses.c */
#ifdef USE_CURSES
#define CON_Init CON_Init_tty
#define CON_Shutdown CON_Shutdown_tty
#define CON_Print CON_Print_tty
#define CON_Input CON_Input_tty
#define CON_Clear_f CON_Clear_tty
#endif

static WORD qconsole_attrib;

// saved console status
static DWORD qconsole_orig_mode;
static CONSOLE_CURSOR_INFO qconsole_orig_cursorinfo;

// current edit buffer
static char qconsole_line[ MAX_EDIT_LINE ];
static int qconsole_linelen = 0;

static HANDLE qconsole_hout;
static HANDLE qconsole_hin;

/*
==================
CON_Clear_f
==================
*/
void CON_Clear_f( void )
{
	CONSOLE_SCREEN_BUFFER_INFO binfo;
	COORD coord = { 0, 0 };
	DWORD count;

	GetConsoleScreenBufferInfo( qconsole_hout, &binfo );
	FillConsoleOutputCharacter( qconsole_hout, (TCHAR) 32, binfo.dwSize.X * binfo.dwSize.Y, coord, &count );
	FillConsoleOutputAttribute( qconsole_hout, binfo.wAttributes, binfo.dwSize.X * binfo.dwSize.Y, coord, &count );
	SetConsoleCursorPosition( qconsole_hout, coord );
}

/*
==================
CON_Show
==================
*/
static void CON_Show( void )
{
	CONSOLE_SCREEN_BUFFER_INFO binfo;
	COORD writeSize = { MAX_EDIT_LINE, 1 };
	COORD writePos = { 0, 0 };
	SMALL_RECT writeArea = { 0, 0, 0, 0 };
	int i;
	CHAR_INFO line[ MAX_EDIT_LINE ];

	GetConsoleScreenBufferInfo( qconsole_hout, &binfo );

	// if we're in the middle of printf, don't bother writing the buffer
	if( binfo.dwCursorPosition.X != 0 )
		return;

	writeArea.Left = 0;
	writeArea.Top = binfo.dwCursorPosition.Y; 
	writeArea.Bottom = binfo.dwCursorPosition.Y; 
	writeArea.Right = MAX_EDIT_LINE;

	// build a space-padded CHAR_INFO array
	for( i = 0; i < MAX_EDIT_LINE; i++ )
	{
		if( i < qconsole_linelen )
			line[ i ].Char.AsciiChar = qconsole_line[ i ];
		else
			line[ i ].Char.AsciiChar = ' ';

		line[ i ].Attributes =  qconsole_attrib;
	}

	if( qconsole_linelen > binfo.srWindow.Right )
	{
		WriteConsoleOutput( qconsole_hout, 
			line + (qconsole_linelen - binfo.srWindow.Right ),
			writeSize, writePos, &writeArea );
	}
	else
	{
		WriteConsoleOutput( qconsole_hout, line, writeSize,
			writePos, &writeArea );
	}
}

/*
==================
CON_Shutdown
==================
*/
void CON_Shutdown( void )
{
	SetConsoleMode( qconsole_hin, qconsole_orig_mode );
	SetConsoleCursorInfo( qconsole_hout, &qconsole_orig_cursorinfo );
	CloseHandle( qconsole_hout );
	CloseHandle( qconsole_hin );
}

/*
==================
CON_Init
==================
*/
void CON_Init( void )
{
	CONSOLE_CURSOR_INFO curs;
	CONSOLE_SCREEN_BUFFER_INFO info;

	qconsole_hin = GetStdHandle( STD_INPUT_HANDLE );
	if( qconsole_hin == INVALID_HANDLE_VALUE )
		return;

	qconsole_hout = GetStdHandle( STD_OUTPUT_HANDLE );
	if( qconsole_hout == INVALID_HANDLE_VALUE )
		return;

	GetConsoleMode( qconsole_hin, &qconsole_orig_mode );

	// allow mouse wheel scrolling
	SetConsoleMode( qconsole_hin,
		qconsole_orig_mode & ~ENABLE_MOUSE_INPUT );

	FlushConsoleInputBuffer( qconsole_hin ); 

	GetConsoleScreenBufferInfo( qconsole_hout, &info );
	qconsole_attrib = info.wAttributes;

	// make cursor invisible
	GetConsoleCursorInfo( qconsole_hout, &qconsole_orig_cursorinfo );
	curs.dwSize = 1;
	curs.bVisible = FALSE;
	SetConsoleCursorInfo( qconsole_hout, &curs );
}

/*
==================
CON_Input
==================
*/
char *CON_Input( void )
{
	INPUT_RECORD buff[ MAX_EDIT_LINE ];
	DWORD count = 0, events = 0;
	WORD key = 0;
	int i;
	int newlinepos = -1;

	if( !GetNumberOfConsoleInputEvents( qconsole_hin, &events ) )
		return NULL;

	if( events < 1 )
		return NULL;
  
	// if we have overflowed, start dropping oldest input events
	if( events >= MAX_EDIT_LINE )
	{
		ReadConsoleInput( qconsole_hin, buff, 1, &events );
		return NULL;
	} 

	if( !ReadConsoleInput( qconsole_hin, buff, events, &count ) )
		return NULL;

	FlushConsoleInputBuffer( qconsole_hin );

	for( i = 0; i < count; i++ )
	{
		if( buff[ i ].EventType != KEY_EVENT )
			continue;
		if( !buff[ i ].Event.KeyEvent.bKeyDown ) 
			continue;

		key = buff[ i ].Event.KeyEvent.wVirtualKeyCode;

		if( key == VK_RETURN )
		{
			newlinepos = i;
			break;
		}
		else if( key == VK_UP )
		{
			Q_strncpyz( qconsole_line, Hist_Prev( ), sizeof( qconsole_line ) );
			qconsole_linelen = strlen( qconsole_line );
			break;
		}
		else if( key == VK_DOWN )
		{
			Q_strncpyz( qconsole_line, Hist_Next( qconsole_line ), sizeof( qconsole_line ) );
			qconsole_linelen = strlen( qconsole_line );
			break;
		}
		else if( key == VK_TAB )
		{
			field_t f;

			Q_strncpyz( f.buffer, qconsole_line,
				sizeof( f.buffer ) );
			Field_AutoComplete( &f );
			Q_strncpyz( qconsole_line, f.buffer,
				sizeof( qconsole_line ) );
			qconsole_linelen = strlen( qconsole_line );
			break;
		}

		if( qconsole_linelen < sizeof( qconsole_line ) - 1 )
		{
			char c = buff[ i ].Event.KeyEvent.uChar.AsciiChar;

			if( key == VK_BACK )
			{
				int pos = ( qconsole_linelen > 0 ) ?
					qconsole_linelen - 1 : 0; 

				qconsole_line[ pos ] = '\0';
				qconsole_linelen = pos;
			}
			else if( c )
			{
				qconsole_line[ qconsole_linelen++ ] = c;
				qconsole_line[ qconsole_linelen ] = '\0'; 
			}
		}
	}

	CON_Show();

	if( newlinepos < 0)
		return NULL;

	if( !qconsole_linelen )
	{
		Com_Printf( "\n" );
		return NULL;
	}

	Hist_Add( qconsole_line );
	Com_Printf( "]%s\n", qconsole_line );

	qconsole_linelen = 0;

	return qconsole_line;
}

/*
==================
CON_Print
==================
*/
void CON_Print( const char *msg )
{
	fputs( msg, stderr );

	CON_Show( );
}
