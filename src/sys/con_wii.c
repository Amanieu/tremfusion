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

#include <unistd.h>
#include <string.h>
#include <gctypes.h>
#include <libwiikeyboard/keyboard.h>

#include <sys/time.h>

/* fallbacks for con_curses.c */
#ifdef USE_CURSES
#define TREM_CON_Init TREM_CON_Init_tty
#define CON_Shutdown CON_Shutdown_tty
#define CON_Print CON_Print_tty
#define CON_Input CON_Input_tty
#define CON_Clear_f CON_Clear_tty
#endif

/*
=============================================================
tty console routines

NOTE: if the user is editing a line when something gets printed to the early
console then it won't look good so we provide CON_Hide and CON_Show to be
called before and after a stdout or stderr output
=============================================================
*/

// general flag to tell about keyboard state
static qboolean keyboard_connected = qfalse;
static int ttycon_hide = 0;

// some key codes that the terminal may be using, initialised on start up
static int TTY_erase;
static int TTY_eof;

static field_t TTY_con;

static FILE *con_log;

/*
==================
CON_FlushIn

Flush stdin, I suspect some terminals are sending a LOT of shit
FIXME relevant?
==================
*/
static void CON_FlushIn( void )
{
  char key;
  while (read(0, &key, 1)!=-1);
}

/*
==================
CON_Back

Output a backspace

NOTE: it seems on some terminals just sending '\b' is not enough so instead we
send "\b \b"
(FIXME there may be a way to find out if '\b' alone would work though)
==================
*/
static void CON_Back( void )
{
  char key;
  size_t size;

  key = '\b';
  size = write(1, &key, 1);
  key = ' ';
  size = write(1, &key, 1);
  key = '\b';
  size = write(1, &key, 1);
}

/*
==================
CON_Hide

Clear the display of the line currently edited
bring cursor back to beginning of line
==================
*/
static void CON_Hide( void )
{
  int i;
  if (ttycon_hide)
  {
    ttycon_hide++;
    return;
  }
  if (TTY_con.cursor>0)
  {
    for (i=0; i<TTY_con.cursor; i++)
    {
      CON_Back();
    }
  }
  CON_Back(); // Delete "]"
  ttycon_hide++;
}

/*
==================
CON_Show

Show the current line
FIXME need to position the cursor if needed?
==================
*/
static void CON_Show( void )
{
  int i;
  
  assert(ttycon_hide>0);
  ttycon_hide--;
  if (ttycon_hide == 0)
  {
    size_t size;
    size = write( 1, "]", 1 );
    if (TTY_con.cursor)
    {
      for (i=0; i<TTY_con.cursor; i++)
      {
        size = write(1, TTY_con.buffer+i, 1);
      }
    }
  }
}

/*
==================
CON_Shutdown

Never exit without calling this, or your terminal will be left in a pretty bad state
==================
*/
void CON_Shutdown( void )
{
  CON_Back(); // Delete "]"
  KEYBOARD_Deinit();
  fclose(con_log);
}

/*
==================
CON_Clear_f
==================
*/
void CON_Clear_f( void )
{
  Com_Printf("\033[2J"); /* VT100 clear screen */
  Com_Printf("\033[0;0f"); /* VT100 move cursor to top left */
}

/*
==================
TREM_CON_Init

Initialize the keyboard (if present)
==================
*/
void TREM_CON_Init( void )
{
  int ret;
  ret = KEYBOARD_Init();
  if (ret == 0) {
    keyboard_connected = qfalse;
  }
  else if(ret >= 1) {
    keyboard_connected = qtrue;
  }
  con_log = fopen("/crashlog.txt", "wb");
}

/*
==================
CON_Input
==================
*/
char *CON_Input( void )
{
  // we use this when sending back commands
  static char text[256];
  int avail;
  char key;
  size_t size;
  keyboardEvent event;
  
  KEYBOARD_ScanKeyboards();

  if( 1 )
  {
    if (KEYBOARD_getEvent(&event) && event.type == KEYBOARD_PRESSED)
    {
      KEYBOARD_switchLed(KEYBOARD_LEDCAPS);
      key = event.keysym.ch;
      // we have something
      // backspace?
      // NOTE TTimo testing a lot of values .. seems it's the only way to get it to work everywhere
      if ((key == TTY_erase) || (key == 127) || (key == KEYBOARD_BACKSPACE))
      {
        if (TTY_con.cursor > 0)
        {
          TTY_con.cursor--;
          TTY_con.buffer[TTY_con.cursor] = '\0';
          CON_Back();
        }
        return NULL;
      }
      // check if this is a control char
      if ((key) && (key) < ' ')
      {
        if (key == KEYBOARD_RETURN || key == '\n')
        {
          // push it in history
          Hist_Add(TTY_con.buffer);
          strcpy(text, TTY_con.buffer);
          Field_Clear(&TTY_con);
          key = '\n';
          size = write(1, &key, 1);
          size = write( 1, "]", 1 );
          return text;
        }
        if (key == KEYBOARD_TAB)
        {
          CON_Hide();
          Field_AutoComplete( &TTY_con );
          CON_Show();
          return NULL;
        }
//        avail = read(0, &key, 1);
//        if (avail != -1)
//        {
//          // VT 100 keys
//          if (key == '[' || key == 'O')
//          {
//            avail = read(0, &key, 1);
//            if (avail != -1)
//            {
//              switch (key)
//              {
//                case 'A':
//                  CON_Hide();
//                  Q_strncpyz(TTY_con.buffer, Hist_Prev(), sizeof(TTY_con.buffer));
//                  TTY_con.cursor = strlen(TTY_con.buffer);
//                  CON_Show();
//                  CON_FlushIn();
//                  return NULL;
//                case 'B':
//                  CON_Hide();
//                  Q_strncpyz(TTY_con.buffer, Hist_Next(TTY_con.buffer), sizeof(TTY_con.buffer));
//                  TTY_con.cursor = strlen(TTY_con.buffer);
//                  CON_Show();
//                  CON_FlushIn();
//                  return NULL;
//                case 'C':
//                  return NULL;
//                case 'D':
//                  return NULL;
//              }
//            }
//          }
//        }
        Com_DPrintf("droping ISCTL sequence: %d, TTY_erase: %d\n", key, TTY_erase);
        CON_FlushIn();
        return NULL;
      }
      // push regular character
      TTY_con.buffer[TTY_con.cursor] = key;
      TTY_con.cursor++;
      // print the current line (this is differential)
      size = write(1, &key, 1);
    }

    return NULL;
  }
}

/*
==================
CON_Print
==================
*/
void CON_Print( const char *msg )
{
  printf(msg);
  fputs(msg, con_log);
  fflush(con_log);
//  CON_Hide( );
//
//  if( com_ansiColor && com_ansiColor->integer )
//    Sys_AnsiColorPrint( msg );
//  else
//    fputs( msg, stderr );
//
//  CON_Show( );
}
