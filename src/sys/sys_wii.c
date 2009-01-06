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

#define __COLOR_H__

#include <stdio.h>
#include <stdlib.h>
#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/consol.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <libwiikeyboard/keyboard.h>
#include <dirent.h>
#include <fat.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <pwd.h>
#include <libgen.h>

// Used to determine where to store user-specific files
static char homePath[ MAX_OSPATH ] = { 0 };

/*
==================
Sys_DefaultHomePath
==================
*/
char *Sys_DefaultHomePath(char **path2)
{
    char *p;

    if( !*homePath )
    {

            Q_strcat( homePath, sizeof( homePath ), "/apps/tremded-wii/home/");
    }

    *path2 = NULL;
    return homePath;
}

/*
=================
Sys_ResolveLink
=================
*/
char *Sys_ResolveLink( char *arg0 )
{
    return arg0;
}

/*
================
Sys_Milliseconds
================
*/

long long sys_timeBase = 0;
int curtime;
int Sys_Milliseconds (void)
{
    if (!sys_timeBase)
    {
        sys_timeBase = gettime();
        return ticks_to_millisecs(gettime());
    }

    curtime = diff_msec(sys_timeBase, gettime());

    return curtime;
}

#if !id386
/*
==================
fastftol
==================
*/
long fastftol( float f )
{
    return (long)f;
}

/*
==================
Sys_SnapVector
==================
*/
void Sys_SnapVector( float *v )
{
    v[0] = rint(v[0]);
    v[1] = rint(v[1]);
    v[2] = rint(v[2]);
}
#endif


/*
==================
Sys_RandomBytes
==================
*/
qboolean Sys_RandomBytes( byte *string, int len )
{
    FILE *fp;

    fp = fopen( "/dev/urandom", "r" );
    if( !fp )
        return qfalse;

    if( !fread( string, sizeof( byte ), len, fp ) )
    {
        fclose( fp );
        return qfalse;
    }

    fclose( fp );
    return qtrue;
}

/*
==================
Sys_GetCurrentUser
==================
*/
char *Sys_GetCurrentUser( void )
{
    struct passwd *p;

//    if ( (p = getpwuid( getuid() )) == NULL ) {
        return "player";
//    }
//    return p->pw_name;
}

/*
==================
Sys_GetClipboardData
==================
*/
char *Sys_GetClipboardData(void)
{
    return NULL;
}

#define MEM_THRESHOLD 96*1024*1024

/*
==================
Sys_LowPhysicalMemory

TODO
==================
*/
qboolean Sys_LowPhysicalMemory( void )
{
    return qfalse;
}

/*
==================
Sys_Basename
==================
*/
const char *Sys_Basename( char *path )
{
    return basename( path );
}

/*
==================
Sys_Dirname
==================
*/
const char *Sys_Dirname( char *path )
{
  // from dirname.c
  char *newpath;
  const char *slash;
  int length;     /* Length of result, not including NUL.  */

  slash = strrchr (path, '/');
  if (slash == 0)
  {
    /* File is in the current directory.  */
    path = ".";
    length = 1;
  }
  else
  {
    /* Remove any trailing slashes from the result.  */
    while (slash > path && *slash == '/')
      --slash;
    length = slash - path + 1;
  }
  newpath = (char *) malloc (length + 1);
  if (newpath == 0)
    return 0;
  strncpy (newpath, path, length);
  newpath[length] = 0;
  return newpath;

}

/*
==================
Sys_Mkdir
==================
*/
void Sys_Mkdir( const char *path )
{
    mkdir( path, 0777 );
}

