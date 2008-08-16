/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "bg_public.h"

#ifdef GAME
# define  POOLSIZE ( 1024 * 1024 )
#else
# define  POOLSIZE ( 256 * 1024 )
#endif

#define HAVE_END_BLOCK_COOKIE 1

// Two sets of mem Cookies so they look right when viewed in memory (they had there bits reversed)
#define  FREEMEMCOOKIE  ((int)0xDEADBE3F)  // Any unlikely to be used value
//#define  FREEMEMCOOKIE  ((int)0x3FBEADDE)  // Any unlikely to be used value
#define  BLOCKSTARTCOOKIE  ((int)0xCAF3BAB3)  // Any unlikely to be used value
//#define  BLOCKSTARTCOOKIE  ((int)0xB3BAF3CA)  // Any unlikely to be used value
#if HAVE_END_BLOCK_COOKIE
#define  BLOCKENDCOOKIE  ((int)0x0BADF00D)  // Any unlikely to be used value
//#define  BLOCKENDCOOKIE  ((int)0x0DF0AD0B)  // Any unlikely to be used value
#endif
#define  ROUNDBITS    31UL          // Round to 32 bytes

typedef struct freeMemNode_s
{
  // Size of ROUNDBITS
  int cookie, size;        // Size includes node (obviously)
  struct freeMemNode_s *prev, *next;
} freeMemNode_t;

#if BG_MEMORY_DEBUG
typedef struct memTracker_s
{
  void *pointer;
  char *file;
  int  line;
  int  size;
  int  allocsize;
} memTracker_t;
static memTracker_t memtracker_array[POOLSIZE/2]= {{0}};
void BG_MemoryDebugDump( void );
void BG_MemoryDebugToFile( void );
#endif

