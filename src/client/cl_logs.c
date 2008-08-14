/*
===========================================================================
This file has been written for TREMULOUS

do what you like with it just keep it open source and free
===========================================================================
*/
// cl_logs.c  -- client logging functions (Written by slacker)

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
			nowString = va( "%02d-%02d-%04d", now.tm_mday, 1 + now.tm_mon, 1900 + now.tm_year );
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
			Com_DecolorString( text, NoColorMsg, sizeof(NoColorMsg) );
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
				Com_DecolorString( text, NoColorMsg, sizeof(NoColorMsg) );
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