/*
==================
Sys_Cwd
==================
*/
char *Sys_Cwd( void )
{
    static char cwd[MAX_OSPATH];

    char *result = getcwd( cwd, sizeof( cwd ) - 1 );
    if( result != cwd )
        return NULL;

    cwd[MAX_OSPATH-1] = 0;

    return cwd;
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==================
Sys_ListFilteredFiles
==================
*/
void Sys_ListFilteredFiles( const char *basedir, char *subdirs, char *filter, char **list, int *numfiles )
{
    char          search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
    char          filename[MAX_OSPATH];
    DIR           *fdir;
    struct dirent *d;
    struct stat   st;

    if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
        return;
    }

    if (strlen(subdirs)) {
        Com_sprintf( search, sizeof(search), "%s/%s", basedir, subdirs );
    }
    else {
        Com_sprintf( search, sizeof(search), "%s", basedir );
    }

    if ((fdir = opendir(search)) == NULL) {
        return;
    }

    while ((d = readdir(fdir)) != NULL) {
        Com_sprintf(filename, sizeof(filename), "%s/%s", search, d->d_name);
        if (stat(filename, &st) == -1)
            continue;

        if (st.st_mode & S_IFDIR) {
            if (Q_stricmp(d->d_name, ".") && Q_stricmp(d->d_name, "..")) {
                if (strlen(subdirs)) {
                    Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s/%s", subdirs, d->d_name);
                }
                else {
                    Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s", d->d_name);
                }
                Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
            }
        }
        if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
            break;
        }
        Com_sprintf( filename, sizeof(filename), "%s/%s", subdirs, d->d_name );
        if (!Com_FilterPath( filter, filename, qfalse ))
            continue;
        list[ *numfiles ] = CopyString( filename );
        (*numfiles)++;
    }

    closedir(fdir);
}

/*
==================
Sys_ListFiles
==================
*/
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs )
{
    struct dirent *d;
    DIR           *fdir;
    qboolean      dironly = wantsubs;
    char          search[MAX_OSPATH];
    int           nfiles;
    char          **listCopy;
    char          *list[MAX_FOUND_FILES];
    int           i;
    struct stat   st;

    int           extLen;

    if (filter) {

        nfiles = 0;
        Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

        list[ nfiles ] = NULL;
        *numfiles = nfiles;

        if (!nfiles)
            return NULL;

        listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
        for ( i = 0 ; i < nfiles ; i++ ) {
            listCopy[i] = list[i];
        }
        listCopy[i] = NULL;

        return listCopy;
    }

    if ( !extension)
        extension = "";

    if ( extension[0] == '/' && extension[1] == 0 ) {
        extension = "";
        dironly = qtrue;
    }

    extLen = strlen( extension );

    // search
    nfiles = 0;

    if ((fdir = opendir(directory)) == NULL) {
        *numfiles = 0;
        return NULL;
    }

    while ((d = readdir(fdir)) != NULL) {
        Com_sprintf(search, sizeof(search), "%s/%s", directory, d->d_name);
        if (stat(search, &st) == -1)
            continue;
        if ((dironly && !(st.st_mode & S_IFDIR)) ||
            (!dironly && (st.st_mode & S_IFDIR)))
            continue;

        if (*extension) {
            if ( strlen( d->d_name ) < strlen( extension ) ||
                Q_stricmp(
                    d->d_name + strlen( d->d_name ) - strlen( extension ),
                    extension ) ) {
                continue; // didn't match
            }
        }

        if ( nfiles == MAX_FOUND_FILES - 1 )
            break;
        list[ nfiles ] = CopyString( d->d_name );
        nfiles++;
    }

    list[ nfiles ] = NULL;

    closedir(fdir);

    // return a copy of the list
    *numfiles = nfiles;

    if ( !nfiles ) {
        return NULL;
    }

    listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
    for ( i = 0 ; i < nfiles ; i++ ) {
        listCopy[i] = list[i];
    }
    listCopy[i] = NULL;

    return listCopy;
}

/*
==================
Sys_FreeFileList
==================
*/
void Sys_FreeFileList( char **list )
{
    int i;

    if ( !list ) {
        return;
    }

    for ( i = 0 ; list[i] ; i++ ) {
        Z_Free( list[i] );
    }

    Z_Free( list );
}

/*
==================
Sys_Sleep

Block execution for msec or until input is recieved.
==================
*/
void Sys_Sleep( int msec )
{
    fd_set fdset;

    if( msec == 0 )
        return;

    if( msec < 0 )
    {
        return;
    }
    else
    {
        usleep(msec);
    }
}

