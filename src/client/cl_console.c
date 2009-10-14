/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

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
// console.c

#include "client.h"


int g_console_field_width = 78;


#define	NUM_CON_TIMES 4

#define		CON_TEXTSIZE	65536
typedef struct {
	qboolean	initialized;

	short	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line

	int 	linewidth;		// characters across screen
	int		totallines;		// total lines in console scrollback

	float	xadjust;		// for wide aspect screens

	float	displayFrac;	// aproaches finalFrac at scr_conspeed
	float	finalFrac;		// 0.0 to 1.0 lines of console to display

	int		vislines;		// in scanlines
} console_t;

extern	console_t	con;

console_t	con;

cvar_t		*cl_autoNamelog;

cvar_t		*con_skipnotify;

cvar_t		*con_conspeed;

cvar_t		*scr_conUseOld;

// Color and alpha for console
cvar_t		*scr_conUseShader;

cvar_t		*scr_conColorAlpha;
cvar_t		*scr_conColorRed;
cvar_t		*scr_conColorBlue;
cvar_t		*scr_conColorGreen;

cvar_t		*scr_conHeight;

// Color and alpha for bar under console
cvar_t		*scr_conBarSize;

cvar_t		*scr_conBarColorAlpha;
cvar_t		*scr_conBarColorRed;
cvar_t		*scr_conBarColorBlue;
cvar_t		*scr_conBarColorGreen;