static char           memoryPool[POOLSIZE];
static freeMemNode_t  *freeHead;
static int            freeMem;
#if BG_MEMORY_DEBUG
void  *_BG_Alloc( int size, char *file, int line )
#else
void *_BG_Alloc( int size )
#endif
{
  // Find a free block and allocate.
  // Does two passes, attempts to fill same-sized free slot first.

  freeMemNode_t *fmn, *prev, *next, *smallest;
  int allocsize, smallestsize;
  char *endptr;
  int *ptr;
#if HAVE_END_BLOCK_COOKIE
  allocsize = ( size + (sizeof(int) * 3) + ROUNDBITS ) & ~ROUNDBITS;    // Round to 32-byte boundary
#else
  allocsize = ( size + (sizeof(int) * 2) + ROUNDBITS ) & ~ROUNDBITS;    // Round to 32-byte boundary
#endif
  ptr = NULL;

  smallest = NULL;
  smallestsize = POOLSIZE + 1;    // Guaranteed not to miss any slots :)
  for( fmn = freeHead; fmn; fmn = fmn->next )
  {
    if( fmn->cookie != FREEMEMCOOKIE ){
#if BG_MEMORY_DEBUG
      Com_Printf("\n ^1BG_Alloc:  Memory corruption detected!\n");
      BG_MemoryDebugDump();
#endif
      Com_Error( ERR_DROP, "1BG_Alloc: Memory corruption detected!\n" );
    }

    if( fmn->size >= allocsize )
    {
      // We've got a block
      if( fmn->size == allocsize )
      {
        // Same size, just remove

        prev = fmn->prev;
        next = fmn->next;
        if( prev )
          prev->next = next;      // Point previous node to next
        if( next )
          next->prev = prev;      // Point next node to previous
        if( fmn == freeHead )
          freeHead = next;      // Set head pointer to next
        ptr = (int *) fmn;
        break;              // Stop the loop, this is fine
      }
      else
      {
        // Keep track of the smallest free slot
        if( fmn->size < smallestsize )
        {
          smallest = fmn;
          smallestsize = fmn->size;
        }
      }
    }
  }

  if( !ptr && smallest )
  {
    // We found a slot big enough
    smallest->size -= allocsize;
    endptr = (char *) smallest + smallest->size;
    ptr = (int *) endptr;
  }

  if( ptr )
  {
#if BG_MEMORY_DEBUG
    int i;
#endif
    freeMem -= allocsize;
    memset( ptr, 0, allocsize );
#if HAVE_END_BLOCK_COOKIE
    ptr[(allocsize / sizeof(int))-1] = BLOCKENDCOOKIE;
#endif
    *ptr++ = BLOCKSTARTCOOKIE;
    *ptr++ = allocsize;        // Store a copy of size for deallocation
#if BG_MEMORY_DEBUG
    for (i=0; i < POOLSIZE/2; i++)
    {
      if( memtracker_array[i].pointer != NULL) continue;
      memtracker_array[i].pointer = (void *) ptr;
      memtracker_array[i].file    = file;
      memtracker_array[i].line    = line;
      memtracker_array[i].size    = size;
      memtracker_array[i].allocsize = allocsize;
//      BG_MemoryDebugToFile();
      break;
    }
//    Com_Printf("BG_Alloc returning pointer %p to %s: %d\n",(void *) ptr, file, line);
#endif
    return( (void *) ptr );
  }
#if BG_MEMORY_DEBUG
    Com_Printf("\n ^1BG_Alloc: failed on allocation of %i bytes\n", size);
    BG_MemoryDebugDump();
#endif
  Com_Error( ERR_DROP, "BG_Alloc: failed on allocation of %i bytes\n", size );
  return( NULL );
}
#if BG_MEMORY_DEBUG
void  _BG_Free( void *ptr, char *file, int line )
#else
void _BG_Free( void *ptr )
#endif
{
  // Release allocated memory, add it to the free list.
#if BG_MEMORY_DEBUG
  int i;
//  Com_Printf("BG_Free called on pointer  %p from %s: %d\n", ptr, file, line);
  for (i=0; i < POOLSIZE/2; i++)
  {
    if( memtracker_array[i].pointer == ptr)
    {
      memtracker_array[i].pointer = NULL;
//      BG_MemoryDebugToFile();
      break;
    }
  }
#endif
  freeMemNode_t *fmn;
  char *freeend;
  int *freeptr;
  int allocsize;

  freeptr = ptr;
  freeptr--;
  allocsize = *freeptr;
//  Com_Printf("BG_Free: freeing %d bytes\n", allocsize);
  freeMem += *freeptr;
  freeptr--;
  if( *freeptr != BLOCKSTARTCOOKIE ){
#if BG_MEMORY_DEBUG
    Com_Printf("\n ^1BG_Free: Start Block Memory corruption detected while freeing %p\n", ptr);
    BG_MemoryDebugDump();
#endif
    Com_Error( ERR_DROP, "BG_Free: Memory corruption detected!\n" );
  }
#if HAVE_END_BLOCK_COOKIE
  if( freeptr[(allocsize / sizeof(int))-1] != BLOCKENDCOOKIE ){
#if BG_MEMORY_DEBUG
    Com_Printf("\n ^1BG_Free: End Block Memory corruption detected while freeing %p\n", ptr);
    BG_MemoryDebugDump();
#endif
    Com_Error( ERR_DROP, "BG_Free: Memory corruption detected!\n" );
  }
#endif
  for( fmn = freeHead; fmn; fmn = fmn->next )
  {
    freeend = ((char *) fmn) + fmn->size;
    if( freeend == (char *) freeptr )
    {
      // Released block can be merged to an existing node
      memset( freeptr, 0xFE, allocsize );
      fmn->size += allocsize;    // Add size of node.
      return;
    }
  }
  memset( freeptr, 0xFE, allocsize ); // Mark memory that has been freed before
  // No merging, add to head of list
  fmn = (freeMemNode_t *) freeptr;
  fmn->size = allocsize;        // Set this first to avoid corrupting *freeptr
  fmn->cookie = FREEMEMCOOKIE;
  fmn->prev = NULL;
  fmn->next = freeHead;
  freeHead->prev = fmn;
  freeHead = fmn;
}