/*
==============
Sys_ErrorDialog

Display an error message
==============
*/
void Sys_ErrorDialog( const char *error )
{
    char buffer[ 1024 ];
    unsigned int size;
    fileHandle_t f;
    const char *fileName = "crashlog.txt";

    Sys_Print( va( "%s\n", error ) );

    // Write console log to file
    f = FS_FOpenFileWrite( fileName );
    if( !f )
    {
        Com_Printf( "ERROR: couldn't open %s\n", fileName );
        return;
    }

    while( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 )
        FS_Write( buffer, size, f );

    FS_FCloseFile( f );
}

void Sys_Frame(void)
{
  WPAD_ScanPads();
  
  // WPAD_ButtonsDown tells us which buttons were pressed in this loop
  // this is a "one shot" state which will not fire again until the button has been released
  u32 pressed = WPAD_ButtonsDown(0);
  
  // We return to the launcher application via exit
  if ( pressed & WPAD_BUTTON_HOME ) exit(0);
  
  // Wait for the next frame
  VIDEO_WaitVSync();
  KEYBOARD_switchLed(KEYBOARD_LEDSCROLL);
}

/*
==============
Sys_GLimpInit

Unix specific GL implementation initialisation
==============
*/
void Sys_GLimpInit( void )
{
    // NOP
}

void Sys_Hold( void ) {
  while(1) {
    // Call WPAD_ScanPads each loop, this reads the latest controller states
    WPAD_ScanPads();

    // WPAD_ButtonsDown tells us which buttons were pressed in this loop
    // this is a "one shot" state which will not fire again until the button has been released
    u32 pressed = WPAD_ButtonsDown(0);

    // Continue if A is pressed
    if ( pressed & WPAD_BUTTON_A ) return;

    // We return to the launcher application via exit
    if ( pressed & WPAD_BUTTON_HOME ) exit(0);

    // Wait for the next frame
    VIDEO_WaitVSync();
  }
}

/*
==============
Sys_PlatformInit

Wii specific initialisation
==============
*/
void Sys_PlatformInit( void )
{
  static void *xfb = NULL;
  static GXRModeObj *rmode = NULL;

  signal( SIGILL, Sys_SigHandler );
  signal( SIGFPE, Sys_SigHandler );
  signal( SIGSEGV, Sys_SigHandler );
  signal( SIGTERM, Sys_SigHandler );
  // Initialise the video system
  VIDEO_Init();
  
  // Obtain the preferred video mode from the system
  // This will correspond to the settings in the Wii menu
  rmode = VIDEO_GetPreferredMode(NULL);

  // Allocate memory for the display in the uncached region
  xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

  // Initialise the console, required for printf
  console_init(xfb,30,30,rmode->fbWidth-20,rmode->xfbHeight-20,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

  // Set up the video registers with the chosen mode
  VIDEO_Configure(rmode);

  // Tell the video hardware where our display memory is
  VIDEO_SetNextFramebuffer(xfb);

  // Make the display visible
  VIDEO_SetBlack(FALSE);

  // Flush the video register changes to the hardware
  VIDEO_Flush();

  // Wait for Video setup to complete
  VIDEO_WaitVSync();
  if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

  // The console understands VT terminal escape codes
  // This positions the cursor on row 2, column 0
  // we can use variables for this with format codes too
  // e.g. printf ("\x1b[%d;%dH", row, column );
  printf("\x1b[2;0H");
  printf("Video Initilized\n");

  // This function initialises the attached controllers
  WPAD_Init();

  // Init FAT
  if (!fatInitDefault()) {
    printf("fatInitDefault failure: terminating\n");
    goto error;
  }
  
  printf("Arena1Size: %d\n", SYS_GetArena1Size());
  printf("Arena2Size: %d\n", SYS_GetArena2Size());
  
  printf("Sys_PlatformInit: Done\n");
  error:
  Sys_Hold();
}
