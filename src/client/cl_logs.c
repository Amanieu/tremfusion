/*
===========================================================================
Copyright (C) 2008 Slacker

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
// cl_logs.c  -- client logging functions

#include "client.h"

static fileHandle_t  LogFileHandle;
qboolean             LogFileOpened = qfalse;

/*
==================
CL_OpenClientLog
==================
*/
void CL_OpenClientLog(void) {
	if( cl_logs && cl_logs->integer ) {
		if( FS_Initialized() && LogFileOpened == qfalse ) {
			char FileLocation[ 255 ];
			char *nowString;
			//get current time/date info
			qtime_t now;
			Com_RealTime( &now );
			nowString = va( "%04d-%02d-%02d", 1900 + now.tm_year, 1 + now.tm_mon, now.tm_mday );
			sprintf(FileLocation,"logs/%s.log", nowString );
			LogFileHandle = FS_FOpenFileAppend( FileLocation ); //open file with filename as date
			LogFileOpened = qtrue; //var to tell if files open
		}
	}
}

/*
==================
CL_CloseClientLog
==================
*/
void CL_CloseClientLog(void) {
	if( cl_logs && cl_logs->integer ) {
		if( FS_Initialized() && LogFileOpened == qtrue ) {
			//close the file
			FS_FCloseFile( LogFileHandle );
			LogFileOpened = qfalse; //var to tell if files open
		}
	}
}

/*
==================
CL_WriteClientLog
==================
*/
void CL_WriteClientLog( char *text ) {
	if( cl_logs && cl_logs->integer ) {
		if( LogFileOpened == qfalse || !LogFileHandle ) {
			CL_OpenClientLog();
		}
		if( FS_Initialized() && LogFileOpened == qtrue ) {
			// varibles
			char NoColorMsg[MAXPRINTMSG];
			//decolor the string
			Q_strncpyz( NoColorMsg, text, sizeof(NoColorMsg) );
			Q_CleanStr( NoColorMsg );
			//write to the file
			FS_Write( NoColorMsg, strlen( NoColorMsg ), LogFileHandle );
			//flush the file so we can see the data
			FS_ForceFlush( LogFileHandle );
		}
	}
}
/*
==================
CL_WriteClientChatLog
==================
*/
void CL_WriteClientChatLog( char *text ) {
	if( cl_logs && cl_logs->integer ) {
		if( LogFileOpened == qfalse || !LogFileHandle ) {
			CL_OpenClientLog();
		}
		if( FS_Initialized() && LogFileOpened == qtrue ) {
			if( cl.serverTime > 0 ) {
				// varibles
				char NoColorMsg[MAXPRINTMSG];
				char Timestamp[ 60 ];
				char LogText[ 60 + MAXPRINTMSG ];
				if( cl_logs->integer == 1 ) {
					//just do 3 stars to seperate from normal logging stuff
					Q_strncpyz( Timestamp, "***", sizeof( Timestamp ) );
				} else if( cl_logs->integer == 2 ) {
					//just game time
					//do timestamp prep
					sprintf( Timestamp, "[%d:%02d]", cl.serverTime / 60000, ( cl.serverTime / 1000 ) % 60 ); //server Time
					
				} else if( cl_logs->integer == 3 ) {
					//just system time
					//do timestamp prep
					//get current time/date info
					qtime_t now;
					Com_RealTime( &now );
					sprintf( Timestamp, "[%d:%02d]", now.tm_hour, now.tm_min ); //server Time
				} else if( cl_logs->integer == 4 ) {
					//all the data
					//do timestamp prep
					//get current time/date info
					qtime_t now;
					Com_RealTime( &now );
					sprintf( Timestamp, "[%d:%02d][%d:%02d]", now.tm_hour, now.tm_min, cl.serverTime / 60000, ( cl.serverTime / 1000 ) % 60 ); //server Time
				}
				//decolor the string
				Q_strncpyz( NoColorMsg, text, sizeof(NoColorMsg) );
				Q_CleanStr( NoColorMsg );
				//prepare text for log
				sprintf( LogText, "%s%s", Timestamp, NoColorMsg ); //thing to write to log
				//write to the file
				FS_Write( LogText, strlen( LogText ), LogFileHandle );
				//flush the file so we can see the data
				FS_ForceFlush( LogFileHandle );
			}
		}
	}
}