void BG_InitMemory( void )
{
  // Set up the initial node

  freeHead = (freeMemNode_t *)memoryPool;
  freeHead->cookie = FREEMEMCOOKIE;
  freeHead->size = POOLSIZE;
  freeHead->next = NULL;
  freeHead->prev = NULL;
  freeMem = sizeof( memoryPool );
#if BG_MEMORY_DEBUG
  Com_Printf("BG_InitMemory: freeMem = %d\n", freeMem);
  Com_Printf("BG_InitMemory: FREEMEMCOOKIE = %d\n", FREEMEMCOOKIE);
  Com_Printf("BG_InitMemory: BLOCKSTARTCOOKIE = %d\n", BLOCKSTARTCOOKIE);
#if HAVE_END_BLOCK_COOKIE
  Com_Printf("BG_InitMemory: BLOCKENDCOOKIE = %d\n", BLOCKENDCOOKIE);
#endif /*#if HAVE_END_BLOCK_COOKIE*/
#endif
}

void BG_DefragmentMemory( void )
{
  // If there's a frenzy of deallocation and we want to
  // allocate something big, this is useful. Otherwise...
  // not much use.

  freeMemNode_t *startfmn, *endfmn, *fmn;

  for( startfmn = freeHead; startfmn; )
  {
    endfmn = (freeMemNode_t *)(((char *) startfmn) + startfmn->size);
    for( fmn = freeHead; fmn; )
    {
      if( fmn->cookie != FREEMEMCOOKIE ){
#if BG_MEMORY_DEBUG
        Com_Printf("\n ^BG_DefragmentMemory:  Memory corruption detected!\n");
        BG_MemoryDebugDump();
#endif
        Com_Error( ERR_DROP, "BG_DefragmentMemory: Memory corruption detected!\n" );
      }

      if( fmn == endfmn )
      {
        // We can add fmn onto startfmn.

        if( fmn->prev )
          fmn->prev->next = fmn->next;
        if( fmn->next )
        {
          if( !(fmn->next->prev = fmn->prev) )
            freeHead = fmn->next;  // We're removing the head node
        }
        startfmn->size += fmn->size;
        memset( fmn, 0, sizeof(freeMemNode_t) );  // A redundant call, really.

        startfmn = freeHead;
        endfmn = fmn = NULL;        // Break out of current loop
      }
      else
        fmn = fmn->next;
    }

    if( endfmn )
      startfmn = startfmn->next;    // endfmn acts as a 'restart' flag here
  }
}
#if BG_MEMORY_DEBUG
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void trap_FS_Read( void *buffer, int len, fileHandle_t f );
void trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void trap_FS_FCloseFile( fileHandle_t f );

void BG_MemoryDebugDump( void )
{
  void *pointer;
  char *file;
  int  line, i, size, allocsize;
  Com_Printf("BG_MemoryDebugDump: freeMem  = %d\n", freeMem);
  Com_Printf("BG_MemoryDebugDump: freeHead = %p\n", freeHead);
  
  Com_Printf("Memory Dump:\n");
  for (i=0; i < POOLSIZE/2; i++)
  {
    if( memtracker_array[i].pointer == NULL) continue;
    pointer   = memtracker_array[i].pointer;
    file      = memtracker_array[i].file;
    line      = memtracker_array[i].line;
    size      = memtracker_array[i].size;
    allocsize = memtracker_array[i].allocsize;
    Com_Printf("%03d: %p: size = %d (%d): %s: %d\n",i , pointer, size, allocsize, file, line);
  }
}
void BG_MemoryDebugToFile( void )
{
  void *pointer;
  char *file;
  int  line, i, size, allocsize;
  fileHandle_t f;
  char data[ 10000 ];
  trap_FS_FOpenFile("bg_memorydump.txt", &f, FS_WRITE);
  Q_strcat( data, 10000, va("BG_MemoryDebugDumpToFile: freeMem  = %d\n", freeMem));
  Q_strcat( data, 10000, va("BG_MemoryDebugDumpToFile: freeHead = %p\n", freeHead));
  Q_strcat( data, 10000, "Memory Dump:\n");
  for (i=0; i < POOLSIZE/2; i++)
  {
    if( memtracker_array[i].pointer == NULL) continue;
    pointer   = memtracker_array[i].pointer;
    file      = memtracker_array[i].file;
    line      = memtracker_array[i].line;
    size      = memtracker_array[i].size;
    allocsize = memtracker_array[i].allocsize;
    Q_strcat( data, 10000, va("%03d: %p: size = %d (%d): %s: %d\n",i , pointer, size, allocsize, file, line));
  }
  trap_FS_Write( data, 10000, f);
  trap_FS_FCloseFile( f);
}
#endif