#define	DEFAULT_CONSOLE_WIDTH	78

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void) {
	// Can't toggle the console when it's the only thing available
	if ( cls.state == CA_DISCONNECTED && Key_GetCatcher( ) == KEYCATCH_CONSOLE ) {
		return;
	}

	if ( !cl_persistantConsole->integer )
		Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_CONSOLE );
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void) {
	chat_playerNum = -1;
	chat_team = qfalse;
	chat_admins = qfalse;
	chat_clans = qfalse;
	prompt.active = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Q_strncpyz( chatField.buffer, Cmd_Args( ), sizeof( chatField.buffer ) );
	chatField.cursor = strlen( chatField.buffer );
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void) {
	chat_playerNum = -1;
	chat_team = qtrue;
	chat_admins = qfalse;
	chat_clans = qfalse;
	prompt.active = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Q_strncpyz( chatField.buffer, Cmd_Args( ), sizeof( chatField.buffer ) );
	chatField.cursor = strlen( chatField.buffer );
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode3_f
================
*/
void Con_MessageMode3_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_CROSSHAIR_PLAYER );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	chat_admins = qfalse;
	chat_clans = qfalse;
	prompt.active = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Q_strncpyz( chatField.buffer, Cmd_Args( ), sizeof( chatField.buffer ) );
	chatField.cursor = strlen( chatField.buffer );
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode4_f
================
*/
void Con_MessageMode4_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_LAST_ATTACKER );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	chat_admins = qfalse;
	chat_clans = qfalse;
	prompt.active = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Q_strncpyz( chatField.buffer, Cmd_Args( ), sizeof( chatField.buffer ) );
	chatField.cursor = strlen( chatField.buffer );
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode5_f
================
*/
void Con_MessageMode5_f (void) {
	chat_playerNum = -1;
	chat_team = qfalse;
	chat_admins = qtrue;
	chat_clans = qfalse;
	prompt.active = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Q_strncpyz( chatField.buffer, Cmd_Args( ), sizeof( chatField.buffer ) );
	chatField.cursor = strlen( chatField.buffer );
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_Prompt_f
================
*/
void Con_Prompt_f (void) {
	if (Cmd_Argc() < 3)
	{
		Com_Printf ("prompt <callback> [prompt]: Opens the chatbox, store the text in ui_sayBuffer and then vstr callback\n");
		return;
	}

	chat_playerNum = -1;
	chat_team = qfalse;
	chat_admins = qfalse;
	chat_clans = qfalse;
	prompt.active = qtrue;

	strcpy(prompt.callback, Cmd_Argv(1));

	// copy the rest of the command line
	Q_strncpyz(prompt.question, Cmd_ArgsFrom(2), sizeof(prompt.question));
	
	Field_Clear( &chatField );
	chatField.widthInChars = 34 - strlen(prompt.question);

	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_MessageMode6_f
================
*/
void Con_MessageMode6_f (void) {
	chat_playerNum = -1;
	chat_team = qfalse;
	chat_admins = qfalse;
	chat_clans = qtrue;
	prompt.active = qfalse;
	Field_Clear( &chatField );
	Q_strncpyz( chatField.buffer, Cmd_Args( ), sizeof( chatField.buffer ) );
	chatField.cursor = strlen( chatField.buffer );
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void) {
	int		i;

	for ( i = 0 ; i < CON_TEXTSIZE ; i++ ) {
		con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}

	Con_Bottom();		// go to end

	CON_Clear_f();		// clear the tty too
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f (void)
{
	int		l, x, i;
	short	*line;
	fileHandle_t	f;
	char	buffer[1024];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: condump <filename>\n");
		return;
	}

	Com_Printf ("Dumped console text to %s.\n", Cmd_Argv(1) );

	f = FS_FOpenFileWrite( Cmd_Argv( 1 ) );
	if (!f)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1 ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if ((line[x] & 0xff) != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;
	for ( ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for(i=0; i<con.linewidth; i++)
			buffer[i] = line[i] & 0xff;
		for (x=con.linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		strcat( buffer, "\n" );
		FS_Write(buffer, strlen(buffer), f);
	}

	FS_FCloseFile( f );
}

/*
================
Con_Search_f

Scroll up to the first console line containing a string
================
*/
void Con_Search_f (void)
{
	int		l, i, x;
	short	*line;
	char	buffer[MAXPRINTMSG];
	int		direction;
	int		c = Cmd_Argc();

	if (c < 2) {
		Com_Printf ("usage: %s <string1> <string2> <...>\n", Cmd_Argv(0));
		return;
	}

	if (!Q_stricmp(Cmd_Argv(0), "searchDown")) {
		direction = 1;
	} else {
		direction = -1;
	}

	// check the lines
	buffer[con.linewidth] = 0;
	for (l = con.display - 1 + direction; l <= con.current && con.current - l < con.totallines; l += direction) {
		line = con.text + (l%con.totallines)*con.linewidth;
		for (i = 0; i < con.linewidth; i++)
			buffer[i] = line[i] & 0xff;
		for (x = con.linewidth - 1 ; x >= 0 ; x--) {
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		// Don't search commands
		if (!Q_stricmpn(buffer, Q_CleanStr(va("%s", cl_consolePrompt->string)), Q_PrintStrlen(cl_consolePrompt->string)))
			continue;
		for (i = 1; i < c; i++) {
			if (Q_stristr(buffer, Cmd_Argv(i))) {
				con.display = l + 1;
				if (con.display > con.current)
					con.display = con.current;
				return;
			}
		}
	}
}

/*
================
Con_Grep_f

Find all console lines containing a string
================
*/
void Con_Grep_f (void)
{
	int		l, x, i;
	short	*line;
	char	buffer[1024];
	char	buffer2[1024];
	char	printbuf[CON_TEXTSIZE];
	char	*search;
	char	lastcolor;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: grep <string>\n");
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1 ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if ((line[x] & 0xff) != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// check the remaining lines
	buffer[con.linewidth] = 0;
	search = Cmd_Argv( 1 );
	printbuf[0] = '\0';
	lastcolor = 7;
	for ( ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for(i=0,x=0; i<con.linewidth; i++)
		{
			if (line[i] >> 8 != lastcolor)
			{
				lastcolor = line[i] >> 8;
				buffer[x++] = Q_COLOR_ESCAPE;
				buffer[x++] = lastcolor + '0';
			}
			buffer[x++] = line[i] & 0xff;
		}
		for (x=con.linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		// Don't search commands
		if (!Q_stricmpn(buffer, cl_consolePrompt->string, strlen(cl_consolePrompt->string)))
			continue;
		strcpy(buffer2, buffer);
		Q_CleanStr(buffer2);
		if (Q_stristr(buffer2, search))
		{
			strcat( printbuf, buffer );
			strcat( printbuf, "\n" );
		}
	}
	if ( printbuf[0] )
		Com_Printf( "%s", printbuf );
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void ) {
	Cmd_TokenizeString( NULL );
	CL_GameConsoleText( );
}

						

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	short	tbuf[CON_TEXTSIZE];

	if (cls.glconfig.vidWidth) {
		if (scr_conUseOld->integer) {
			width = cls.glconfig.vidWidth / SCR_ConsoleFontCharWidth('W');
		} else {
			width = (cls.glconfig.vidWidth - 30) / SCR_ConsoleFontCharWidth('W');
		}
		g_consoleField.widthInChars = width - Q_PrintStrlen(cl_consolePrompt->string) - 1;
	} else {
		width = 0;
	}

	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = DEFAULT_CONSOLE_WIDTH;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		for(i=0; i<CON_TEXTSIZE; i++)

			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;
	
		if (con.linewidth < numchars)
			numchars = con.linewidth;

		Com_Memcpy (tbuf, con.text, CON_TEXTSIZE * sizeof(short));
		for(i=0; i<CON_TEXTSIZE; i++)

			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';


		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}

/*
==================
Cmd_CompleteTxtName
==================
*/
void Cmd_CompleteTxtName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "", "txt", qfalse );
	}
}


/*
================
Con_Init
================
*/
void Con_Init (void) {
	cl_autoNamelog = Cvar_Get ("cl_autoNamelog", "0", CVAR_ARCHIVE);
	
	con_conspeed = Cvar_Get ("scr_conspeed", "3", 0);
	
	con_skipnotify = Cvar_Get ("con_skipnotify", "0", 0);
	
	scr_conUseOld = Cvar_Get ("scr_conUseOld", "0", CVAR_ARCHIVE|CVAR_LATCH);
	
	// Defines cvar for color and alpha for console/bar under console
	scr_conUseShader = Cvar_Get ("scr_conUseShader", "0", CVAR_ARCHIVE);
	
	scr_conColorAlpha = Cvar_Get ("scr_conColorAlpha", "0.75", CVAR_ARCHIVE);
	scr_conColorRed = Cvar_Get ("scr_conColorRed", "0", CVAR_ARCHIVE);
	scr_conColorBlue = Cvar_Get ("scr_conColorBlue", "0.1", CVAR_ARCHIVE);
	scr_conColorGreen = Cvar_Get ("scr_conColorGreen", "0", CVAR_ARCHIVE);

	scr_conHeight = Cvar_Get ("scr_conHeight", "50", CVAR_ARCHIVE);
	
	scr_conBarSize = Cvar_Get ("scr_conBarSize", "2", CVAR_ARCHIVE);
	
	scr_conBarColorAlpha = Cvar_Get ("scr_conBarColorAlpha", "1", CVAR_ARCHIVE);
	scr_conBarColorRed = Cvar_Get ("scr_conBarColorRed", "1", CVAR_ARCHIVE);
	scr_conBarColorBlue = Cvar_Get ("scr_conBarColorBlue", "0", CVAR_ARCHIVE);
	scr_conBarColorGreen = Cvar_Get ("scr_conBarColorGreen", "0", CVAR_ARCHIVE);
	// Done defining cvars for console colors
	
	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("messagemode3", Con_MessageMode3_f);
	Cmd_AddCommand ("messagemode4", Con_MessageMode4_f);
	Cmd_AddCommand ("messagemode5", Con_MessageMode5_f);
	Cmd_AddCommand ("messagemode6", Con_MessageMode6_f);
	Cmd_AddCommand ("prompt", Con_Prompt_f);
	Cmd_SetCommandCompletionFunc( "prompt", Cvar_CompleteCvarName );
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	Cmd_SetCommandCompletionFunc( "condump", Cmd_CompleteTxtName );
	Cmd_AddCommand ("search", Con_Search_f);
	Cmd_AddCommand ("searchDown", Con_Search_f);
	Cmd_AddCommand ("grep", Con_Grep_f);
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (qboolean skipnotify)
{
	int		i;

	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	for(i=0; i<con.linewidth; i++)
		con.text[(con.current%con.totallines)*con.linewidth+i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
}

/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void CL_ConsolePrint( char *txt ) {
	int		y;
	int		c, l;
	int		color;
	qboolean skipnotify = qfalse;		// NERVE - SMF
	static qboolean is_new_line = qtrue;
	
	CL_WriteClientChatLog( txt );
	
/* auto-namelog code */
    if (( strstr(txt, "^7 connected\n") != NULL ) && !clc.demoplaying && cl_autoNamelog->integer)
	{
	char *p;
	char text[MAX_SAY_TEXT];

	Q_strncpyz( text, txt, MAX_SAY_TEXT );
	p = strstr(text, "^7 connected\n");
	if (p)
		{
		char buf[MAX_SAY_TEXT];

		*p = '\0';

		Com_sprintf( buf, sizeof(buf), "!namelog %s", text );
		CL_AddReliableCommand( buf, qfalse );
		}
	}
/* end auto-namelog code */
	
	// TTimo - prefix for text that shows up in console but not in notify
	// backported from RTCW
	if ( !Q_strncmp( txt, "[skipnotify]", 12 ) ) {
		skipnotify = qtrue;
		txt += 12;
	}
	
	// for some demos we don't want to ever show anything on the console
	if ( cl_noprint && cl_noprint->integer ) {
		return;
	}
	
	if (!con.initialized) {
		con.linewidth = -1;
		Con_CheckResize ();
		con.initialized = qtrue;
	}

	if( !skipnotify && (!con_skipnotify || !con_skipnotify->integer) ) {
		Cmd_SaveCmdContext( );

		// feed the text to cgame
		if( is_new_line && ( com_timestamps && com_timestamps->integer ) )
			Cmd_TokenizeString( txt + 16 );
		else
			Cmd_TokenizeString( txt );
		CL_GameConsoleText( );

		Cmd_RestoreCmdContext( );
	}

	color = ColorIndex(COLOR_WHITE);

	while ( (c = *txt) != 0 ) {
		if ( Q_IsColorString( txt ) ) {
			color = ColorIndex( *(txt+1) );
			txt += 2;
			continue;
		}

		// count word length
		for (l=0 ; l< con.linewidth ; l++) {
			if ( txt[l] <= ' ' && txt[l] >= 0) {
				break;
			}

		}

		// word wrap
		if (l != con.linewidth && (con.x + l >= con.linewidth) ) {
			Con_Linefeed(skipnotify);

		}

		txt++;

		switch (c)
		{
		case '\n':
			Con_Linefeed (skipnotify);
			break;
		case '\r':
			con.x = 0;
			break;
		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = (color << 8) | (unsigned char)c;
			con.x++;
			if (con.x >= con.linewidth) {
				Con_Linefeed(skipnotify);
				con.x = 0;
			}
			break;
		}
	}
	is_new_line = txt[strlen(txt) - 1] == '\n';
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
void Con_DrawInput (void) {
	int		y;
	char	prompt[ MAX_STRING_CHARS ];
	vec4_t	color;
	qtime_t realtime;

	if ( cls.state != CA_DISCONNECTED && !(Key_GetCatcher( ) & KEYCATCH_CONSOLE ) ) {
		return;
	}

	Com_RealTime( &realtime );

	y = con.vislines - ( SCR_ConsoleFontCharHeight() * 2 ) + 2 ;

	Com_sprintf( prompt,  sizeof( prompt ), "^0[^3%02d%c%02d^0]^7 %s", realtime.tm_hour, (realtime.tm_sec & 1) ? ':' : ' ', realtime.tm_min, cl_consolePrompt->string );

	color[0] = 1.0f;
	color[1] = 1.0f;
	color[2] = 1.0f;
	color[3] = (scr_conUseOld->integer ? 1.0f : con.displayFrac * 2.0f);

	SCR_DrawSmallStringExt( con.xadjust + cl_conXOffset->integer, y, prompt, color, qfalse, qfalse );

	Q_CleanStr( prompt );
	Field_Draw( &g_consoleField, con.xadjust + cl_conXOffset->integer + SCR_ConsoleFontStringWidth(prompt, strlen(prompt)), y, qtrue, qtrue, color[3] );
}

/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole( float frac ) {
	int				i, x, y;
	int				rows;
	short			*text;
	int				row;
	int				lines;
//	qhandle_t		conShader;
	int				currentColor;
	vec4_t			color;

	if (scr_conUseOld->integer) {
		lines = cls.glconfig.vidHeight * frac;
		if (lines <= 0)
			return;

		if (lines > cls.glconfig.vidHeight )
			lines = cls.glconfig.vidHeight;
	} else {
		lines = cls.glconfig.vidHeight * scr_conHeight->integer * 0.01 - 10;
	}

	// on wide screens, we will center the text
	if (!scr_conUseOld->integer) {
		con.xadjust = 15;
	}
	SCR_AdjustFrom640( &con.xadjust, NULL, NULL, NULL );

	// draw the background
	if (scr_conUseOld->integer) {
		y = frac * SCREEN_HEIGHT;
		if ( y < 1 ) {
			y = 0;
		}
		else {
		 if( scr_conUseShader->integer )
		   {
			SCR_DrawPic( 0, 0, SCREEN_WIDTH, y, cls.consoleShader );
		   }
		 else
		   {
			// This will be overwrote, so ill just abuse it here, no need to define another array
			color[0] = scr_conColorRed->value;
			color[1] = scr_conColorGreen->value;
			color[2] = scr_conColorBlue->value;
			color[3] = scr_conColorAlpha->value;
			
			SCR_FillRect( 0, 0, SCREEN_WIDTH, y, color );
		   }
		}

		color[0] = scr_conBarColorRed->value;
		color[1] = scr_conBarColorGreen->value;
		color[2] = scr_conBarColorBlue->value;
		color[3] = scr_conBarColorAlpha->value;
		
		SCR_FillRect( 0, y, SCREEN_WIDTH, scr_conBarSize->value, color );
	} else {
		color[0] = scr_conColorRed->value;
		color[1] = scr_conColorGreen->value;
		color[2] = scr_conColorBlue->value;
		color[3] = frac * 2 * scr_conColorAlpha->value;
		SCR_FillRect(10, 10, 620, 460 * scr_conHeight->integer * 0.01, color);

		color[0] = scr_conBarColorRed->value;
		color[1] = scr_conBarColorGreen->value;
		color[2] = scr_conBarColorBlue->value;
		color[3] = frac * 2 * scr_conBarColorAlpha->value;
		SCR_FillRect(10, 10, 620, 1, color);	//top
		SCR_FillRect(10, 460 * scr_conHeight->integer * 0.01 + 10, 621, 1, color);	//bottom
		SCR_FillRect(10, 10, 1, 460 * scr_conHeight->integer * 0.01, color);	//left
		SCR_FillRect(630, 10, 1, 460 * scr_conHeight->integer * 0.01, color);	//right
	}


	// draw the version number

	color[0] = 1.0f;
	color[1] = 0.0f;
	color[2] = 0.0f;
	color[3] = (scr_conUseOld->integer ? 1.0f : frac * 2.0f);
	re.SetColor( color );

	i = strlen( Q3_VERSION );
    float totalwidth = SCR_ConsoleFontStringWidth( Q3_VERSION, i ) + cl_conXOffset->integer;
	if (!scr_conUseOld->integer) {
		totalwidth += 30;
	}
    float currentWidthLocation = 0;
	for (x=0 ; x<i ; x++) {
        SCR_DrawConsoleFontChar( cls.glconfig.vidWidth - totalwidth + currentWidthLocation, lines-SCR_ConsoleFontCharHeight(), Q3_VERSION[x] );
        currentWidthLocation += SCR_ConsoleFontCharWidth( Q3_VERSION[x] );
	}


	// draw the text
	con.vislines = lines;
	rows = (lines)/SCR_ConsoleFontCharHeight() - 3;		// rows of text to draw
	if (scr_conUseOld->integer)
		rows++;

	y = lines - (SCR_ConsoleFontCharHeight()*3);

	// draw from the bottom up
	if (con.display != con.current)
	{
		// draw arrows to show the buffer is backscrolled
		color[0] = 1.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = (scr_conUseOld->integer ? 1.0f : frac * 2.0f);
	    re.SetColor( color );
        for (x=0 ; x<con.linewidth - (scr_conUseOld->integer ? 0 : 4); x+=4)
            SCR_DrawConsoleFontChar( con.xadjust + (x+1)*SCR_ConsoleFontCharWidth('^'), y, '^' );
        y -= SCR_ConsoleFontCharHeight();
        rows--;
	}
	
	row = con.display;

	if ( con.x == 0 ) {
		row--;
	}

	currentColor = 7;
	color[0] = g_color_table[currentColor][0];
	color[1] = g_color_table[currentColor][1];
	color[2] = g_color_table[currentColor][2];
	color[3] = (scr_conUseOld->integer ? 1.0f : frac * 2.0f);
	re.SetColor( color );

	for (i=0 ; i<rows ; i++, y -= SCR_ConsoleFontCharHeight(), row--)
	{
		if (row < 0)
			break;
		if (con.current - row >= con.totallines) {
			// past scrollback wrap point
			continue;	
		}

		text = con.text + (row % con.totallines)*con.linewidth;

        float currentWidthLocation = cl_conXOffset->integer;
		for (x=0 ; x<con.linewidth ; x++) {
			if ( ( (text[x]>>8)&7 ) != currentColor ) {
				currentColor = (text[x]>>8)&7;
				color[0] = g_color_table[currentColor][0];
				color[1] = g_color_table[currentColor][1];
				color[2] = g_color_table[currentColor][2];
				color[3] = (scr_conUseOld->integer ? 1.0f : frac * 2.0f);
				re.SetColor( color );
			}
            
            SCR_DrawConsoleFontChar(  con.xadjust + currentWidthLocation, y, text[x] & 0xff );
            currentWidthLocation += SCR_ConsoleFontCharWidth( text[x] & 0xff );
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();

	re.SetColor( NULL );
}



/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void ) {
	// check for console width changes from a vid mode change
	Con_CheckResize ();

	// if disconnected, render console full screen
	if ( cls.state == CA_DISCONNECTED ) {
		if ( !( Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME)) ) {
			Con_DrawSolidConsole( 1.0 );
			return;
		}
	}

	if ( con.displayFrac ) {
		Con_DrawSolidConsole( con.displayFrac );
	}

	if( Key_GetCatcher( ) & ( KEYCATCH_UI | KEYCATCH_CGAME ) )
		return;

	// draw the chat line
	if( Key_GetCatcher( ) & KEYCATCH_MESSAGE )
	{
		int skip;

		if( chat_team )
		{
			SCR_DrawBigString( 8, 232, "Team Say:", 1.0f, qfalse );
			skip = 11;
		}
		else if( chat_admins )
		{
			SCR_DrawBigString( 8, 232, "Admin Say:", 1.0f, qfalse );
			skip = 11;
		}
		else if (prompt.active)
		{ 
			SCR_DrawBigString( 8, 232, prompt.question, 1.0f, qfalse );
			skip = strlen(prompt.question) + 1;	
		}
		else if( chat_clans )
		{
			SCR_DrawBigString( 8, 232, "Clan Say:", 1.0f, qfalse );
			skip = 11;
		}
		else
		{ 
			SCR_DrawBigString( 8, 232, "Say:", 1.0f, qfalse );
			skip = 5;
		}

		Field_BigDraw( &chatField, skip * BIGCHAR_WIDTH, 232, qtrue, qtrue );
	}
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole (void) {
	// decide on the destination height of the console
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
		if (scr_conUseOld->integer) {
			con.finalFrac = MAX(0.10, 0.01 * scr_conHeight->integer);  // configured console percentage
		} else {
			con.finalFrac = 0.5;
		}
	else
		con.finalFrac = 0;				// none visible
	
	// scroll towards the destination height
	if (con.finalFrac < con.displayFrac)
	{
		con.displayFrac -= con_conspeed->value*cls.realFrametime*0.001;
		if (con.finalFrac > con.displayFrac)
			con.displayFrac = con.finalFrac;

	}
	else if (con.finalFrac > con.displayFrac)
	{
		con.displayFrac += con_conspeed->value*cls.realFrametime*0.001;
		if (con.finalFrac < con.displayFrac)
			con.displayFrac = con.finalFrac;
	}

}


void Con_PageUp( void ) {
	con.display -= 2;
	if ( con.current - con.display >= con.totallines ) {
		con.display = con.current - con.totallines + 1;
	}
}

void Con_PageDown( void ) {
	con.display += 2;
	if (con.display > con.current) {
		con.display = con.current;
	}
}

void Con_Top( void ) {
	con.display = con.totallines;
	if ( con.current - con.display >= con.totallines ) {
		con.display = con.current - con.totallines + 1;
	}
}

void Con_Bottom( void ) {
	con.display = con.current;
}


void Con_Close( void ) {
	if ( !com_cl_running->integer ) {
		return;
	}
	if ( !cl_persistantConsole->integer )
		Field_Clear( &g_consoleField );
	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CONSOLE );
	con.finalFrac = 0;				// none visible
	con.displayFrac = 0;
}
