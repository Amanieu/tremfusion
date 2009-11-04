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

#include "ui_shared.h"

#define SCROLL_TIME_START         500
#define SCROLL_TIME_ADJUST        150
#define SCROLL_TIME_ADJUSTOFFSET  40
#define SCROLL_TIME_FLOOR         20

typedef struct scrollInfo_s
{
  int nextScrollTime;
  int nextAdjustTime;
  int adjustValue;
  int scrollKey;
  float xStart;
  float yStart;
  itemDef_t *item;
  qboolean scrollDir;
}
scrollInfo_t;

static scrollInfo_t scrollInfo;

#define MAX_DELAYED_COMMANDS 64

typedef struct delayed_cmd_s
{
  itemDef_t *item;
  int time;
}
delayed_cmd_t; 

delayed_cmd_t delayed_cmd[ MAX_DELAYED_COMMANDS ]; 

// prevent compiler warnings
void voidFunction( void *var )
{
  return;
}

qboolean voidFunction2( itemDef_t *var1, int var2 )
{
  return qfalse;
}

static CaptureFunc *captureFunc = voidFunction;
static int captureFuncExpiry = 0;
static void *captureData = NULL;
static itemDef_t *itemCapture = NULL;   // item that has the mouse captured ( if any )

displayContextDef_t *DC = NULL;

static qboolean g_waitingForKey = qfalse;
static qboolean g_editingField = qfalse;

static itemDef_t *g_bindItem = NULL;
static itemDef_t *g_editItem = NULL;

menuDef_t Menus[MAX_MENUS];      // defined menus
int menuCount = 0;               // how many

menuDef_t *menuStack[MAX_OPEN_MENUS];
int openMenuCount = 0;

#define DOUBLE_CLICK_DELAY 300
static int lastListBoxClickTime = 0;

void Item_RunScript( itemDef_t *item, const char *s );
void Item_SetupKeywordHash( void );
static ID_INLINE qboolean Item_IsEditField( itemDef_t *item );
void Menu_SetupKeywordHash( void );
int BindingIDFromName( const char *name );
qboolean Item_Bind_HandleKey( itemDef_t *item, int key, qboolean down );
itemDef_t *Menu_SetPrevCursorItem( menuDef_t *menu );
itemDef_t *Menu_SetNextCursorItem( menuDef_t *menu );
static qboolean Menu_OverActiveItem( menuDef_t *menu, float x, float y );

void trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
/*
===============
UI_InstallCaptureFunc
===============
*/
void UI_InstallCaptureFunc( CaptureFunc *f, void *data, int timeout )
{
  captureFunc = f;
  captureData = data;

  if( timeout > 0 )
    captureFuncExpiry = DC->realTime + timeout;
  else
    captureFuncExpiry = 0;
}

/*
===============
UI_RemoveCaptureFunc
===============
*/
void UI_RemoveCaptureFunc( void )
{
  captureFunc = voidFunction;
  captureData = NULL;
  captureFuncExpiry = 0;
}

#ifdef CGAME
#define MEM_POOL_SIZE  128 * 1024
#else
#define MEM_POOL_SIZE  1024 * 1024
#endif

static char   UI_memoryPool[MEM_POOL_SIZE];
static int    allocPoint, outOfMemory;
// Hacked new memory pool for hud
static char   UI_hudmemoryPool[MEM_POOL_SIZE];
static int    hudallocPoint, hudoutOfMemory;

/*
===============
UI_ResetHUDMemory
===============
*/
void UI_ResetHUDMemory( void )
{
  hudallocPoint = 0;
  hudoutOfMemory = qfalse;
}

/*
===============
UI_Alloc
===============
*/
void *UI_Alloc( int size )
{
  char  *p;

  if( allocPoint + size > MEM_POOL_SIZE )
  {
    outOfMemory = qtrue;

    if( DC->Print )
      DC->Print( "UI_Alloc: Failure. Out of memory!\n" );
    //DC->trap_Print(S_COLOR_YELLOW"WARNING: UI Out of Memory!\n");
    return NULL;
  }

  p = &UI_memoryPool[ allocPoint ];

  allocPoint += ( size + 15 ) & ~15;

  return p;
}

/*
===============
UI_HUDAlloc
===============
*/
void *UI_HUDAlloc( int size )
{
  char  *p;

  if( hudallocPoint + size > MEM_POOL_SIZE )
  {
  DC->Print( "UI_HUDAlloc: Out of memory! Reinititing hud Memory!!\n" );
    hudoutOfMemory = qtrue;
    UI_ResetHUDMemory();
  }

  p = &UI_hudmemoryPool[ hudallocPoint ];

  hudallocPoint += ( size + 15 ) & ~15;

  return p;
}

/*
===============
UI_InitMemory
===============
*/
void UI_InitMemory( void )
{
  allocPoint = 0;
  outOfMemory = qfalse;
  hudallocPoint = 0;
  hudoutOfMemory = qfalse;
}

qboolean UI_OutOfMemory( )
{
  return outOfMemory;
}





#define HASH_TABLE_SIZE 2048
/*
================
return a hash value for the string
================
*/
static long hashForString( const char *str )
{
  int   i;
  long  hash;
  char  letter;

  hash = 0;
  i = 0;

  while( str[i] != '\0' )
  {
    letter = tolower( str[i] );
    hash += ( long )( letter ) * ( i + 119 );
    i++;
  }

  hash &= ( HASH_TABLE_SIZE - 1 );
  return hash;
}

typedef struct stringDef_s
{
  struct stringDef_s *next;
  const char *str;
}

stringDef_t;

static int strPoolIndex = 0;
static char strPool[STRING_POOL_SIZE];

static int strHandleCount = 0;
static stringDef_t *strHandle[HASH_TABLE_SIZE];


const char *String_Alloc( const char *p )
{
  int len;
  long hash;
  stringDef_t *str, *last;
  static const char *staticNULL = "";

  if( p == NULL )
    return NULL;

  if( *p == 0 )
    return staticNULL;

  hash = hashForString( p );

  str = strHandle[hash];

  while( str )
  {
    if( strcmp( p, str->str ) == 0 )
      return str->str;

    str = str->next;
  }

  len = strlen( p );

  if( len + strPoolIndex + 1 < STRING_POOL_SIZE )
  {
    int ph = strPoolIndex;
    strcpy( &strPool[strPoolIndex], p );
    strPoolIndex += len + 1;

    str = strHandle[hash];
    last = str;

    while( str && str->next )
    {
      last = str;
      str = str->next;
    }

    str  = UI_Alloc( sizeof( stringDef_t ) );
    str->next = NULL;
    str->str = &strPool[ph];

    if( last )
      last->next = str;
    else
      strHandle[hash] = str;

    return &strPool[ph];
  }

  return NULL;
}

void String_Report( void )
{
  float f;
  Com_Printf( "Memory/String Pool Info\n" );
  Com_Printf( "----------------\n" );
  f = strPoolIndex;
  f /= STRING_POOL_SIZE;
  f *= 100;
  Com_Printf( "String Pool is %.1f%% full, %i bytes out of %i used.\n", f, strPoolIndex, STRING_POOL_SIZE );
  f = allocPoint;
  f /= MEM_POOL_SIZE;
  f *= 100;
  Com_Printf( "Memory Pool is %.1f%% full, %i bytes out of %i used.\n", f, allocPoint, MEM_POOL_SIZE );
}

/*
=================
String_Init
=================
*/
void String_Init( void )
{
  int i;

  for( i = 0; i < HASH_TABLE_SIZE; i++ )
    strHandle[ i ] = 0;

  strHandleCount = 0;

  strPoolIndex = 0;

  menuCount = 0;

  openMenuCount = 0;

  UI_InitMemory( );

  Item_SetupKeywordHash( );

  Menu_SetupKeywordHash( );

  if( DC && DC->getBindingBuf )
    Controls_GetConfig( );
}

/*
=================
PC_SourceWarning
=================
*/
void PC_SourceWarning( int handle, char *format, ... )
{
  int line;
  char filename[128];
  va_list argptr;
  static char string[4096];

  va_start( argptr, format );
  Q_vsnprintf( string, sizeof( string ), format, argptr );
  va_end( argptr );

  filename[0] = '\0';
  line = 0;
  trap_Parse_SourceFileAndLine( handle, filename, &line );

  Com_Printf( S_COLOR_YELLOW "WARNING: %s, line %d: %s\n", filename, line, string );
}

/*
=================
PC_SourceError
=================
*/
void PC_SourceError( int handle, char *format, ... )
{
  int line;
  char filename[128];
  va_list argptr;
  static char string[4096];

  va_start( argptr, format );
  Q_vsnprintf( string, sizeof( string ), format, argptr );
  va_end( argptr );

  filename[0] = '\0';
  line = 0;
  trap_Parse_SourceFileAndLine( handle, filename, &line );

  Com_Printf( S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string );
}

/*
=================
LerpColor
=================
*/
void LerpColor( vec4_t a, vec4_t b, vec4_t c, float t )
{
  int i;

  // lerp and clamp each component

  for( i = 0; i < 4; i++ )
  {
    c[i] = a[i] + t * ( b[i] - a[i] );

    if( c[i] < 0 )
      c[i] = 0;
    else if( c[i] > 1.0 )
      c[i] = 1.0;
  }
}

/*
=================
Float_Parse
=================
*/
qboolean Float_Parse( char **p, float *f )
{
  char  *token;
  token = COM_ParseExt( p, qfalse );

  if( token && token[0] != 0 )
  {
    *f = atof( token );
    return qtrue;
  }
  else
    return qfalse;
}

#define MAX_EXPR_ELEMENTS 32

typedef enum
{
  EXPR_OPERATOR,
  EXPR_VALUE
}
exprType_t;

typedef struct exprToken_s
{
  exprType_t  type;
  union
  {
    char  op;
    float val;
  } u;
}
exprToken_t;

typedef struct exprList_s
{
  exprToken_t l[ MAX_EXPR_ELEMENTS ];
  int f, b;
}
exprList_t;

/*
=================
OpPrec
 
Return a value reflecting operator precedence
=================
*/
static ID_INLINE int OpPrec( char op )
{
  switch( op )
  {
    case '*':
      return 4;

    case '/':
      return 3;

    case '+':
      return 2;

    case '-':
      return 1;

    case '(':
      return 0;

    default:
      return -1;
  }
}

/*
=================
PC_Expression_Parse
=================
*/
static qboolean PC_Expression_Parse( int handle, float *f )
{
  pc_token_t  token;
  int         unmatchedParentheses = 0;
  exprList_t  stack, fifo;
  exprToken_t value;
  qboolean    expectingNumber = qtrue;

#define FULL( a ) ( a.b >= ( MAX_EXPR_ELEMENTS - 1 ) )
#define EMPTY( a ) ( a.f > a.b )

#define PUSH_VAL( a, v ) \
  { \
    if( FULL( a ) ) \
      return qfalse; \
    a.b++; \
    a.l[ a.b ].type = EXPR_VALUE; \
    a.l[ a.b ].u.val = v; \
  }

#define PUSH_OP( a, o ) \
  { \
    if( FULL( a ) ) \
      return qfalse; \
    a.b++; \
    a.l[ a.b ].type = EXPR_OPERATOR; \
    a.l[ a.b ].u.op = o; \
  }

#define POP_STACK( a ) \
  { \
    if( EMPTY( a ) ) \
      return qfalse; \
    value = a.l[ a.b ]; \
    a.b--; \
  }

#define PEEK_STACK_OP( a )  ( a.l[ a.b ].u.op )
#define PEEK_STACK_VAL( a ) ( a.l[ a.b ].u.val )

#define POP_FIFO( a ) \
  { \
    if( EMPTY( a ) ) \
      return qfalse; \
    value = a.l[ a.f ]; \
    a.f++; \
  }

  stack.f = fifo.f = 0;
  stack.b = fifo.b = -1;

  while( trap_Parse_ReadToken( handle, &token ) )
  {
    if( !unmatchedParentheses && token.string[ 0 ] == ')' )
      break;

    // Special case to catch negative numbers
    if( expectingNumber && token.string[ 0 ] == '-' )
    {
      if( !trap_Parse_ReadToken( handle, &token ) )
        return qfalse;

      token.floatvalue = -token.floatvalue;
    }

    if( token.type == TT_NUMBER )
    {
      if( !expectingNumber )
        return qfalse;

      expectingNumber = !expectingNumber;

      PUSH_VAL( fifo, token.floatvalue );
    }
    else
    {
      switch( token.string[ 0 ] )
      {
        case '(':
          unmatchedParentheses++;
          PUSH_OP( stack, '(' );
          break;

        case ')':
          unmatchedParentheses--;

          if( unmatchedParentheses < 0 )
            return qfalse;

          while( !EMPTY( stack ) && PEEK_STACK_OP( stack ) != '(' )
          {
            POP_STACK( stack );
            PUSH_OP( fifo, value.u.op );
          }

          // Pop the '('
          POP_STACK( stack );

          break;

        case '*':
        case '/':
        case '+':
        case '-':
          if( expectingNumber )
            return qfalse;

          expectingNumber = !expectingNumber;

          if( EMPTY( stack ) )
          {
            PUSH_OP( stack, token.string[ 0 ] );
          }
          else
          {
            while( !EMPTY( stack ) && OpPrec( token.string[ 0 ] ) < OpPrec( PEEK_STACK_OP( stack ) ) )
            {
              POP_STACK( stack );
              PUSH_OP( fifo, value.u.op );
            }

            PUSH_OP( stack, token.string[ 0 ] );
          }

          break;

        default:
          // Unknown token
          return qfalse;
      }
    }
  }

  while( !EMPTY( stack ) )
  {
    POP_STACK( stack );
    PUSH_OP( fifo, value.u.op );
  }

  while( !EMPTY( fifo ) )
  {
    POP_FIFO( fifo );

    if( value.type == EXPR_VALUE )
    {
      PUSH_VAL( stack, value.u.val );
    }
    else if( value.type == EXPR_OPERATOR )
    {
      char op = value.u.op;
      float operand1, operand2, result;

      POP_STACK( stack );
      operand2 = value.u.val;
      POP_STACK( stack );
      operand1 = value.u.val;

      switch( op )
      {
        case '*':
          result = operand1 * operand2;
          break;

        case '/':
          result = operand1 / operand2;
          break;

        case '+':
          result = operand1 + operand2;
          break;

        case '-':
          result = operand1 - operand2;
          break;

        default:
          Com_Error( ERR_FATAL, "Unknown operator '%c' in postfix string", op );
          return qfalse;
      }

      PUSH_VAL( stack, result );
    }
  }

  POP_STACK( stack );

  *f = value.u.val;

  return qtrue;

#undef FULL
#undef EMPTY
#undef PUSH_VAL
#undef PUSH_OP
#undef POP_STACK
#undef PEEK_STACK_OP
#undef PEEK_STACK_VAL
#undef POP_FIFO
}

/*
=================
PC_Float_Parse
=================
*/
qboolean PC_Float_Parse( int handle, float *f )
{
  pc_token_t token;
  int negative = qfalse;

  if( !trap_Parse_ReadToken( handle, &token ) )
    return qfalse;

  if( token.string[ 0 ] == '(' )
    return PC_Expression_Parse( handle, f );

  if( token.string[0] == '-' )
  {
    if( !trap_Parse_ReadToken( handle, &token ) )
      return qfalse;

    negative = qtrue;
  }

  if( token.type != TT_NUMBER )
  {
    PC_SourceError( handle, "expected float but found %s\n", token.string );
    return qfalse;
  }

  if( negative )
    * f = -token.floatvalue;
  else
    *f = token.floatvalue;

  return qtrue;
}

/*
=================
Color_Parse
=================
*/
qboolean Color_Parse( char **p, vec4_t *c )
{
  int i;
  float f;

  for( i = 0; i < 4; i++ )
  {
    if( !Float_Parse( p, &f ) )
      return qfalse;

    ( *c )[i] = f;
  }

  return qtrue;
}

/*
=================
PC_Color_Parse
=================
*/
qboolean PC_Color_Parse( int handle, vec4_t *c )
{
  int i;
  float f;

  for( i = 0; i < 4; i++ )
  {
    if( !PC_Float_Parse( handle, &f ) )
      return qfalse;

    ( *c )[i] = f;
  }

  return qtrue;
}

/*
=================
Int_Parse
=================
*/
qboolean Int_Parse( char **p, int *i )
{
  char  *token;
  token = COM_ParseExt( p, qfalse );

  if( token && token[0] != 0 )
  {
    *i = atoi( token );
    return qtrue;
  }
  else
    return qfalse;
}

/*
=================
PC_Int_Parse
=================
*/
qboolean PC_Int_Parse( int handle, int *i )
{
  pc_token_t token;
  int negative = qfalse;

  if( !trap_Parse_ReadToken( handle, &token ) )
    return qfalse;

  if( token.string[ 0 ] == '(' )
  {
    float f;

    if( PC_Expression_Parse( handle, &f ) )
    {
      *i = ( int )f;
      return qtrue;
    }
    else
      return qfalse;
  }

  if( token.string[0] == '-' )
  {
    if( !trap_Parse_ReadToken( handle, &token ) )
      return qfalse;

    negative = qtrue;
  }

  if( token.type != TT_NUMBER )
  {
    PC_SourceError( handle, "expected integer but found %s\n", token.string );
    return qfalse;
  }

  *i = token.intvalue;

  if( negative )
    * i = - *i;

  return qtrue;
}

/*
=================
Rect_Parse
=================
*/
qboolean Rect_Parse( char **p, rectDef_t *r )
{
  if( Float_Parse( p, &r->x ) )
  {
    if( Float_Parse( p, &r->y ) )
    {
      if( Float_Parse( p, &r->w ) )
      {
        if( Float_Parse( p, &r->h ) )
          return qtrue;
      }
    }
  }

  return qfalse;
}

/*
=================
PC_Rect_Parse
=================
*/
qboolean PC_Rect_Parse( int handle, rectDef_t *r )
{
  if( PC_Float_Parse( handle, &r->x ) )
  {
    if( PC_Float_Parse( handle, &r->y ) )
    {
      if( PC_Float_Parse( handle, &r->w ) )
      {
        if( PC_Float_Parse( handle, &r->h ) )
          return qtrue;
      }
    }
  }

  return qfalse;
}

/*
=================
String_Parse
=================
*/
qboolean String_Parse( char **p, const char **out )
{
  char *token;

  token = COM_ParseExt( p, qfalse );

  if( token && token[0] != 0 )
  {
    *( out ) = String_Alloc( token );
    return qtrue;
  }

  return qfalse;
}

/*
=================
PC_String_Parse
=================
*/
qboolean PC_String_Parse( int handle, const char **out )
{
  pc_token_t token;

  if( !trap_Parse_ReadToken( handle, &token ) )
    return qfalse;

  *( out ) = String_Alloc( token.string );

  return qtrue;
}

/*
=================
PC_Script_Parse
=================
*/
qboolean PC_Script_Parse( int handle, const char **out )
{
  char script[1024];
  pc_token_t token;

  memset( script, 0, sizeof( script ) );
  // scripts start with { and have ; separated command lists.. commands are command, arg..
  // basically we want everything between the { } as it will be interpreted at run time

  if( !trap_Parse_ReadToken( handle, &token ) )
    return qfalse;

  if( Q_stricmp( token.string, "{" ) != 0 )
    return qfalse;

  while( 1 )
  {
    if( !trap_Parse_ReadToken( handle, &token ) )
      return qfalse;

    if( Q_stricmp( token.string, "}" ) == 0 )
    {
      *out = String_Alloc( script );
      return qtrue;
    }

    if( token.string[1] != '\0' )
      Q_strcat( script, 1024, va( "\"%s\"", token.string ) );
    else
      Q_strcat( script, 1024, token.string );

    Q_strcat( script, 1024, " " );
  }

  return qfalse;
}

// display, window, menu, item code
//

/*
==================
Init_Display
 
Initializes the display with a structure to all the drawing routines
==================
*/
void Init_Display( displayContextDef_t *dc )
{
  DC = dc;
}



// type and style painting

void GradientBar_Paint( rectDef_t *rect, vec4_t color )
{
  // gradient bar takes two paints
  DC->setColor( color );
  DC->drawHandlePic( rect->x, rect->y, rect->w, rect->h, DC->Assets.gradientBar );
  DC->setColor( NULL );
}


/*
==================
Window_Init
 
Initializes a window structure ( windowDef_t ) with defaults
 
==================
*/
void Window_Init( Window *w )
{
  memset( w, 0, sizeof( windowDef_t ) );
  w->borderSize = 1;
  w->foreColor[0] = w->foreColor[1] = w->foreColor[2] = w->foreColor[3] = 1.0;
  w->cinematic = -1;
}

void Fade( int *flags, float *f, float clamp, int *nextTime, int offsetTime, qboolean bFlags, float fadeAmount )
{
  if( *flags & ( WINDOW_FADINGOUT | WINDOW_FADINGIN ) )
  {
    if( DC->realTime > *nextTime )
    {
      *nextTime = DC->realTime + offsetTime;

      if( *flags & WINDOW_FADINGOUT )
      {
        *f -= fadeAmount;

        if( bFlags && *f <= 0.0 )
          *flags &= ~( WINDOW_FADINGOUT | WINDOW_VISIBLE );
      }
      else
      {
        *f += fadeAmount;

        if( *f >= clamp )
        {
          *f = clamp;

          if( bFlags )
            *flags &= ~WINDOW_FADINGIN;
        }
      }
    }
  }
}



static void Window_Paint( Window *w, float fadeAmount, float fadeClamp, float fadeCycle )
{
  vec4_t color;
  rectDef_t fillRect = w->rect;


  if( DC->getCVarValue( "ui_developer" ) )
  {
    color[0] = color[1] = color[2] = color[3] = 1;
    DC->drawRect( w->rect.x, w->rect.y, w->rect.w, w->rect.h, 1, color );
  }

  if( w == NULL || ( w->style == 0 && w->border == 0 ) )
    return;

  if( w->border != 0 )
  {
    fillRect.x += w->borderSize;
    fillRect.y += w->borderSize;
    fillRect.w -= w->borderSize + 1;
    fillRect.h -= w->borderSize + 1;
  }

  if( w->style == WINDOW_STYLE_FILLED )
  {
    // box, but possible a shader that needs filled

    if( w->background )
    {
      Fade( &w->flags, &w->backColor[3], fadeClamp, &w->nextTime, fadeCycle, qtrue, fadeAmount );
      DC->setColor( w->backColor );
      DC->drawHandlePic( fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->background );
      DC->setColor( NULL );
    }
    else if( w->border == WINDOW_BORDER_ROUNDED )
      DC->fillRoundedRect( fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->borderSize, w->backColor );
    else
      DC->fillRect( fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->backColor );
  }
  else if( w->style == WINDOW_STYLE_GRADIENT )
  {
    GradientBar_Paint( &fillRect, w->backColor );
    // gradient bar
  }
  else if( w->style == WINDOW_STYLE_SHADER )
  {
    if( w->flags & WINDOW_FORECOLORSET )
      DC->setColor( w->foreColor );

    DC->drawHandlePic( fillRect.x, fillRect.y, fillRect.w, fillRect.h, w->background );
    DC->setColor( NULL );
  }
  else if( w->style == WINDOW_STYLE_CINEMATIC )
  {
    if( w->cinematic == -1 )
    {
      w->cinematic = DC->playCinematic( w->cinematicName, fillRect.x, fillRect.y, fillRect.w, fillRect.h );

      if( w->cinematic == -1 )
        w->cinematic = -2;
    }

    if( w->cinematic >= 0 )
    {
      DC->runCinematicFrame( w->cinematic );
      DC->drawCinematic( w->cinematic, fillRect.x, fillRect.y, fillRect.w, fillRect.h );
    }
  }
}

static void Border_Paint( Window *w )
{
  if( w == NULL || ( w->style == 0 && w->border == 0 ) )
    return;

  if( w->border == WINDOW_BORDER_FULL )
  {
    // full
    DC->drawRect( w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize, w->borderColor );
  }
  else if( w->border == WINDOW_BORDER_HORZ )
  {
    // top/bottom
    DC->setColor( w->borderColor );
    DC->drawTopBottom( w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize );
    DC->setColor( NULL );
  }
  else if( w->border == WINDOW_BORDER_VERT )
  {
    // left right
    DC->setColor( w->borderColor );
    DC->drawSides( w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize );
    DC->setColor( NULL );
  }
  else if( w->border == WINDOW_BORDER_KCGRADIENT )
  {
    // this is just two gradient bars along each horz edge
    rectDef_t r = w->rect;
    r.h = w->borderSize;
    GradientBar_Paint( &r, w->borderColor );
    r.y = w->rect.y + w->rect.h - 1;
    GradientBar_Paint( &r, w->borderColor );
  }
  else if( w->border == WINDOW_BORDER_ROUNDED )
  {
    // full rounded
    DC->drawRoundedRect( w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->borderSize, w->borderColor );
  }
}


void Item_SetScreenCoords( itemDef_t *item, float x, float y )
{
  if( item == NULL )
    return;

  if( item->window.border != 0 )
  {
    x += item->window.borderSize;
    y += item->window.borderSize;
  }

  item->window.rect.x = x + item->window.rectClient.x;
  item->window.rect.y = y + item->window.rectClient.y;
  item->window.rect.w = item->window.rectClient.w;
  item->window.rect.h = item->window.rectClient.h;

  // force the text rects to recompute
  //item->textRect.w = 0;
  //item->textRect.h = 0;
}

// FIXME: consolidate this with nearby stuff
void Item_UpdatePosition( itemDef_t *item )
{
  float x, y;
  menuDef_t *menu;

  if( item == NULL || item->parent == NULL )
    return;

  menu = item->parent;

  x = menu->window.rect.x;
  y = menu->window.rect.y;

  if( menu->window.border != 0 )
  {
    x += menu->window.borderSize;
    y += menu->window.borderSize;
  }

  Item_SetScreenCoords( item, x, y );

}

// menus
void Menu_UpdatePosition( menuDef_t *menu )
{
  int i;
  float x, y;

  if( menu == NULL )
    return;

  x = menu->window.rect.x;
  y = menu->window.rect.y;

  if( menu->window.border != 0 )
  {
    x += menu->window.borderSize;
    y += menu->window.borderSize;
  }

  for( i = 0; i < menu->itemCount; i++ )
    Item_SetScreenCoords( menu->items[i], x, y );
}

static void Menu_AspectiseRect( int bias, Rectangle *rect )
{
  switch( bias )
  {
    case ALIGN_LEFT:
      rect->x *= DC->aspectScale;
      rect->w *= DC->aspectScale;
      break;

    case ALIGN_CENTER:
      rect->x = ( rect->x * DC->aspectScale ) +
                ( 320.0f - ( 320.0f * DC->aspectScale ) );
      rect->w *= DC->aspectScale;
      break;

    case ALIGN_RIGHT:
      rect->x = 640.0f - ( ( 640.0f - rect->x ) * DC->aspectScale );
      rect->w *= DC->aspectScale;
      break;

    default:

    case ASPECT_NONE:
      break;
  }
}

void Menu_AspectCompensate( menuDef_t *menu )
{
  int i;

  if( menu->window.aspectBias != ASPECT_NONE )
  {
    Menu_AspectiseRect( menu->window.aspectBias, &menu->window.rect );

    for( i = 0; i < menu->itemCount; i++ )
    {
      menu->items[ i ]->window.rectClient.x *= DC->aspectScale;
      menu->items[ i ]->window.rectClient.w *= DC->aspectScale;
      menu->items[ i ]->textalignx *= DC->aspectScale;
    }
  }
  else
  {
    for( i = 0; i < menu->itemCount; i++ )
    {
      Menu_AspectiseRect( menu->items[ i ]->window.aspectBias,
                          &menu->items[ i ]->window.rectClient );

      if( menu->items[ i ]->window.aspectBias != ASPECT_NONE )
        menu->items[ i ]->textalignx *= DC->aspectScale;
    }
  }
}

void Menu_PostParse( menuDef_t *menu )
{
  if( menu == NULL )
    return;

  if( menu->fullScreen )
  {
    menu->window.rect.x = 0;
    menu->window.rect.y = 0;
    menu->window.rect.w = 640;
    menu->window.rect.h = 480;
  }

  Menu_AspectCompensate( menu );
  Menu_UpdatePosition( menu );
}

itemDef_t *Menu_ClearFocus( menuDef_t *menu )
{
  int i;
  itemDef_t *ret = NULL;

  if( menu == NULL )
    return NULL;

  for( i = 0; i < menu->itemCount; i++ )
  {
    if( menu->items[i]->window.flags & WINDOW_HASFOCUS )
      ret = menu->items[i];

    menu->items[i]->window.flags &= ~WINDOW_HASFOCUS;

    if( menu->items[i]->leaveFocus )
      Item_RunScript( menu->items[i], menu->items[i]->leaveFocus );
  }

  return ret;
}

qboolean IsVisible( int flags )
{
  return ( flags & WINDOW_VISIBLE && !( flags & WINDOW_FADINGOUT ) );
}

qboolean Rect_ContainsPoint( rectDef_t *rect, float x, float y )
{
  if( rect )
  {
    if( x > rect->x && x < rect->x + rect->w && y > rect->y && y < rect->y + rect->h )
      return qtrue;
  }

  return qfalse;
}

int Menu_ItemsMatchingGroup( menuDef_t *menu, const char *name )
{
  int i;
  int count = 0;

  for( i = 0; i < menu->itemCount; i++ )
  {
    if( Q_stricmp( menu->items[i]->window.name, name ) == 0 ||
        ( menu->items[i]->window.group && Q_stricmp( menu->items[i]->window.group, name ) == 0 ) )
    {
      count++;
    }
  }

  return count;
}

itemDef_t *Menu_GetMatchingItemByNumber( menuDef_t *menu, int index, const char *name )
{
  int i;
  int count = 0;

  for( i = 0; i < menu->itemCount; i++ )
  {
    if( Q_stricmp( menu->items[i]->window.name, name ) == 0 ||
        ( menu->items[i]->window.group && Q_stricmp( menu->items[i]->window.group, name ) == 0 ) )
    {
      if( count == index )
        return menu->items[i];

      count++;
    }
  }

  return NULL;
}



void Script_SetColor( itemDef_t *item, char **args )
{
  const char *name;
  int i;
  float f;
  vec4_t *out;
  // expecting type of color to set and 4 args for the color

  if( String_Parse( args, &name ) )
  {
    out = NULL;

    if( Q_stricmp( name, "backcolor" ) == 0 )
    {
      out = &item->window.backColor;
      item->window.flags |= WINDOW_BACKCOLORSET;
    }
    else if( Q_stricmp( name, "forecolor" ) == 0 )
    {
      out = &item->window.foreColor;
      item->window.flags |= WINDOW_FORECOLORSET;
    }
    else if( Q_stricmp( name, "bordercolor" ) == 0 )
      out = &item->window.borderColor;

    if( out )
    {
      for( i = 0; i < 4; i++ )
      {
        if( !Float_Parse( args, &f ) )
          return;

        ( *out )[i] = f;
      }
    }
  }
}

void Script_SetAsset( itemDef_t *item, char **args )
{
  const char *name;
  // expecting name to set asset to

  if( String_Parse( args, &name ) )
  {
    // check for a model

    if( item->type == ITEM_TYPE_MODEL )
    {}

  }

}

void Script_SetBackground( itemDef_t *item, char **args )
{
  const char *name;
  // expecting name to set asset to

  if( String_Parse( args, &name ) )
    item->window.background = DC->registerShaderNoMip( name );
}




itemDef_t *Menu_FindItemByName( menuDef_t *menu, const char *p )
{
  int i;

  if( menu == NULL || p == NULL )
    return NULL;

  for( i = 0; i < menu->itemCount; i++ )
  {
    if( Q_stricmp( p, menu->items[i]->window.name ) == 0 )
      return menu->items[i];
  }

  return NULL;
}

void Script_SetItemColor( itemDef_t *item, char **args )
{
  const char *itemname;
  const char *name;
  vec4_t color;
  int i;
  vec4_t *out;
  // expecting type of color to set and 4 args for the color

  if( String_Parse( args, &itemname ) && String_Parse( args, &name ) )
  {
    itemDef_t * item2;
    int j;
    int count = Menu_ItemsMatchingGroup( item->parent, itemname );

    if( !Color_Parse( args, &color ) )
      return;

    for( j = 0; j < count; j++ )
    {
      item2 = Menu_GetMatchingItemByNumber( item->parent, j, itemname );

      if( item2 != NULL )
      {
        out = NULL;

        if( Q_stricmp( name, "backcolor" ) == 0 )
          out = &item2->window.backColor;
        else if( Q_stricmp( name, "forecolor" ) == 0 )
        {
          out = &item2->window.foreColor;
          item2->window.flags |= WINDOW_FORECOLORSET;
        }
        else if( Q_stricmp( name, "bordercolor" ) == 0 )
          out = &item2->window.borderColor;

        if( out )
        {
          for( i = 0; i < 4; i++ )
            ( *out )[i] = color[i];
        }
      }
    }
  }
}


void Menu_ShowItemByName( menuDef_t *menu, const char *p, qboolean bShow )
{
  itemDef_t * item;
  int i;
  int count = Menu_ItemsMatchingGroup( menu, p );

  for( i = 0; i < count; i++ )
  {
    item = Menu_GetMatchingItemByNumber( menu, i, p );

    if( item != NULL )
    {
      if( bShow )
        item->window.flags |= WINDOW_VISIBLE;
      else
      {
        item->window.flags &= ~WINDOW_VISIBLE;
        // stop cinematics playing in the window

        if( item->window.cinematic >= 0 )
        {
          DC->stopCinematic( item->window.cinematic );
          item->window.cinematic = -1;
        }
      }
    }
  }
}

void Menu_FadeItemByName( menuDef_t *menu, const char *p, qboolean fadeOut )
{
  itemDef_t * item;
  int i;
  int count = Menu_ItemsMatchingGroup( menu, p );

  for( i = 0; i < count; i++ )
  {
    item = Menu_GetMatchingItemByNumber( menu, i, p );

    if( item != NULL )
    {
      if( fadeOut )
      {
        item->window.flags |= ( WINDOW_FADINGOUT | WINDOW_VISIBLE );
        item->window.flags &= ~WINDOW_FADINGIN;
      }
      else
      {
        item->window.flags |= ( WINDOW_VISIBLE | WINDOW_FADINGIN );
        item->window.flags &= ~WINDOW_FADINGOUT;
      }
    }
  }
}

menuDef_t *Menus_FindByName( const char *p )
{
  int i;

  for( i = 0; i < menuCount; i++ )
  {
    if( Q_stricmp( Menus[i].window.name, p ) == 0 )
      return & Menus[i];
  }

  return NULL;
}

static void Menu_RunCloseScript( menuDef_t *menu )
{
  if( menu && menu->window.flags & WINDOW_VISIBLE && menu->onClose )
  {
    itemDef_t item;
    item.parent = menu;
    Item_RunScript( &item, menu->onClose );
  }
}

static void Menus_Close( menuDef_t *menu )
{
  if( menu != NULL )
  {
    Menu_RunCloseScript( menu );
    menu->window.flags &= ~( WINDOW_VISIBLE | WINDOW_HASFOCUS );

    if( openMenuCount > 0 )
      openMenuCount--;

    if( openMenuCount > 0 )
      Menus_Activate( menuStack[ openMenuCount - 1 ] );
  }
}

void Menus_CloseByName( const char *p )
{
  Menus_Close( Menus_FindByName( p ) );
}

void Menus_CloseAll( qboolean force )
{
  int i;

  // Close any menus on the stack first
  if( openMenuCount > 0 )
  {
    for( i = openMenuCount; i > 0; i-- )
      Menus_Close( menuStack[ i ] );

    openMenuCount = 0;
  }

  for( i = 0; i < menuCount; i++ )
    if( !( Menus[i].window.flags & WINDOW_DONTCLOSEALL ) || force )
      Menus_Close( &Menus[ i ] );

  if( force )
  {
    openMenuCount = 0;
    g_editingField = qfalse;
    g_waitingForKey = qfalse;
    g_editItem = NULL;
  }
}


void Script_Show( itemDef_t *item, char **args )
{
  const char *name;

  if( String_Parse( args, &name ) )
    Menu_ShowItemByName( item->parent, name, qtrue );
}

void Script_Hide( itemDef_t *item, char **args )
{
  const char *name;

  if( String_Parse( args, &name ) )
    Menu_ShowItemByName( item->parent, name, qfalse );
}

void Script_FadeIn( itemDef_t *item, char **args )
{
  const char *name;

  if( String_Parse( args, &name ) )
    Menu_FadeItemByName( item->parent, name, qfalse );
}

void Script_FadeOut( itemDef_t *item, char **args )
{
  const char *name;

  if( String_Parse( args, &name ) )
    Menu_FadeItemByName( item->parent, name, qtrue );
}



void Script_Open( itemDef_t *item, char **args )
{
  const char *name;

  if( String_Parse( args, &name ) )
    Menus_ActivateByName( name );
}

void Script_ConditionalOpen( itemDef_t *item, char **args )
{
  const char *cvar;
  const char *name1;
  const char *name2;
  float           val;

  if( String_Parse( args, &cvar ) && String_Parse( args, &name1 ) && String_Parse( args, &name2 ) )
  {
    val = DC->getCVarValue( cvar );

    if( val == 0.0f )
      Menus_ActivateByName( name2 );
    else
      Menus_ActivateByName( name1 );
  }
}

void Script_Close( itemDef_t *item, char **args )
{
  const char *name;

  if( String_Parse( args, &name ) )
    Menus_CloseByName( name );
}

void Menu_TransitionItemByName( menuDef_t *menu, const char *p, rectDef_t rectTo,
                                int time )
{
  itemDef_t *item;
  int i;
  int count = Menu_ItemsMatchingGroup( menu, p );

  for( i = 0; i < count; i++ )
  {
    item = Menu_GetMatchingItemByNumber( menu, i, p );

    if( item != NULL )
    {
      if (time)
      {
        item->window.flags |= WINDOW_INTRANSITION;
        memcpy( &item->window.rectEffects, &rectTo, sizeof( rectDef_t ) );
        item->window.offsetTime = DC->realTime;
        item->window.rectEffects2.x = abs( rectTo.x - item->window.rectClient.x ) / (float)time;
        item->window.rectEffects2.y = abs( rectTo.y - item->window.rectClient.y ) / (float)time;
        item->window.rectEffects2.w = abs( rectTo.w - item->window.rectClient.w ) / (float)time;
        item->window.rectEffects2.h = abs( rectTo.h - item->window.rectClient.h ) / (float)time;
      }
      else
      {
        memcpy( &item->window.rectClient, &rectTo, sizeof( rectDef_t ) );
        Item_UpdatePosition( item );
      }
    }
  }
}


void Script_Transition( itemDef_t *item, char **args )
{
  const char *name;
  rectDef_t rectTo;
  int time;

  if( String_Parse( args, &name ) )
  {
    if( Rect_Parse( args, &rectTo ) && Int_Parse( args, &time ) )
    {
      Menu_TransitionItemByName( item->parent, name, rectTo, time );
    }
  }
}


void Menu_OrbitItemByName( menuDef_t *menu, const char *p, float x, float y, float cx, float cy, int time )
{
  itemDef_t *item;
  int i;
  int count = Menu_ItemsMatchingGroup( menu, p );

  for( i = 0; i < count; i++ )
  {
    item = Menu_GetMatchingItemByNumber( menu, i, p );

    if( item != NULL )
    {
      item->window.flags |= ( WINDOW_ORBITING | WINDOW_VISIBLE );
      item->window.offsetTime = time;
      item->window.rectEffects.x = cx;
      item->window.rectEffects.y = cy;
      item->window.rectClient.x = x;
      item->window.rectClient.y = y;
      Item_UpdatePosition( item );
    }
  }
}


void Script_Orbit( itemDef_t *item, char **args )
{
  const char *name;
  float cx, cy, x, y;
  int time;

  if( String_Parse( args, &name ) )
  {
    if( Float_Parse( args, &x ) && Float_Parse( args, &y ) &&
        Float_Parse( args, &cx ) && Float_Parse( args, &cy ) && Int_Parse( args, &time ) )
    {
      Menu_OrbitItemByName( item->parent, name, x, y, cx, cy, time );
    }
  }
}



void Script_SetFocus( itemDef_t *item, char **args )
{
  const char *name;
  itemDef_t *focusItem;

  if( String_Parse( args, &name ) )
  {
    focusItem = Menu_FindItemByName( item->parent, name );

    if( focusItem && !( focusItem->window.flags & WINDOW_DECORATION ) )
    {
      Menu_ClearFocus( item->parent );
      focusItem->window.flags |= WINDOW_HASFOCUS;

      if( focusItem->onFocus )
        Item_RunScript( focusItem, focusItem->onFocus );

      // Edit fields get activated too
      if( Item_IsEditField( focusItem ) )
      {
        g_editingField = qtrue;
        g_editItem = focusItem;
      }

      if( DC->Assets.itemFocusSound )
        DC->startLocalSound( DC->Assets.itemFocusSound, CHAN_LOCAL_SOUND );
    }
  }
}

void Script_Reset( itemDef_t *item, char **args )
{
  const char *name;
  itemDef_t *resetItem;

  if( String_Parse( args, &name ) )
  {
    resetItem = Menu_FindItemByName( item->parent, name );

    if( resetItem )
    {
      if( resetItem->type == ITEM_TYPE_LISTBOX )
      {
        resetItem->cursorPos = 0;
        resetItem->typeData.list->startPos = 0;
        DC->feederSelection( resetItem->feederID, 0 );
      }
    }
  }
}

void Script_SetPlayerModel( itemDef_t *item, char **args )
{
  const char *name;

  if( String_Parse( args, &name ) )
    DC->setCVar( "team_model", name );
}

void Script_SetPlayerHead( itemDef_t *item, char **args )
{
  const char *name;

  if( String_Parse( args, &name ) )
    DC->setCVar( "team_headmodel", name );
}

void Script_SetCvar( itemDef_t *item, char **args )
{
  const char *cvar, *val;

  if( String_Parse( args, &cvar ) && String_Parse( args, &val ) )
    DC->setCVar( cvar, val );

}

void Script_Exec( itemDef_t *item, char **args )
{
  const char *val;

  if( String_Parse( args, &val ) )
    DC->executeText( EXEC_APPEND, va( "%s ; ", val ) );
}

void Script_Play( itemDef_t *item, char **args )
{
  const char *val;

  if( String_Parse( args, &val ) )
    DC->startLocalSound( DC->registerSound( val, qfalse ), CHAN_LOCAL_SOUND );
}

void Script_playLooped( itemDef_t *item, char **args )
{
  const char *val;

  if( String_Parse( args, &val ) )
  {
    DC->stopBackgroundTrack();
    DC->startBackgroundTrack( val, val );
  }
}

void Menu_DelayItemByName( menuDef_t *menu, const char *p, int time )
{
  itemDef_t *item;
  int i, j = 0;
  int count = Menu_ItemsMatchingGroup( menu, p );

  for( i = 0; i < count; i++ )
  {
    item = Menu_GetMatchingItemByNumber( menu, i, p );

    if( item != NULL && item->delayEvent && item->delayEvent[ 0 ] )
    {
      for( ; j < MAX_DELAYED_COMMANDS; j++ )
      {
        if( !delayed_cmd[ j ].time )
        {
          delayed_cmd[ j ].item = item;
          delayed_cmd[ j ].time = DC->realTime + time;
          j++;
          break;
        }
      }
    }
  }
}

void Script_Delay( itemDef_t *item, char **args )
{
  const char *name;
  int time;

  if( Int_Parse( args, &time ) && String_Parse( args, &name ) )
  {
    Menu_DelayItemByName( item->parent, name, time );
  }
}

static qboolean UI_Text_IsEmoticon( const char *s, qboolean *escaped,
                                    int *length, qhandle_t *h, int *width )
{
  char name[ MAX_EMOTICON_NAME_LEN ] = {""};
  const char *p = s;
  int i = 0;
  int j = 0;

  if( *p != '[' )
    return qfalse;
  p++;

  *escaped = qfalse;
  if( *p == '[' )
  {
    *escaped = qtrue;
    p++;
  }

  while( *p && i < ( MAX_EMOTICON_NAME_LEN - 1 ) )
  {
    if( *p == ']' )
    {
      for( j = 0; j < DC->Assets.emoticonCount; j++ )
      {
        if( !Q_stricmp( DC->Assets.emoticons[ j ], name ) )
        {
          if( *escaped )
          {
            *length = 1;
            return qtrue;
          }
          if( h )
            *h = DC->Assets.emoticonShaders[ j ];
          if( width )
            *width = DC->Assets.emoticonWidths[ j ];
          *length = i + 2;
          return qtrue;
        }
      }
      return qfalse;
    }
    name[ i++ ] = *p;
    name[ i ] = '\0';
    p++;
  }
  return qfalse;
}


float UI_Text_Width( const char *text, float scale, int limit )
{
  int         count, len;
  float       out;
  glyphInfo_t *glyph;
  float       useScale;
  const char  *s = text;
  fontInfo_t  *font = &DC->Assets.textFont;
  int         emoticonLen;
  qboolean    emoticonEscaped;
  float       emoticonW;
  int         emoticonWidth;
  int         emoticons = 0;

  if( scale <= DC->getCVarValue( "ui_smallFont" ) )
    font = &DC->Assets.smallFont;
  else if( scale >= DC->getCVarValue( "ui_bigFont" ) )
    font = &DC->Assets.bigFont;

  useScale = scale * font->glyphScale;
  emoticonW = UI_Text_Height( "[", scale, 0 ) * DC->aspectScale;
  out = 0;

  if( text )
  {
    len = Q_PrintStrlen( text );

    if( limit > 0 && len > limit )
      len = limit;

    count = 0;

    while( s && *s && count < len )
    {
      glyph = &font->glyphs[( unsigned char )*s];

      if( Q_IsColorString( s ) )
      {
        s += 2;
        continue;
      }

      if( UI_Text_IsEmoticon( s, &emoticonEscaped, &emoticonLen,
                              NULL, &emoticonWidth ) )
      {
        if( emoticonEscaped )
          s++;
        else
        {
          s += emoticonLen;
          count += emoticonLen;
          emoticons += emoticonWidth;
          continue;
        }
      }
      out += ( glyph->xSkip * DC->aspectScale );
      s++;
      count++;
    }
  }

  return ( out * useScale ) + ( emoticons * emoticonW );
}

float UI_Text_Height( const char *text, float scale, int limit )
{
  int         len, count;
  float       max;
  glyphInfo_t *glyph;
  float       useScale;
  const char  *s = text;
  fontInfo_t  *font = &DC->Assets.textFont;

  if( scale <= DC->getCVarValue( "ui_smallFont" ) )
    font = &DC->Assets.smallFont;
  else if( scale >= DC->getCVarValue( "ui_bigFont" ) )
    font = &DC->Assets.bigFont;

  useScale = scale * font->glyphScale;
  max = 0;

  if( text )
  {
    len = strlen( text );

    if( limit > 0 && len > limit )
      len = limit;

    count = 0;

    while( s && *s && count < len )
    {
      if( Q_IsColorString( s ) )
      {
        s += 2;
        continue;
      }
      else
      {
        glyph = &font->glyphs[( unsigned char )*s];

        if( max < glyph->height )
          max = glyph->height;

        s++;
        count++;
      }
    }
  }

  return max * useScale;
}

float UI_Text_EmWidth( float scale )
{
  return UI_Text_Width( "M", scale, 0 );
}

float UI_Text_EmHeight( float scale )
{
  return UI_Text_Height( "M", scale, 0 );
}


/*
================
UI_AdjustFrom640
 
Adjusted for resolution and screen aspect ratio
================
*/
void UI_AdjustFrom640( float *x, float *y, float *w, float *h )
{
  *x *= DC->xscale;
  *y *= DC->yscale;
  *w *= DC->xscale;
  *h *= DC->yscale;
}

static void UI_Text_PaintChar( float x, float y, float scale,
                               glyphInfo_t *glyph, float size )
{
  float w, h;

  w = glyph->imageWidth;
  h = glyph->imageHeight;

  if( size > 0.0f )
  {
    float half = size * 0.5f * scale;
    x -= half;
    y -= half;
    w += size;
    h += size;
  }

  w *= ( DC->aspectScale * scale );
  h *= scale;
  y -= ( glyph->top * scale );
  UI_AdjustFrom640( &x, &y, &w, &h );

  DC->drawStretchPic( x, y, w, h,
                      glyph->s,
                      glyph->t,
                      glyph->s2,
                      glyph->t2,
                      glyph->glyph );
}

static void UI_Text_Paint_Generic( float x, float y, float scale, float gapAdjust,
                                   const char *text, vec4_t color, int style,
                                   int limit, float *maxX,
                                   int cursorPos, char cursor )
{
  const char  *s = text;
  int         len;
  int         count = 0;
  vec4_t      newColor;
  fontInfo_t  *font = &DC->Assets.textFont;
  glyphInfo_t *glyph;
  float       useScale;
  qhandle_t   emoticonHandle = 0;
  float       emoticonH, emoticonW;
  qboolean    emoticonEscaped;
  int         emoticonLen = 0;
  int         emoticonWidth;
  int         cursorX = -1;

  if( !text )
    return;

  if( scale <= DC->getCVarValue( "ui_smallFont" ) )
    font = &DC->Assets.smallFont;
  else if( scale >= DC->getCVarValue( "ui_bigFont" ) )
    font = &DC->Assets.bigFont;

  useScale = scale * font->glyphScale;

  emoticonH = font->glyphs[ (int)'[' ].height * useScale;
  emoticonW = emoticonH * DC->aspectScale;

  len = strlen( text );
  if( limit > 0 && len > limit )
    len = limit;

  DC->setColor( color );
  memcpy( &newColor[0], &color[0], sizeof( vec4_t ) );

  while( s && *s && count < len )
  {
    glyph = &font->glyphs[ (int)*s ];

    if( maxX && UI_Text_Width( s, scale, 1 ) + x > *maxX )
    {
      *maxX = 0;
      break;
    }

    if( cursorPos < 0 )
    {
      if( Q_IsColorString( s ) )
      {
        memcpy( newColor, g_color_table[ColorIndex( *( s+1 ) )],
                sizeof( newColor ) );
        newColor[3] = color[3];
        DC->setColor( newColor );
        s += 2;
        continue;
      }

      if( UI_Text_IsEmoticon( s, &emoticonEscaped, &emoticonLen,
                              &emoticonHandle, &emoticonWidth ) )
      {
        if( emoticonEscaped )
          s++;
        else
        {
          float yadj = useScale * glyph->top;

          DC->setColor( NULL );
          DC->drawHandlePic( x, y - yadj,
                             ( emoticonW * emoticonWidth ),
                             emoticonH,
                             emoticonHandle );
          DC->setColor( newColor );
          x += ( emoticonW * emoticonWidth ) + gapAdjust;
          s += emoticonLen;
          count += emoticonWidth;
          continue;
        }
      }
    }
    else
    {
      if( Q_IsColorString( s ) )
      {
        memcpy( newColor, g_color_table[ColorIndex( *( s+1 ) )],
                sizeof( newColor ) );
        newColor[3] = color[3];
        DC->setColor( newColor );
      }
    }

    if( style == ITEM_TEXTSTYLE_SHADOWED ||
       style == ITEM_TEXTSTYLE_SHADOWEDMORE )
    {
      int ofs;

      if( style == ITEM_TEXTSTYLE_SHADOWED )
        ofs = 1;
      else
        ofs = 2;
      colorBlack[3] = newColor[3];
      DC->setColor( colorBlack );
      UI_Text_PaintChar( x + ofs, y + ofs, useScale, glyph, 0.0f );
      DC->setColor( newColor );
      colorBlack[3] = 1.0f;
    }
    else if( style == ITEM_TEXTSTYLE_NEON )
    {
      vec4_t glow;

      memcpy( &glow[0], &newColor[0], sizeof( vec4_t ) );
      glow[ 3 ] *= 0.2f;

      DC->setColor( glow );
      UI_Text_PaintChar( x, y, useScale, glyph, 6.0f );
      UI_Text_PaintChar( x, y, useScale, glyph, 4.0f );
      DC->setColor( newColor );
      UI_Text_PaintChar( x, y, useScale, glyph, 2.0f );

      DC->setColor( colorWhite );
    }

    UI_Text_PaintChar( x, y, useScale, glyph, 0.0f );

    if( count == cursorPos )
      cursorX = x;

    x += ( glyph->xSkip * DC->aspectScale * useScale ) + gapAdjust;
    s++;
    count++;

  }

  if( maxX )
      *maxX = x;

  // paint cursor
  if( cursorPos >= 0 )
  {
    if( cursorPos == len )
      cursorX = x;

    if( cursorX >= 0 && !( ( DC->realTime / BLINK_DIVISOR ) & 1 ) )
    {
      glyph = &font->glyphs[ (int)cursor ];
      UI_Text_PaintChar( cursorX, y, useScale, glyph, 0.0f );
    }
  }

  DC->setColor( NULL );
}

void UI_Text_Paint_Limit( float *maxX, float x, float y, float scale,
                          vec4_t color, const char *text, float adjust, int limit )
{
  UI_Text_Paint_Generic( x, y, scale, adjust,
                         text, color, ITEM_TEXTSTYLE_NORMAL,
                         limit, maxX, -1, 0 );
}

void UI_Text_Paint( float x, float y, float scale, vec4_t color, const char *text,
                    float adjust, int limit, int style )
{
  UI_Text_Paint_Generic( x, y, scale, adjust,
                         text, color, style,
                         limit, NULL, -1, 0 );
}

void UI_Text_PaintWithCursor( float x, float y, float scale, vec4_t color, const char *text,
                              int cursorPos, char cursor, int limit, int style )
{
  UI_Text_Paint_Generic( x, y, scale, 0.0,
                         text, color, style,
                         limit, NULL, cursorPos, cursor );
}

commandDef_t commandList[] =
  {
    {"close", &Script_Close},                     // menu
    {"conditionalopen", &Script_ConditionalOpen}, // menu
    {"delay", &Script_Delay},                     // group/name
    {"exec", &Script_Exec},                       // group/name
    {"fadein", &Script_FadeIn},                   // group/name
    {"fadeout", &Script_FadeOut},                 // group/name
    {"hide", &Script_Hide},                       // group/name
    {"open", &Script_Open},                       // menu
    {"orbit", &Script_Orbit},                     // group/name
    {"play", &Script_Play},                       // group/name
    {"playlooped", &Script_playLooped},           // group/name
    {"reset", &Script_Reset},                     // resets the state of the item argument
    {"setasset", &Script_SetAsset},               // works on this
    {"setbackground", &Script_SetBackground},     // works on this
    {"setcolor", &Script_SetColor},               // works on this
    {"setcvar", &Script_SetCvar},                 // group/name
    {"setfocus", &Script_SetFocus},               // sets this background color to team color
    {"setitemcolor", &Script_SetItemColor},       // group/name
    {"setplayerhead", &Script_SetPlayerHead},     // sets this background color to team color
    {"setplayermodel", &Script_SetPlayerModel},   // sets this background color to team color
    {"show", &Script_Show},                       // group/name
    {"transition", &Script_Transition},           // group/name
  };

static size_t scriptCommandCount = sizeof( commandList ) / sizeof( commandDef_t );

// despite what lcc thinks, we do not get cmdcmp here
static int commandComp( const void *a, const void *b )
{
  return Q_stricmp( (const char *)a, ((commandDef_t *)b)->name );
}

void Item_RunScript( itemDef_t *item, const char *s )
{
  char script[1024], *p;
  commandDef_t *cmd;
  memset( script, 0, sizeof( script ) );

  if( item && s && s[0] )
  {
    Q_strcat( script, 1024, s );
    p = script;

    while( 1 )
    {
      const char *command;
      // expect command then arguments, ; ends command, NULL ends script

      if( !String_Parse( &p, &command ) )
        return;

      if( command[0] == ';' && command[1] == '\0' )
        continue;

      cmd = bsearch( command, commandList, scriptCommandCount,
        sizeof( commandDef_t ), commandComp );
      if( cmd )
        cmd->handler( item, &p );
      else
      // not in our auto list, pass to handler
        DC->runScript( &p );
    }
  }
}


qboolean Item_EnableShowViaCvar( itemDef_t *item, int flag )
{
  char script[1024], *p;
  memset( script, 0, sizeof( script ) );

  if( item && item->enableCvar && *item->enableCvar && item->cvarTest && *item->cvarTest )
  {
    char buff[1024];
    DC->getCVarString( item->cvarTest, buff, sizeof( buff ) );

    Q_strcat( script, 1024, item->enableCvar );
    p = script;

    while( 1 )
    {
      const char *val;
      // expect value then ; or NULL, NULL ends list

      if( !String_Parse( &p, &val ) )
        return ( item->cvarFlags & flag ) ? qfalse : qtrue;

      if( val[0] == ';' && val[1] == '\0' )
        continue;

      // enable it if any of the values are true
      if( item->cvarFlags & flag )
      {
        if( Q_stricmp( buff, val ) == 0 )
          return qtrue;
      }
      else
      {
        // disable it if any of the values are true

        if( Q_stricmp( buff, val ) == 0 )
          return qfalse;
      }

    }

    return ( item->cvarFlags & flag ) ? qfalse : qtrue;
  }

  return qtrue;
}


// will optionaly set focus to this item
qboolean Item_SetFocus( itemDef_t *item, float x, float y )
{
  int i;
  itemDef_t *oldFocus;
  sfxHandle_t *sfx = &DC->Assets.itemFocusSound;
  qboolean playSound = qfalse;
  menuDef_t *parent;
  // sanity check, non-null, not a decoration and does not already have the focus

  if( item == NULL || item->window.flags & WINDOW_DECORATION ||
      item->window.flags & WINDOW_HASFOCUS || !( item->window.flags & WINDOW_VISIBLE ) )
  {
    return qfalse;
  }

  parent = ( menuDef_t* )item->parent;

  // items can be enabled and disabled based on cvars

  if( item->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) && !Item_EnableShowViaCvar( item, CVAR_ENABLE ) )
    return qfalse;

  if( item->cvarFlags & ( CVAR_SHOW | CVAR_HIDE ) && !Item_EnableShowViaCvar( item, CVAR_SHOW ) )
    return qfalse;

  oldFocus = Menu_ClearFocus( item->parent );

  if( item->type == ITEM_TYPE_TEXT )
  {
    rectDef_t r;
    r = item->textRect;
    r.y -= r.h;

    if( Rect_ContainsPoint( &r, x, y ) )
    {
      item->window.flags |= WINDOW_HASFOCUS;

      if( item->focusSound )
        sfx = &item->focusSound;

      playSound = qtrue;
    }
    else
    {
      if( oldFocus )
      {
        oldFocus->window.flags |= WINDOW_HASFOCUS;

        if( oldFocus->onFocus )
          Item_RunScript( oldFocus, oldFocus->onFocus );
      }
    }
  }
  else
  {
    item->window.flags |= WINDOW_HASFOCUS;

    if( item->onFocus )
      Item_RunScript( item, item->onFocus );

    if( item->focusSound )
      sfx = &item->focusSound;

    playSound = qtrue;
  }

  if( playSound && sfx )
    DC->startLocalSound( *sfx, CHAN_LOCAL_SOUND );

  for( i = 0; i < parent->itemCount; i++ )
  {
    if( parent->items[i] == item )
    {
      parent->cursorItem = i;
      break;
    }
  }

  return qtrue;
}

int Item_ListBox_MaxScroll( itemDef_t *item )
{
  listBoxDef_t *listPtr = item->typeData.list;
  int count = DC->feederCount( item->feederID );
  int max;

  if( item->window.flags & WINDOW_HORIZONTAL )
    max = count - ( item->window.rect.w / listPtr->elementWidth ) + 1;
  else
    max = count - ( item->window.rect.h / listPtr->elementHeight ) + 1;

  if( max < 0 )
    return 0;

  return max;
}

int Item_ListBox_ThumbPosition( itemDef_t *item )
{
  float max, pos, size;
  int startPos = item->typeData.list->startPos;

  max = Item_ListBox_MaxScroll( item );

  if( item->window.flags & WINDOW_HORIZONTAL )
  {
    size = item->window.rect.w - ( SCROLLBAR_WIDTH * 2 ) - 2;

    if( max > 0 )
      pos = ( size - SCROLLBAR_WIDTH ) / ( float ) max;
    else
      pos = 0;

    pos *= startPos;
    return item->window.rect.x + 1 + SCROLLBAR_WIDTH + pos;
  }
  else
  {
    size = item->window.rect.h - ( SCROLLBAR_HEIGHT * 2 ) - 2;

    if( max > 0 )
      pos = ( size - SCROLLBAR_HEIGHT ) / ( float ) max;
    else
      pos = 0;

    pos *= startPos;
    return item->window.rect.y + 1 + SCROLLBAR_HEIGHT + pos;
  }
}

int Item_ListBox_ThumbDrawPosition( itemDef_t *item )
{
  int min, max;

  if( itemCapture == item )
  {
    if( item->window.flags & WINDOW_HORIZONTAL )
    {
      min = item->window.rect.x + SCROLLBAR_WIDTH + 1;
      max = item->window.rect.x + item->window.rect.w - 2 * SCROLLBAR_WIDTH - 1;

      if( DC->cursorx >= min + SCROLLBAR_WIDTH / 2 && DC->cursorx <= max + SCROLLBAR_WIDTH / 2 )
        return DC->cursorx - SCROLLBAR_WIDTH / 2;
      else
        return Item_ListBox_ThumbPosition( item );
    }
    else
    {
      min = item->window.rect.y + SCROLLBAR_HEIGHT + 1;
      max = item->window.rect.y + item->window.rect.h - 2 * SCROLLBAR_HEIGHT - 1;

      if( DC->cursory >= min + SCROLLBAR_HEIGHT / 2 && DC->cursory <= max + SCROLLBAR_HEIGHT / 2 )
        return DC->cursory - SCROLLBAR_HEIGHT / 2;
      else
        return Item_ListBox_ThumbPosition( item );
    }
  }
  else
    return Item_ListBox_ThumbPosition( item );
}

float Item_Slider_ThumbPosition( itemDef_t *item )
{
  float value, range, x;
  editFieldDef_t *editDef = item->typeData.edit;

  if( item->text )
    x = item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET;
  else
    x = item->window.rect.x;

  if( editDef == NULL && item->cvar )
    return x;

  value = DC->getCVarValue( item->cvar );

  if( value < editDef->minVal )
    value = editDef->minVal;
  else if( value > editDef->maxVal )
    value = editDef->maxVal;

  range = editDef->maxVal - editDef->minVal;
  value -= editDef->minVal;
  value /= range;
  value *= SLIDER_WIDTH - SLIDER_THUMB_WIDTH;
  x += value;

  return x;
}

static float Item_Slider_VScale( itemDef_t *item )
{
  if( SLIDER_THUMB_HEIGHT > item->window.rect.h )
    return item->window.rect.h / SLIDER_THUMB_HEIGHT;
  else
    return 1.0f;
}

int Item_Slider_OverSlider( itemDef_t *item, float x, float y )
{
  rectDef_t r;
  float vScale = Item_Slider_VScale( item );

  r.x = Item_Slider_ThumbPosition( item );
  r.y = item->textRect.y - item->textRect.h +
        ( ( item->textRect.h - ( SLIDER_THUMB_HEIGHT * vScale ) ) / 2.0f );
  r.w = SLIDER_THUMB_WIDTH;
  r.h = SLIDER_THUMB_HEIGHT * vScale;

  if( Rect_ContainsPoint( &r, x, y ) )
    return WINDOW_LB_THUMB;

  return 0;
}

int Item_ListBox_OverLB( itemDef_t *item, float x, float y )
{
  rectDef_t r;
  int thumbstart;
  int count;

  count = DC->feederCount( item->feederID );

  if( item->window.flags & WINDOW_HORIZONTAL )
  {
    // check if on left arrow
    r.x = item->window.rect.x;
    r.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_HEIGHT;
    r.w = SCROLLBAR_WIDTH;
    r.h = SCROLLBAR_HEIGHT;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_LEFTARROW;

    // check if on right arrow
    r.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_WIDTH;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_RIGHTARROW;

    // check if on thumb
    thumbstart = Item_ListBox_ThumbPosition( item );

    r.x = thumbstart;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_THUMB;

    r.x = item->window.rect.x + SCROLLBAR_WIDTH;
    r.w = thumbstart - r.x;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_PGUP;

    r.x = thumbstart + SCROLLBAR_WIDTH;
    r.w = item->window.rect.x + item->window.rect.w - SCROLLBAR_WIDTH;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_PGDN;
  }
  else
  {
    r.x = item->window.rect.x + item->window.rect.w - SCROLLBAR_WIDTH;
    r.y = item->window.rect.y;
    r.w = SCROLLBAR_WIDTH;
    r.h = SCROLLBAR_HEIGHT;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_LEFTARROW;

    r.y = item->window.rect.y + item->window.rect.h - SCROLLBAR_HEIGHT;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_RIGHTARROW;

    thumbstart = Item_ListBox_ThumbPosition( item );
    r.y = thumbstart;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_THUMB;

    r.y = item->window.rect.y + SCROLLBAR_HEIGHT;
    r.h = thumbstart - r.y;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_PGUP;

    r.y = thumbstart + SCROLLBAR_HEIGHT;
    r.h = item->window.rect.y + item->window.rect.h - SCROLLBAR_HEIGHT;

    if( Rect_ContainsPoint( &r, x, y ) )
      return WINDOW_LB_PGDN;
  }

  return 0;
}


void Item_ListBox_MouseEnter( itemDef_t *item, float x, float y )
{
  rectDef_t r;
  listBoxDef_t *listPtr = item->typeData.list;

  item->window.flags &= ~( WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB |
                           WINDOW_LB_PGUP | WINDOW_LB_PGDN );
  item->window.flags |= Item_ListBox_OverLB( item, x, y );

  if( item->window.flags & WINDOW_HORIZONTAL )
  {
    if( !( item->window.flags & ( WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW | WINDOW_LB_THUMB |
                                  WINDOW_LB_PGUP | WINDOW_LB_PGDN ) ) )
    {
      // check for selection hit as we have exausted buttons and thumb

      if( listPtr->elementStyle == LISTBOX_IMAGE )
      {
        r.x = item->window.rect.x;
        r.y = item->window.rect.y;
        r.h = item->window.rect.h - SCROLLBAR_HEIGHT;
        r.w = item->window.rect.w - listPtr->drawPadding;

        if( Rect_ContainsPoint( &r, x, y ) )
        {
          listPtr->cursorPos =  ( int )( ( x - r.x ) / listPtr->elementWidth )  + listPtr->startPos;

          if( listPtr->cursorPos >= listPtr->endPos )
            listPtr->cursorPos = listPtr->endPos;
        }
      }
    }
  }
  else if( !( item->window.flags & ( WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW |
                                     WINDOW_LB_THUMB | WINDOW_LB_PGUP | WINDOW_LB_PGDN ) ) )
  {
    r.x = item->window.rect.x;
    r.y = item->window.rect.y;
    r.w = item->window.rect.w - SCROLLBAR_WIDTH;
    r.h = item->window.rect.h - listPtr->drawPadding;

    if( Rect_ContainsPoint( &r, x, y ) )
    {
      listPtr->cursorPos =  ( int )( ( y - 2 - r.y ) / listPtr->elementHeight )  + listPtr->startPos;

      if( listPtr->cursorPos > listPtr->endPos )
        listPtr->cursorPos = listPtr->endPos;
    }
  }
}

void Item_MouseEnter( itemDef_t *item, float x, float y )
{
  rectDef_t r;

  if( item )
  {
    r = item->textRect;
    r.y -= r.h;
    // in the text rect?

    // items can be enabled and disabled based on cvars

    if( item->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) && !Item_EnableShowViaCvar( item, CVAR_ENABLE ) )
      return;

    if( item->cvarFlags & ( CVAR_SHOW | CVAR_HIDE ) && !Item_EnableShowViaCvar( item, CVAR_SHOW ) )
      return;

    if( Rect_ContainsPoint( &r, x, y ) )
    {
      if( !( item->window.flags & WINDOW_MOUSEOVERTEXT ) )
      {
        Item_RunScript( item, item->mouseEnterText );
        item->window.flags |= WINDOW_MOUSEOVERTEXT;
      }

      if( !( item->window.flags & WINDOW_MOUSEOVER ) )
      {
        Item_RunScript( item, item->mouseEnter );
        item->window.flags |= WINDOW_MOUSEOVER;
      }

    }
    else
    {
      // not in the text rect

      if( item->window.flags & WINDOW_MOUSEOVERTEXT )
      {
        // if we were
        Item_RunScript( item, item->mouseExitText );
        item->window.flags &= ~WINDOW_MOUSEOVERTEXT;
      }

      if( !( item->window.flags & WINDOW_MOUSEOVER ) )
      {
        Item_RunScript( item, item->mouseEnter );
        item->window.flags |= WINDOW_MOUSEOVER;
      }

      if( item->type == ITEM_TYPE_LISTBOX )
        Item_ListBox_MouseEnter( item, x, y );
    }
  }
}

void Item_MouseLeave( itemDef_t *item )
{
  if( item )
  {
    if( item->window.flags & WINDOW_MOUSEOVERTEXT )
    {
      Item_RunScript( item, item->mouseExitText );
      item->window.flags &= ~WINDOW_MOUSEOVERTEXT;
    }

    Item_RunScript( item, item->mouseExit );
    item->window.flags &= ~( WINDOW_LB_RIGHTARROW | WINDOW_LB_LEFTARROW );
  }
}

itemDef_t *Menu_HitTest( menuDef_t *menu, float x, float y )
{
  int i;

  for( i = 0; i < menu->itemCount; i++ )
  {
    if( Rect_ContainsPoint( &menu->items[i]->window.rect, x, y ) )
      return menu->items[i];
  }

  return NULL;
}

void Item_SetMouseOver( itemDef_t *item, qboolean focus )
{
  if( item )
  {
    if( focus )
      item->window.flags |= WINDOW_MOUSEOVER;
    else
      item->window.flags &= ~WINDOW_MOUSEOVER;
  }
}


qboolean Item_OwnerDraw_HandleKey( itemDef_t *item, int key )
{
  if( item && DC->ownerDrawHandleKey )
    return DC->ownerDrawHandleKey( item->window.ownerDraw, key );

  return qfalse;
}

qboolean Item_ListBox_HandleKey( itemDef_t *item, int key, qboolean down, qboolean force )
{
  listBoxDef_t *listPtr = item->typeData.list;
  int count = DC->feederCount( item->feederID );
  int max, viewmax;

  if( force || ( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) &&
      item->window.flags & WINDOW_HASFOCUS ) )
  {
    max = Item_ListBox_MaxScroll( item );

    if( item->window.flags & WINDOW_HORIZONTAL )
    {
      viewmax = ( item->window.rect.w / listPtr->elementWidth );

      if( key == K_LEFTARROW || key == K_KP_LEFTARROW )
      {
        if( !listPtr->notselectable )
        {
          listPtr->cursorPos--;

          if( listPtr->cursorPos < 0 )
            listPtr->cursorPos = 0;

          if( listPtr->cursorPos < listPtr->startPos )
            listPtr->startPos = listPtr->cursorPos;

          if( listPtr->cursorPos >= listPtr->startPos + viewmax )
            listPtr->startPos = listPtr->cursorPos - viewmax + 1;

          item->cursorPos = listPtr->cursorPos;
          DC->feederSelection( item->feederID, item->cursorPos );
        }
        else
        {
          listPtr->startPos--;

          if( listPtr->startPos < 0 )
            listPtr->startPos = 0;
        }

        return qtrue;
      }

      if( key == K_RIGHTARROW || key == K_KP_RIGHTARROW )
      {
        if( !listPtr->notselectable )
        {
          listPtr->cursorPos++;

          if( listPtr->cursorPos < listPtr->startPos )
            listPtr->startPos = listPtr->cursorPos;

          if( listPtr->cursorPos >= count )
            listPtr->cursorPos = count - 1;

          if( listPtr->cursorPos >= listPtr->startPos + viewmax )
            listPtr->startPos = listPtr->cursorPos - viewmax + 1;

          item->cursorPos = listPtr->cursorPos;
          DC->feederSelection( item->feederID, item->cursorPos );
        }
        else
        {
          listPtr->startPos++;

          if( listPtr->startPos >= count )
            listPtr->startPos = count - 1;
        }

        return qtrue;
      }
    }
    else
    {
      viewmax = ( item->window.rect.h / listPtr->elementHeight );

      if( key == K_UPARROW || key == K_KP_UPARROW )
      {
        if( !listPtr->notselectable )
        {
          listPtr->cursorPos--;

          if( listPtr->cursorPos < 0 )
            listPtr->cursorPos = 0;

          if( listPtr->cursorPos < listPtr->startPos )
            listPtr->startPos = listPtr->cursorPos;

          if( listPtr->cursorPos >= listPtr->startPos + viewmax )
            listPtr->startPos = listPtr->cursorPos - viewmax + 1;

          item->cursorPos = listPtr->cursorPos;
          DC->feederSelection( item->feederID, item->cursorPos );
        }
        else
        {
          listPtr->startPos--;

          if( listPtr->startPos < 0 )
            listPtr->startPos = 0;
        }

        return qtrue;
      }

      if( key == K_DOWNARROW || key == K_KP_DOWNARROW )
      {
        if( !listPtr->notselectable )
        {
          listPtr->cursorPos++;

          if( listPtr->cursorPos < listPtr->startPos )
            listPtr->startPos = listPtr->cursorPos;

          if( listPtr->cursorPos >= count )
            listPtr->cursorPos = count - 1;

          if( listPtr->cursorPos >= listPtr->startPos + viewmax )
            listPtr->startPos = listPtr->cursorPos - viewmax + 1;

          item->cursorPos = listPtr->cursorPos;
          DC->feederSelection( item->feederID, item->cursorPos );
        }
        else
        {
          listPtr->startPos++;

          if( listPtr->startPos > max )
            listPtr->startPos = max;
        }

        return qtrue;
      }
    }

    // mouse hit
    if( key == K_MOUSE1 || key == K_MOUSE2 )
    {
      if( item->window.flags & WINDOW_LB_LEFTARROW )
      {
        listPtr->startPos--;

        if( listPtr->startPos < 0 )
          listPtr->startPos = 0;
      }
      else if( item->window.flags & WINDOW_LB_RIGHTARROW )
      {
        // one down
        listPtr->startPos++;

        if( listPtr->startPos > max )
          listPtr->startPos = max;
      }
      else if( item->window.flags & WINDOW_LB_PGUP )
      {
        // page up
        listPtr->startPos -= viewmax;

        if( listPtr->startPos < 0 )
          listPtr->startPos = 0;
      }
      else if( item->window.flags & WINDOW_LB_PGDN )
      {
        // page down
        listPtr->startPos += viewmax;

        if( listPtr->startPos > max )
          listPtr->startPos = max;
      }
      else if( item->window.flags & WINDOW_LB_THUMB )
      {
        // Display_SetCaptureItem(item);
      }
      else
      {
        // select an item

        if( item->cursorPos != listPtr->cursorPos )
        {
          item->cursorPos = listPtr->cursorPos;
          DC->feederSelection( item->feederID, item->cursorPos );
        }

        if( DC->realTime < lastListBoxClickTime && listPtr->doubleClick )
          Item_RunScript( item, listPtr->doubleClick );

        lastListBoxClickTime = DC->realTime + DOUBLE_CLICK_DELAY;
      }

      return qtrue;
    }

    // Scroll wheel
    if( key == K_MWHEELUP )
    {
      listPtr->startPos--;

      if( listPtr->startPos < 0 )
        listPtr->startPos = 0;

      return qtrue;
    }

    if( key == K_MWHEELDOWN )
    {
      listPtr->startPos++;

      if( listPtr->startPos > max )
        listPtr->startPos = max;

      return qtrue;
    }

    // Invoke the doubleClick handler when enter is pressed
    if( key == K_ENTER )
    {
      if( listPtr->doubleClick )
        Item_RunScript( item, listPtr->doubleClick );

      return qtrue;
    }

    if( key == K_HOME || key == K_KP_HOME )
    {
      // home
      listPtr->startPos = 0;
      return qtrue;
    }

    if( key == K_END || key == K_KP_END )
    {
      // end
      listPtr->startPos = max;
      return qtrue;
    }

    if( key == K_PGUP || key == K_KP_PGUP )
    {
      // page up

      if( !listPtr->notselectable )
      {
        listPtr->cursorPos -= viewmax;

        if( listPtr->cursorPos < 0 )
          listPtr->cursorPos = 0;

        if( listPtr->cursorPos < listPtr->startPos )
          listPtr->startPos = listPtr->cursorPos;

        if( listPtr->cursorPos >= listPtr->startPos + viewmax )
          listPtr->startPos = listPtr->cursorPos - viewmax + 1;

        item->cursorPos = listPtr->cursorPos;
        DC->feederSelection( item->feederID, item->cursorPos );
      }
      else
      {
        listPtr->startPos -= viewmax;

        if( listPtr->startPos < 0 )
          listPtr->startPos = 0;
      }

      return qtrue;
    }

    if( key == K_PGDN || key == K_KP_PGDN )
    {
      // page down

      if( !listPtr->notselectable )
      {
        listPtr->cursorPos += viewmax;

        if( listPtr->cursorPos < listPtr->startPos )
          listPtr->startPos = listPtr->cursorPos;

        if( listPtr->cursorPos >= count )
          listPtr->cursorPos = count - 1;

        if( listPtr->cursorPos >= listPtr->startPos + viewmax )
          listPtr->startPos = listPtr->cursorPos - viewmax + 1;

        item->cursorPos = listPtr->cursorPos;
        DC->feederSelection( item->feederID, item->cursorPos );
      }
      else
      {
        listPtr->startPos += viewmax;

        if( listPtr->startPos > max )
          listPtr->startPos = max;
      }

      return qtrue;
    }
  }

  return qfalse;
}

qboolean Item_YesNo_HandleKey( itemDef_t *item, int key )
{
  if( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) &&
      item->window.flags & WINDOW_HASFOCUS && item->cvar )
  {
    if( key == K_MOUSE1 || key == K_ENTER || key == K_MOUSE2 || key == K_MOUSE3 )
    {
      DC->setCVar( item->cvar, va( "%i", !DC->getCVarValue( item->cvar ) ) );
      return qtrue;
    }
  }

  return qfalse;

}

int Item_Multi_CountSettings( itemDef_t *item )
{
  if( item->typeData.multi == NULL )
    return 0;

  return item->typeData.multi->count;
}

int Item_Multi_FindCvarByValue( itemDef_t *item )
{
  char buff[1024];
  float value = 0;
  int i;
  multiDef_t *multiPtr = item->typeData.multi;

  if( multiPtr )
  {
    if( multiPtr->strDef )
      DC->getCVarString( item->cvar, buff, sizeof( buff ) );
    else
      value = DC->getCVarValue( item->cvar );

    for( i = 0; i < multiPtr->count; i++ )
    {
      if( multiPtr->strDef )
      {
        if( Q_stricmp( buff, multiPtr->cvarStr[i] ) == 0 )
          return i;
      }
      else
      {
        if( multiPtr->cvarValue[i] == value )
          return i;
      }
    }
  }

  return 0;
}

const char *Item_Multi_Setting( itemDef_t *item )
{
  char buff[1024];
  float value = 0;
  int i;
  multiDef_t *multiPtr = item->typeData.multi;

  if( multiPtr )
  {
    if( multiPtr->strDef )
      DC->getCVarString( item->cvar, buff, sizeof( buff ) );
    else
      value = DC->getCVarValue( item->cvar );

    for( i = 0; i < multiPtr->count; i++ )
    {
      if( multiPtr->strDef )
      {
        if( Q_stricmp( buff, multiPtr->cvarStr[i] ) == 0 )
          return multiPtr->cvarList[i];
      }
      else
      {
        if( multiPtr->cvarValue[i] == value )
          return multiPtr->cvarList[i];
      }
    }
  }

  DC->getCVarString( item->cvar, buff, sizeof( buff ) );
  return va( "Custom (%s)", buff );
}

qboolean Item_Combobox_HandleKey( itemDef_t *item, int key )
{
  comboBoxDef_t *comboPtr = item->typeData.combo;
  qboolean mouseOver = Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory );
  int count = DC->feederCount( item->feederID );

  if( comboPtr )
  {
    if( item->window.flags & WINDOW_HASFOCUS )
    {
      if( ( mouseOver && key == K_MOUSE1 ) ||
          key == K_ENTER || key == K_RIGHTARROW || key == K_DOWNARROW )
      {
        if( count > 0 )
          comboPtr->cursorPos = ( comboPtr->cursorPos + 1 ) % count;

        DC->feederSelection( item->feederID, comboPtr->cursorPos );

        return qtrue;
      }
      else if( ( mouseOver && key == K_MOUSE2 ) ||
               key == K_LEFTARROW || key == K_UPARROW )
      {
        if( count > 0 )
          comboPtr->cursorPos = ( count + comboPtr->cursorPos - 1 ) % count;

        DC->feederSelection( item->feederID, comboPtr->cursorPos );

        return qtrue;
      }
    }
  }

  return qfalse;
}

qboolean Item_Multi_HandleKey( itemDef_t *item, int key )
{
  qboolean mouseOver = Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory );
  int max = Item_Multi_CountSettings( item );
  qboolean changed = qfalse;

  if( item->typeData.multi )
  {
    if( item->window.flags & WINDOW_HASFOCUS && item->cvar && max > 0 )
    {
      int current;

      if( ( mouseOver && key == K_MOUSE1 ) ||
          key == K_ENTER || key == K_RIGHTARROW || key == K_DOWNARROW )
      {
        current = ( Item_Multi_FindCvarByValue( item ) + 1 ) % max;
        changed = qtrue;
      }
      else if( ( mouseOver && key == K_MOUSE2 ) ||
               key == K_LEFTARROW || key == K_UPARROW )
      {
        current = ( Item_Multi_FindCvarByValue( item ) + max - 1 ) % max;
        changed = qtrue;
      }

      if( changed )
      {
        if( item->typeData.multi->strDef )
          DC->setCVar( item->cvar, item->typeData.multi->cvarStr[current] );
        else
        {
          float value = item->typeData.multi->cvarValue[current];

          if( ( ( float )( ( int ) value ) ) == value )
            DC->setCVar( item->cvar, va( "%i", ( int ) value ) );
          else
            DC->setCVar( item->cvar, va( "%f", value ) );
        }

        return qtrue;
      }
    }
  }

  return qfalse;
}

#define MIN_FIELD_WIDTH   10
#define EDIT_CURSOR_WIDTH 10

static void Item_TextField_CalcPaintOffset( itemDef_t *item, char *buff )
{
  editFieldDef_t  *editPtr = item->typeData.edit;

  if( item->cursorPos < editPtr->paintOffset )
    editPtr->paintOffset = item->cursorPos;
  else
  {
    // If there is a maximum field width

    if( editPtr->maxFieldWidth > 0 )
    {
      // If the cursor is at the end of the string, maximise the amount of the
      // string that's visible

      if( buff[ item->cursorPos + 1 ] == '\0' )
      {
        while( UI_Text_Width( &buff[ editPtr->paintOffset ], item->textscale, 0 ) <=
               ( editPtr->maxFieldWidth - EDIT_CURSOR_WIDTH ) && editPtr->paintOffset > 0 )
          editPtr->paintOffset--;
      }

      buff[ item->cursorPos + 1 ] = '\0';

      // Shift paintOffset so that the cursor is visible

      while( UI_Text_Width( &buff[ editPtr->paintOffset ], item->textscale, 0 ) >
             ( editPtr->maxFieldWidth - EDIT_CURSOR_WIDTH ) )
        editPtr->paintOffset++;
    }
  }
}

qboolean Item_TextField_HandleKey( itemDef_t *item, int key )
{
  char buff[1024];
  int len;
  itemDef_t *newItem = NULL;
  editFieldDef_t *editPtr = item->typeData.edit;
  qboolean releaseFocus = qtrue;

  if( item->cvar )
  {
    Com_Memset( buff, 0, sizeof( buff ) );
    DC->getCVarString( item->cvar, buff, sizeof( buff ) );
    len = strlen( buff );

    if( len < item->cursorPos )
      item->cursorPos = len;

    if( editPtr->maxChars && len > editPtr->maxChars )
      len = editPtr->maxChars;

    if( key & K_CHAR_FLAG )
    {
      key &= ~K_CHAR_FLAG;

      if( key == 'h' - 'a' + 1 )
      {
        // ctrl-h is backspace

        if( item->cursorPos > 0 )
        {
          memmove( &buff[item->cursorPos - 1], &buff[item->cursorPos], len + 1 - item->cursorPos );
          item->cursorPos--;
        }

        DC->setCVar( item->cvar, buff );
      }
      else if( key == 'c' - 'a' + 1 )
      {
        // ctrl-c clears the field

        item->cursorPos = 0;
        DC->setCVar( item->cvar, "" );
      }
      else if( key == 'a' - 'a' + 1 )
      {
        // ctrl-a is home

        item->cursorPos = 0;
      }
      else if( key == 'e' - 'a' + 1 )
      {
        // ctrl-e is end

        item->cursorPos = strlen( buff ) - 1;
      }
      else if( key == 'w' - 'a' + 1 )
      {
        // ctrl-w deletes the last word

        while ( item->cursorPos )
        {
          if ( buff[ item->cursorPos - 1 ] != ' ' ) {
            buff[ item->cursorPos - 1 ] = 0;
            item->cursorPos--;
          } else {
            item->cursorPos--;
            if ( buff[ item->cursorPos - 1 ] != ' ' )
              break;
          }
        }

        DC->setCVar( item->cvar, buff );
      }
      else if( ( key < 32 && key >= 0 ) || !item->cvar )
      {
        // Ignore any non printable chars
        releaseFocus = qfalse;
        goto exit;
      }
      else if( item->type == ITEM_TYPE_NUMERICFIELD && ( key < '0' || key > '9' ) )
      {
        // Ignore non-numeric characters
        releaseFocus = qfalse;
        goto exit;
      }
      else
      {
        if( !DC->getOverstrikeMode() )
        {
          if( ( len == MAX_EDITFIELD - 1 ) || ( editPtr->maxChars && len >= editPtr->maxChars ) )
          {
            // Reached maximum field length
            releaseFocus = qfalse;
            goto exit;
          }

          memmove( &buff[item->cursorPos + 1], &buff[item->cursorPos], len + 1 - item->cursorPos );
        }
        else
        {
          // Reached maximum field length
          if( editPtr->maxChars && item->cursorPos >= editPtr->maxChars )
          {
            releaseFocus = qfalse;
            goto exit;
          }
        }

        buff[ item->cursorPos ] = key;

        DC->setCVar( item->cvar, buff );

        if( item->cursorPos < len + 1 )
          item->cursorPos++;
      }
    }
    else
    {
      switch( key )
      {
        case K_DEL:
        case K_KP_DEL:
          if( item->cursorPos < len )
          {
            memmove( buff + item->cursorPos, buff + item->cursorPos + 1, len - item->cursorPos );
            DC->setCVar( item->cvar, buff );
          }

          break;

        case K_RIGHTARROW:
        case K_KP_RIGHTARROW:
          if( item->cursorPos < len )
            item->cursorPos++;

          break;

        case K_LEFTARROW:
        case K_KP_LEFTARROW:
          if( item->cursorPos > 0 )
            item->cursorPos--;

          break;

        case K_HOME:
        case K_KP_HOME:
          item->cursorPos = 0;

          break;

        case K_END:
        case K_KP_END:
          item->cursorPos = len;

          break;

        case K_INS:
        case K_KP_INS:
          DC->setOverstrikeMode( !DC->getOverstrikeMode() );

          break;

        case K_TAB:
        case K_DOWNARROW:
        case K_KP_DOWNARROW:
        case K_UPARROW:
        case K_KP_UPARROW:
          // Ignore these keys from the say field
          if( item->type == ITEM_TYPE_SAYFIELD )
            break;

          newItem = Menu_SetNextCursorItem( item->parent );

          if( newItem && Item_IsEditField( newItem ) )
          {
            g_editItem = newItem;
          }
          else
          {
            releaseFocus = qtrue;
            goto exit;
          }

          break;

        case K_MOUSE1:
        case K_MOUSE2:
        case K_MOUSE3:
        case K_MOUSE4:
          // Ignore these buttons from the say field
          if( item->type == ITEM_TYPE_SAYFIELD )
            break;
          // FALLTHROUGH
        case K_ENTER:
        case K_KP_ENTER:
        case K_ESCAPE:
          releaseFocus = qtrue;
          goto exit;

        default:
          break;
      }
    }

    releaseFocus = qfalse;
  }

exit:
  Item_TextField_CalcPaintOffset( item, buff );

  return !releaseFocus;
}

static void Scroll_ListBox_AutoFunc( void *p )
{
  scrollInfo_t *si = ( scrollInfo_t* )p;

  if( DC->realTime > si->nextScrollTime )
  {
    // need to scroll which is done by simulating a click to the item
    // this is done a bit sideways as the autoscroll "knows" that the item is a listbox
    // so it calls it directly
    Item_ListBox_HandleKey( si->item, si->scrollKey, qtrue, qfalse );
    si->nextScrollTime = DC->realTime + si->adjustValue;
  }

  if( DC->realTime > si->nextAdjustTime )
  {
    si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;

    if( si->adjustValue > SCROLL_TIME_FLOOR )
      si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
  }
}

static void Scroll_ListBox_ThumbFunc( void *p )
{
  scrollInfo_t *si = ( scrollInfo_t* )p;
  rectDef_t r;
  int pos, max;

  if( si->item->window.flags & WINDOW_HORIZONTAL )
  {
    if( DC->cursorx == si->xStart )
      return;

    r.x = si->item->window.rect.x + SCROLLBAR_WIDTH + 1;
    r.y = si->item->window.rect.y + si->item->window.rect.h - SCROLLBAR_HEIGHT - 1;
    r.w = si->item->window.rect.w - ( SCROLLBAR_WIDTH * 2 ) - 2;
    r.h = SCROLLBAR_HEIGHT;
    max = Item_ListBox_MaxScroll( si->item );
    //
    pos = ( DC->cursorx - r.x - SCROLLBAR_WIDTH / 2 ) * max / ( r.w - SCROLLBAR_WIDTH );

    if( pos < 0 )
      pos = 0;
    else if( pos > max )
      pos = max;

    si->item->typeData.list->startPos = pos;
    si->xStart = DC->cursorx;
  }
  else if( DC->cursory != si->yStart )
  {
    r.x = si->item->window.rect.x + si->item->window.rect.w - SCROLLBAR_WIDTH - 1;
    r.y = si->item->window.rect.y + SCROLLBAR_HEIGHT + 1;
    r.w = SCROLLBAR_WIDTH;
    r.h = si->item->window.rect.h - ( SCROLLBAR_HEIGHT * 2 ) - 2;
    max = Item_ListBox_MaxScroll( si->item );
    //
    pos = ( DC->cursory - r.y - SCROLLBAR_HEIGHT / 2 ) * max / ( r.h - SCROLLBAR_HEIGHT );

    if( pos < 0 )
      pos = 0;
    else if( pos > max )
      pos = max;

    si->item->typeData.list->startPos = pos;
    si->yStart = DC->cursory;
  }

  if( DC->realTime > si->nextScrollTime )
  {
    // need to scroll which is done by simulating a click to the item
    // this is done a bit sideways as the autoscroll "knows" that the item is a listbox
    // so it calls it directly
    Item_ListBox_HandleKey( si->item, si->scrollKey, qtrue, qfalse );
    si->nextScrollTime = DC->realTime + si->adjustValue;
  }

  if( DC->realTime > si->nextAdjustTime )
  {
    si->nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;

    if( si->adjustValue > SCROLL_TIME_FLOOR )
      si->adjustValue -= SCROLL_TIME_ADJUSTOFFSET;
  }
}

static void Scroll_Slider_ThumbFunc( void *p )
{
  float x, value, cursorx;
  scrollInfo_t *si = ( scrollInfo_t* )p;

  if( si->item->text )
    x = si->item->textRect.x + si->item->textRect.w + ITEM_VALUE_OFFSET;
  else
    x = si->item->window.rect.x;

  cursorx = DC->cursorx;

  if( cursorx < x )
    cursorx = x;
  else if( cursorx > x + SLIDER_WIDTH )
    cursorx = x + SLIDER_WIDTH;

  value = cursorx - x - ( SLIDER_THUMB_WIDTH / 2 );
  value /= SLIDER_WIDTH - SLIDER_THUMB_WIDTH;
  value *= si->item->typeData.edit->maxVal - si->item->typeData.edit->minVal;
  value += si->item->typeData.edit->minVal;
  DC->setCVar( si->item->cvar, va( "%f", value ) );
}

void Item_StartCapture( itemDef_t *item, int key )
{
  int flags;

  // Don't allow captureFunc to be overridden

  if( captureFunc != voidFunction )
    return;

  switch( item->type )
  {
    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_SAYFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_LISTBOX:
    {
      flags = Item_ListBox_OverLB( item, DC->cursorx, DC->cursory );

      if( flags & ( WINDOW_LB_LEFTARROW | WINDOW_LB_RIGHTARROW ) )
      {
        scrollInfo.nextScrollTime = DC->realTime + SCROLL_TIME_START;
        scrollInfo.nextAdjustTime = DC->realTime + SCROLL_TIME_ADJUST;
        scrollInfo.adjustValue = SCROLL_TIME_START;
        scrollInfo.scrollKey = key;
        scrollInfo.scrollDir = ( flags & WINDOW_LB_LEFTARROW ) ? qtrue : qfalse;
        scrollInfo.item = item;
        UI_InstallCaptureFunc( Scroll_ListBox_AutoFunc, &scrollInfo, 0 );
        itemCapture = item;
      }
      else if( flags & WINDOW_LB_THUMB )
      {
        scrollInfo.scrollKey = key;
        scrollInfo.item = item;
        scrollInfo.xStart = DC->cursorx;
        scrollInfo.yStart = DC->cursory;
        UI_InstallCaptureFunc( Scroll_ListBox_ThumbFunc, &scrollInfo, 0 );
        itemCapture = item;
      }

      break;
    }

    case ITEM_TYPE_SLIDER:
    {
      flags = Item_Slider_OverSlider( item, DC->cursorx, DC->cursory );

      if( flags & WINDOW_LB_THUMB )
      {
        scrollInfo.scrollKey = key;
        scrollInfo.item = item;
        scrollInfo.xStart = DC->cursorx;
        scrollInfo.yStart = DC->cursory;
        UI_InstallCaptureFunc( Scroll_Slider_ThumbFunc, &scrollInfo, 0 );
        itemCapture = item;
      }

      break;
    }
  }
}

void Item_StopCapture( itemDef_t *item )
{
}

qboolean Item_Slider_HandleKey( itemDef_t *item, int key, qboolean down )
{
  float x, value, width;

  if( item->window.flags & WINDOW_HASFOCUS && item->cvar &&
      Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) )
  {
    if( item->typeData.edit && ( key == K_ENTER ||
        key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 ) )
    {
      rectDef_t testRect;
      width = SLIDER_WIDTH;

      if( item->text )
        x = item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET;
      else
        x = item->window.rect.x;

      testRect = item->window.rect;
      value = ( float )SLIDER_THUMB_WIDTH / 2;
      testRect.x = x - value;
      testRect.w = SLIDER_WIDTH + value;

      if( Rect_ContainsPoint( &testRect, DC->cursorx, DC->cursory ) )
      {
        value = ( float )( DC->cursorx - x ) / width;
        value *= ( item->typeData.edit->maxVal - item->typeData.edit->minVal );
        value += item->typeData.edit->minVal;
        DC->setCVar( item->cvar, va( "%f", value ) );
        return qtrue;
      }
    }
  }

  return qfalse;
}


qboolean Item_HandleKey( itemDef_t *item, int key, qboolean down )
{
  if( itemCapture )
  {
    Item_StopCapture( itemCapture );
    itemCapture = NULL;
    UI_RemoveCaptureFunc( );
  }
  else
  {
    if( down && ( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 ) )
      Item_StartCapture( item, key );
  }

  if( !down )
    return qfalse;

  // Edit fields are handled specially
  if( Item_IsEditField( item ) )
    return qfalse;

  switch( item->type )
  {
    case ITEM_TYPE_BUTTON:
      return qfalse;

    case ITEM_TYPE_RADIOBUTTON:
      return qfalse;

    case ITEM_TYPE_CHECKBOX:
      return qfalse;

    case ITEM_TYPE_COMBO:
      return Item_Combobox_HandleKey( item, key );

    case ITEM_TYPE_LISTBOX:
      return Item_ListBox_HandleKey( item, key, down, qfalse );

    case ITEM_TYPE_YESNO:
      return Item_YesNo_HandleKey( item, key );

    case ITEM_TYPE_MULTI:
      return Item_Multi_HandleKey( item, key );

    case ITEM_TYPE_OWNERDRAW:
      return Item_OwnerDraw_HandleKey( item, key );

    case ITEM_TYPE_BIND:
      return Item_Bind_HandleKey( item, key, down );

    case ITEM_TYPE_SLIDER:
      return Item_Slider_HandleKey( item, key, down );

    default:
      return qfalse;
  }
}

void Item_Action( itemDef_t *item )
{
  if( item )
    Item_RunScript( item, item->action );
}

itemDef_t *Menu_SetPrevCursorItem( menuDef_t *menu )
{
  qboolean wrapped = qfalse;
  int oldCursor = menu->cursorItem;

  if( menu->cursorItem < 0 )
  {
    menu->cursorItem = menu->itemCount - 1;
    wrapped = qtrue;
  }

  while( menu->cursorItem > -1 )
  {
    menu->cursorItem--;

    if( menu->cursorItem < 0 && !wrapped )
    {
      wrapped = qtrue;
      menu->cursorItem = menu->itemCount - 1;
    }

    if( Item_SetFocus( menu->items[menu->cursorItem], DC->cursorx, DC->cursory ) )
    {
      Menu_HandleMouseMove( menu, menu->items[menu->cursorItem]->window.rect.x + 1,
                            menu->items[menu->cursorItem]->window.rect.y + 1 );
      return menu->items[menu->cursorItem];
    }
  }

  menu->cursorItem = oldCursor;
  return NULL;

}

itemDef_t *Menu_SetNextCursorItem( menuDef_t *menu )
{
  qboolean wrapped = qfalse;
  int oldCursor = menu->cursorItem;


  if( menu->cursorItem == -1 )
  {
    menu->cursorItem = 0;
    wrapped = qtrue;
  }

  while( menu->cursorItem < menu->itemCount )
  {
    menu->cursorItem++;

    if( menu->cursorItem >= menu->itemCount && !wrapped )
    {
      wrapped = qtrue;
      menu->cursorItem = 0;
    }

    if( Item_SetFocus( menu->items[menu->cursorItem], DC->cursorx, DC->cursory ) )
    {
      Menu_HandleMouseMove( menu, menu->items[menu->cursorItem]->window.rect.x + 1,
                            menu->items[menu->cursorItem]->window.rect.y + 1 );
      return menu->items[menu->cursorItem];
    }

  }

  menu->cursorItem = oldCursor;
  return NULL;
}

static void Window_CloseCinematic( windowDef_t *window )
{
  if( window->style == WINDOW_STYLE_CINEMATIC && window->cinematic >= 0 )
  {
    DC->stopCinematic( window->cinematic );
    window->cinematic = -1;
  }
}

static void Menu_CloseCinematics( menuDef_t *menu )
{
  if( menu )
  {
    int i;
    Window_CloseCinematic( &menu->window );

    for( i = 0; i < menu->itemCount; i++ )
    {
      Window_CloseCinematic( &menu->items[i]->window );

      if( menu->items[i]->type == ITEM_TYPE_OWNERDRAW )
        DC->stopCinematic( 0 - menu->items[i]->window.ownerDraw );
    }
  }
}

static void Display_CloseCinematics( void )
{
  int i;

  for( i = 0; i < menuCount; i++ )
    Menu_CloseCinematics( &Menus[i] );
}

void  Menus_Activate( menuDef_t *menu )
{
  int i;
  qboolean onTopOfMenuStack = qfalse;

  if( openMenuCount > 0 && menuStack[ openMenuCount - 1 ] == menu )
    onTopOfMenuStack = qtrue;

  menu->window.flags |= ( WINDOW_HASFOCUS | WINDOW_VISIBLE );

  // If being opened for the first time
  if( !onTopOfMenuStack )
  {
    if( menu->onOpen )
    {
      itemDef_t item;
      item.parent = menu;
      Item_RunScript( &item, menu->onOpen );
    }

    if( menu->soundName && *menu->soundName )
      DC->startBackgroundTrack( menu->soundName, menu->soundName );

    Display_CloseCinematics( );

    Menu_HandleMouseMove( menu, DC->cursorx, DC->cursory ); // force the item under the cursor to focus

    for( i = 0; i < menu->itemCount; i++ ) // reset selection in listboxes when opened
    {
      if( menu->items[ i ]->type == ITEM_TYPE_LISTBOX )
      {
        menu->items[ i ]->cursorPos = 0;
        menu->items[ i ]->typeData.list->startPos = 0;
        DC->feederSelection( menu->items[ i ]->feederID, 0 );
      }
      else if( menu->items[ i ]->type == ITEM_TYPE_COMBO )
      {
        menu->items[ i ]->typeData.combo->cursorPos =
          DC->feederInitialise( menu->items[ i ]->feederID );
      }

    }

    if( openMenuCount < MAX_OPEN_MENUS )
      menuStack[ openMenuCount++ ] = menu;
  }
}

qboolean Menus_ReplaceActive( menuDef_t *menu )
{
  int i;
  menuDef_t *active;

  if( openMenuCount < 1 )
    return qfalse;

  active = menuStack[ openMenuCount - 1]; 

  if( !( active->window.flags & WINDOW_HASFOCUS )  ||
     !( active->window.flags & WINDOW_VISIBLE ) )
  {
    return qfalse;
  }

  if( menu == active )
    return qfalse;

  if( menu->itemCount != active->itemCount )
    return qfalse;
  
  for( i = 0; i < menu->itemCount; i++ ) 
  {
    if( menu->items[ i ]->type != active->items[ i ]->type )
      return qfalse;
  }

  active->window.flags &= ~( WINDOW_FADINGOUT | WINDOW_VISIBLE );
  menu->window.flags |= ( WINDOW_HASFOCUS | WINDOW_VISIBLE );

  menuStack[ openMenuCount - 1 ] = menu;
  if( menu->onOpen )
  {
    itemDef_t item;
    item.parent = menu;
    Item_RunScript( &item, menu->onOpen );
  }
  
  for( i = 0; i < menu->itemCount; i++ ) 
  {
      menu->items[ i ]->cursorPos = active->items[ i ]->cursorPos;
      menu->items[ i ]->typeData.list->startPos = active->items[ i ]->typeData.list->startPos;
      menu->items[ i ]->feederID = active->items[ i ]->feederID;
      menu->items[ i ]->typeData.combo->cursorPos = active->items[ i ]->typeData.combo->cursorPos;
  }
  return qtrue;
}

int Display_VisibleMenuCount( void )
{
  int i, count;
  count = 0;

  for( i = 0; i < menuCount; i++ )
  {
    if( Menus[i].window.flags & ( WINDOW_FORCED | WINDOW_VISIBLE ) )
      count++;
  }

  return count;
}

void Menus_HandleOOBClick( menuDef_t *menu, int key, qboolean down )
{
  if( menu )
  {
    int i;
    // basically the behaviour we are looking for is if there are windows in the stack.. see if
    // the cursor is within any of them.. if not close them otherwise activate them and pass the
    // key on.. force a mouse move to activate focus and script stuff

    if( down && menu->window.flags & WINDOW_OOB_CLICK )
      Menus_Close( menu );

    for( i = 0; i < menuCount; i++ )
    {
      if( Menu_OverActiveItem( &Menus[i], DC->cursorx, DC->cursory ) )
      {
        Menus_Close( menu );
        Menus_Activate( &Menus[i] );
        Menu_HandleMouseMove( &Menus[i], DC->cursorx, DC->cursory );
        Menu_HandleKey( &Menus[i], key, down );
      }
    }

    if( Display_VisibleMenuCount() == 0 )
    {
      if( DC->Pause )
        DC->Pause( qfalse );
    }

    Display_CloseCinematics();
  }
}

static rectDef_t *Item_CorrectedTextRect( itemDef_t *item )
{
  static rectDef_t rect;
  memset( &rect, 0, sizeof( rectDef_t ) );

  if( item )
  {
    rect = item->textRect;

    if( rect.w )
      rect.y -= rect.h;
  }

  return &rect;
}

void Menu_HandleKey( menuDef_t *menu, int key, qboolean down )
{
  int i;
  itemDef_t *item = NULL;

  // KTW: Draggable Windows
  if (key == K_MOUSE1 && down && Rect_ContainsPoint(&menu->window.rect, DC->cursorx, DC->cursory) &&
        menu->window.style && menu->window.border) 
      menu->window.flags |= WINDOW_DRAG;
  else
      menu->window.flags &= ~WINDOW_DRAG;

  if( g_waitingForKey && down )
  {
    Item_Bind_HandleKey( g_bindItem, key, down );
    return;
  }

  if( g_editingField && down )
  {
    if( !Item_TextField_HandleKey( g_editItem, key ) )
    {
      g_editingField = qfalse;
      Item_RunScript( g_editItem, g_editItem->onTextEntry );
      g_editItem = NULL;
      return;
    }
    else
    {
      Item_RunScript( g_editItem, g_editItem->onCharEntry );
    }
  }

  if( menu == NULL )
    return;

  // see if the mouse is within the window bounds and if so is this a mouse click
  if( down && !( menu->window.flags & WINDOW_POPUP ) &&
      !Rect_ContainsPoint( &menu->window.rect, DC->cursorx, DC->cursory ) )
  {
    static qboolean inHandleKey = qfalse;

    if( !inHandleKey && ( key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3 ) )
    {
      inHandleKey = qtrue;
      Menus_HandleOOBClick( menu, key, down );
      inHandleKey = qfalse;
      return;
    }
  }

  // get the item with focus
  for( i = 0; i < menu->itemCount; i++ )
  {
    if( menu->items[i]->window.flags & WINDOW_HASFOCUS )
      item = menu->items[i];
  }

  if( item != NULL )
  {
    if( Item_HandleKey( item, key, down ) )
    {
      Item_Action( item );
      return;
    }
  }

  if( !down )
    return;

  // default handling
  switch( key )
  {
    case K_F11:
      DC->executeText( EXEC_APPEND, "screenshotJPEG\n" );
      break;

    case K_KP_UPARROW:
    case K_UPARROW:
      Menu_SetPrevCursorItem( menu );
      break;

    case K_ESCAPE:
      if( !g_waitingForKey && menu->onESC )
      {
        itemDef_t it;
        it.parent = menu;
        Item_RunScript( &it, menu->onESC );
      }

      break;

    case K_TAB:
    case K_KP_DOWNARROW:
    case K_DOWNARROW:
      Menu_SetNextCursorItem( menu );
      break;

    case K_MOUSE1:
    case K_MOUSE2:
      if( item )
      {
        if( item->type == ITEM_TYPE_TEXT )
        {
          if( Rect_ContainsPoint( Item_CorrectedTextRect( item ), DC->cursorx, DC->cursory ) )
            Item_Action( item );
        }
        else if( Item_IsEditField( item ) )
        {
          if( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) )
          {
            char buffer[ MAX_STRING_CHARS ] = { 0 };

            if( item->cvar )
              DC->getCVarString( item->cvar, buffer, sizeof( buffer ) );

            item->cursorPos = strlen( buffer );

            Item_TextField_CalcPaintOffset( item, buffer );

            g_editingField = qtrue;

            g_editItem = item;
          }
        }
        else
        {
          if( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) )
            Item_Action( item );
        }
      }

      break;

    case K_JOY1:
    case K_JOY2:
    case K_JOY3:
    case K_JOY4:
    case K_AUX1:
    case K_AUX2:
    case K_AUX3:
    case K_AUX4:
    case K_AUX5:
    case K_AUX6:
    case K_AUX7:
    case K_AUX8:
    case K_AUX9:
    case K_AUX10:
    case K_AUX11:
    case K_AUX12:
    case K_AUX13:
    case K_AUX14:
    case K_AUX15:
    case K_AUX16:
      break;

    case K_KP_ENTER:
    case K_ENTER:
      if( item )
      {
        if( Item_IsEditField( item ) )
        {
          char buffer[ MAX_STRING_CHARS ] = { 0 };

          if( item->cvar )
            DC->getCVarString( item->cvar, buffer, sizeof( buffer ) );

          item->cursorPos = strlen( buffer );

          Item_TextField_CalcPaintOffset( item, buffer );

          g_editingField = qtrue;

          g_editItem = item;
        }
        else
          Item_Action( item );
      }

      break;
  }
}

void ToWindowCoords( float *x, float *y, windowDef_t *window )
{
  if( window->border != 0 )
  {
    *x += window->borderSize;
    *y += window->borderSize;
  }

  *x += window->rect.x;
  *y += window->rect.y;
}

void Rect_ToWindowCoords( rectDef_t *rect, windowDef_t *window )
{
  ToWindowCoords( &rect->x, &rect->y, window );
}

void Item_SetTextExtents( itemDef_t *item, int *width, int *height, const char *text )
{
  const char *textPtr = ( text ) ? text : item->text;
  qboolean cvarContent;

  // it's hard to make a policy on what should be aligned statically and what
  // should be aligned dynamically; there are reasonable cases for both. If
  // it continues to be a problem then there should probably be an item keyword
  // for it; but for the moment only adjusting the alignment of ITEM_TYPE_TEXT
  // seems to suffice.
  cvarContent = ( item->cvar && item->textalignment != ALIGN_LEFT &&
                  item->type == ITEM_TYPE_TEXT );

  if( textPtr == NULL )
    return;

  *width = item->textRect.w;
  *height = item->textRect.h;

  // as long as the item isn't dynamic content (ownerdraw or cvar), this
  // keeps us from computing the widths and heights more than once
  if( *width == 0 || cvarContent || ( item->type == ITEM_TYPE_OWNERDRAW &&
      item->textalignment != ALIGN_LEFT ) || ( ((menuDef_t*)item->parent)->window.flags & WINDOW_DRAG ) )
  {
    int originalWidth;

    if( cvarContent )
    {
      char buff[ MAX_CVAR_VALUE_STRING ];
      DC->getCVarString( item->cvar, buff, sizeof( buff ) );
      originalWidth = UI_Text_Width( item->text, item->textscale, 0 ) +
                      UI_Text_Width( buff, item->textscale, 0 );
    }
    else
      originalWidth = UI_Text_Width( item->text, item->textscale, 0 );

    *width = UI_Text_Width( textPtr, item->textscale, 0 );
    *height = UI_Text_Height( textPtr, item->textscale, 0 );
    item->textRect.w = *width;
    item->textRect.h = *height;

    if( item->textvalignment == VALIGN_BOTTOM )
      item->textRect.y = item->textaligny + item->window.rect.h;
    else if( item->textvalignment == VALIGN_CENTER )
      item->textRect.y = item->textaligny + ( ( *height + item->window.rect.h ) / 2.0f );
    else if( item->textvalignment == VALIGN_TOP )
      item->textRect.y = item->textaligny + *height;

    if( item->textalignment == ALIGN_LEFT )
      item->textRect.x = item->textalignx;
    else if( item->textalignment == ALIGN_CENTER )
      item->textRect.x = item->textalignx + ( ( item->window.rect.w - originalWidth ) / 2.0f );
    else if( item->textalignment == ALIGN_RIGHT )
      item->textRect.x = item->textalignx + item->window.rect.w - originalWidth;

    ToWindowCoords( &item->textRect.x, &item->textRect.y, &item->window );
  }
}

void Item_TextColor( itemDef_t *item, vec4_t *newColor )
{
  vec4_t lowLight;
  menuDef_t *parent = ( menuDef_t* )item->parent;

  Fade( &item->window.flags, &item->window.foreColor[3], parent->fadeClamp,
        &item->window.nextTime, parent->fadeCycle, qtrue, parent->fadeAmount );

  if( item->window.flags & WINDOW_HASFOCUS )
    memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
  else if( item->textStyle == ITEM_TEXTSTYLE_BLINK && !( ( DC->realTime / BLINK_DIVISOR ) & 1 ) )
  {
    lowLight[0] = 0.8 * item->window.foreColor[0];
    lowLight[1] = 0.8 * item->window.foreColor[1];
    lowLight[2] = 0.8 * item->window.foreColor[2];
    lowLight[3] = 0.8 * item->window.foreColor[3];
    LerpColor( item->window.foreColor, lowLight, *newColor, 0.5 + 0.5 * sin( DC->realTime / PULSE_DIVISOR ) );
  }
  else
  {
    memcpy( newColor, &item->window.foreColor, sizeof( vec4_t ) );
    // items can be enabled and disabled based on cvars
  }

  if( item->enableCvar != NULL && *item->enableCvar && item->cvarTest != NULL && *item->cvarTest )
  {
    if( item->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) && !Item_EnableShowViaCvar( item, CVAR_ENABLE ) )
      memcpy( newColor, &parent->disableColor, sizeof( vec4_t ) );
  }
}

static void SkipColorCodes( const char **text, char *lastColor )
{
  while( Q_IsColorString( *text ) )
  {
    lastColor[ 0 ] = (*text)[ 0 ];
    lastColor[ 1 ] = (*text)[ 1 ];
    (*text) += 2;
  }
}

static void SkipWhiteSpace( const char **text, char *lastColor )
{
  while( **text )
  {
    SkipColorCodes( text, lastColor );

    if( **text != '\n' && isspace( **text ) )
      (*text)++;
    else
      break;
  }
}

static void SkipEmoticons( const char **text )
{
  int      emoticonLen;
  qboolean emoticonEscaped;

  while( UI_Text_IsEmoticon( *text, &emoticonEscaped, &emoticonLen, NULL, NULL ) )
  {
    if( emoticonEscaped )
      (*text)++;
    else
      (*text) += emoticonLen;
  }
}

static const char *Item_Text_Wrap( const char *text, float scale, float width )
{
  static char   out[ 8192 ] = "";
  char          *paint = out;
  char          c[ 3 ] = "^7";
  const char    *p = text;
  const char    *eol;
  const char    *q = NULL, *qMinus1 = NULL;
  unsigned int  testLength;
  unsigned int  i;

  if( strlen( text ) >= sizeof( out ) )
    return NULL;

  *paint = '\0';

  while( *p )
  {
    testLength = 1;
    eol = p;
    q = p + 1;

    SkipColorCodes( &q, c );

    while( UI_Text_Width( p, scale, testLength ) < width )
    {
      // Remaining string is too short to wrap
      if( testLength >= strlen( p ) )
      {
        eol = p + strlen( p );
        break;
      }

      // Point q at the end of the current testLength
      for( q = p, i = 0; i < testLength; i++ )
      {
        SkipColorCodes( &q, c );
        SkipEmoticons( &q );

        qMinus1 = q;
        q++;
      }

      // Some color escapes might still be present
      SkipColorCodes( &q, c );
      SkipEmoticons( &q );

      // Manual line break
      if( *q == '\n' )
      {
        eol = q + 1;
        break;
      }

      if( !isspace( *qMinus1 ) && isspace( *q ) )
        eol = q;

      testLength++;
    }

    // No split has taken place, so just split mid-word
    if( eol == p )
      eol = q;

    paint = out + strlen( out );

    // Copy text
    strncpy( paint, p, eol - p );

    paint[ eol - p ] = '\0';
    p = eol;

    if( out[ strlen( out ) - 1 ] == '\n' )
    {
      // The line is deliberately broken, clear the color
      c[ 0 ] = '\0';
    }
    else
    {
      // Add a \n if it's not there already
      Q_strcat( out, sizeof( out ), "\n" );

      // Skip leading whitespace on next line and save the
      // last color code
      SkipWhiteSpace( &p, c );
    }

    Q_strcat( out, sizeof( out ), c );

    paint = out + strlen( out );
  }

  return out;
}

#define MAX_WRAP_CACHE  16
#define MAX_WRAP_LINES  128
#define MAX_WRAP_TEXT   512

typedef struct
{
  char      text[ MAX_WRAP_TEXT * MAX_WRAP_LINES ]; //FIXME: augment with hash?
  rectDef_t rect;
  float     scale;
  char      lines[ MAX_WRAP_LINES ][ MAX_WRAP_TEXT ];
  float     lineCoords[ MAX_WRAP_LINES ][ 2 ];
  int       numLines;
}

wrapCache_t;

static wrapCache_t  wrapCache[ MAX_WRAP_CACHE ];
static int          cacheWriteIndex   = 0;
static int          cacheReadIndex    = 0;
static int          cacheReadLineNum  = 0;

static void UI_CreateCacheEntry( const char *text, rectDef_t *rect, float scale )
{
  wrapCache_t *cacheEntry = &wrapCache[ cacheWriteIndex ];

  Q_strncpyz( cacheEntry->text, text, sizeof( cacheEntry->text ) );
  cacheEntry->rect.x    = rect->x;
  cacheEntry->rect.y    = rect->y;
  cacheEntry->rect.w    = rect->w;
  cacheEntry->rect.h    = rect->h;
  cacheEntry->scale     = scale;
  cacheEntry->numLines  = 0;
}

static void UI_AddCacheEntryLine( const char *text, float x, float y )
{
  wrapCache_t *cacheEntry = &wrapCache[ cacheWriteIndex ];

  if( cacheEntry->numLines >= MAX_WRAP_LINES )
    return;

  Q_strncpyz( cacheEntry->lines[ cacheEntry->numLines ], text,
              sizeof( cacheEntry->lines[ 0 ] ) );

  cacheEntry->lineCoords[ cacheEntry->numLines ][ 0 ] = x - cacheEntry->rect.x;

  cacheEntry->lineCoords[ cacheEntry->numLines ][ 1 ] = y - cacheEntry->rect.y;

  cacheEntry->numLines++;
}

static void UI_FinishCacheEntry( void )
{
  cacheWriteIndex = ( cacheWriteIndex + 1 ) % MAX_WRAP_CACHE;
}

static qboolean UI_CheckWrapCache( const char *text, rectDef_t *rect, float scale )
{
  int i;

  for( i = 0; i < MAX_WRAP_CACHE; i++ )
  {
    wrapCache_t *cacheEntry = &wrapCache[ i ];

    if( Q_stricmp( text, cacheEntry->text ) )
      continue;

    if( rect->w != cacheEntry->rect.w ||
        rect->h != cacheEntry->rect.h )
      continue;

    if( cacheEntry->scale != scale )
      continue;

    // This is a match
    cacheEntry->rect.x = rect->x;
    cacheEntry->rect.y = rect->y;

    cacheReadIndex = i;

    cacheReadLineNum = 0;

    return qtrue;
  }

  // No match - wrap isn't cached
  return qfalse;
}

static qboolean UI_NextWrapLine( const char **text, float *x, float *y )
{
  wrapCache_t *cacheEntry = &wrapCache[ cacheReadIndex ];

  if( cacheReadLineNum >= cacheEntry->numLines )
    return qfalse;

  *text = cacheEntry->lines[ cacheReadLineNum ];

  *x    = cacheEntry->lineCoords[ cacheReadLineNum ][ 0 ] + cacheEntry->rect.x;

  *y    = cacheEntry->lineCoords[ cacheReadLineNum ][ 1 ] + cacheEntry->rect.y;

  cacheReadLineNum++;

  return qtrue;
}

void Item_Text_Wrapped_Paint( itemDef_t *item )
{
  char        text[ 1024 ];
  const char  *p, *textPtr;
  float       x, y, w, h;
  vec4_t      color;

  if( item->text == NULL )
  {
    if( item->cvar == NULL )
      return;
    else
    {
      DC->getCVarString( item->cvar, text, sizeof( text ) );
      textPtr = text;
    }
  }
  else
    textPtr = item->text;

  if( *textPtr == '\0' )
    return;

  Item_TextColor( item, &color );

  // Check if this block is cached
  if( ( qboolean )DC->getCVarValue( "ui_textWrapCache" ) &&
      UI_CheckWrapCache( textPtr, &item->window.rect, item->textscale ) )
  {
    while( UI_NextWrapLine( &p, &x, &y ) )
    {
      UI_Text_Paint( x, y, item->textscale, color,
                     p, 0, 0, item->textStyle );
    }
  }
  else
  {
    char        buff[ 4096 ];
    float       fontHeight    = UI_Text_EmHeight( item->textscale );
    const float lineSpacing   = fontHeight * 0.4f;
    float       lineHeight    = fontHeight + lineSpacing;
    float       textHeight;
    int         textLength;
    int         paintLines, totalLines, lineNum = 0;
    float       paintY;
    int         i;

    UI_CreateCacheEntry( textPtr, &item->window.rect, item->textscale );

    x = item->window.rect.x + item->textalignx;
    y = item->window.rect.y + item->textaligny;
    w = item->window.rect.w - ( 2.0f * item->textalignx );
    h = item->window.rect.h - ( 2.0f * item->textaligny );

    textPtr = Item_Text_Wrap( textPtr, item->textscale, w );
    textLength = strlen( textPtr );

    // Count lines
    totalLines = 0;

    for( i = 0; i < textLength; i++ )
    {
      if( textPtr[ i ] == '\n' )
        totalLines++;
    }

    paintLines = ( int )floor( ( h + lineSpacing ) / lineHeight );

    if( paintLines > totalLines )
      paintLines = totalLines;

    textHeight = ( paintLines * lineHeight ) - lineSpacing;

    switch( item->textvalignment )
    {
      default:

      case VALIGN_BOTTOM:
        paintY = y + ( h - textHeight );
        break;

      case VALIGN_CENTER:
        paintY = y + ( ( h - textHeight ) / 2.0f );
        break;

      case VALIGN_TOP:
        paintY = y;
        break;
    }

    p = textPtr;

    for( i = 0, lineNum = 0; i < textLength && lineNum < paintLines; i++ )
    {
      int lineLength = &textPtr[ i ] - p;

      if( lineLength >= sizeof( buff ) - 1 )
        break;

      if( textPtr[ i ] == '\n' || textPtr[ i ] == '\0' )
      {
        itemDef_t   lineItem;
        int         width, height;

        memset( &lineItem, 0, sizeof( itemDef_t ) );
        strncpy( buff, p, lineLength );
        buff[ lineLength ] = '\0';
        p = &textPtr[ i + 1 ];

        lineItem.type               = ITEM_TYPE_TEXT;
        lineItem.textscale          = item->textscale;
        lineItem.textStyle          = item->textStyle;
        lineItem.text               = buff;
        lineItem.textalignment      = item->textalignment;
        lineItem.textvalignment     = VALIGN_TOP;
        lineItem.textalignx         = 0.0f;
        lineItem.textaligny         = 0.0f;

        lineItem.textRect.w         = 0.0f;
        lineItem.textRect.h         = 0.0f;
        lineItem.window.rect.x      = x;
        lineItem.window.rect.y      = paintY + ( lineNum * lineHeight );
        lineItem.window.rect.w      = w;
        lineItem.window.rect.h      = lineHeight;
        lineItem.window.border      = item->window.border;
        lineItem.window.borderSize  = item->window.borderSize;

        if( DC->getCVarValue( "ui_developer" ) )
        {
          vec4_t color;
          color[ 0 ] = color[ 2 ] = color[ 3 ] = 1.0f;
          color[ 1 ] = 0.0f;
          DC->drawRect( lineItem.window.rect.x, lineItem.window.rect.y,
                        lineItem.window.rect.w, lineItem.window.rect.h, 1, color );
        }

        Item_SetTextExtents( &lineItem, &width, &height, buff );
        UI_Text_Paint( lineItem.textRect.x, lineItem.textRect.y,
                       lineItem.textscale, color, buff, 0, 0,
                       lineItem.textStyle );
        UI_AddCacheEntryLine( buff, lineItem.textRect.x, lineItem.textRect.y );

        lineNum++;
      }
    }

    UI_FinishCacheEntry( );
  }
}

/*
==============
UI_DrawTextBlock
==============
*/
void UI_DrawTextBlock( rectDef_t *rect, float text_x, float text_y, vec4_t color,
                       float scale, int textalign, int textvalign,
                       int textStyle, const char *text )
{
  static menuDef_t  dummyParent;
  static itemDef_t  textItem;

  textItem.text = text;

  textItem.parent = &dummyParent;
  memcpy( textItem.window.foreColor, color, sizeof( vec4_t ) );
  textItem.window.flags = 0;

  textItem.window.rect.x = rect->x;
  textItem.window.rect.y = rect->y;
  textItem.window.rect.w = rect->w;
  textItem.window.rect.h = rect->h;
  textItem.window.border = 0;
  textItem.window.borderSize = 0.0f;
  textItem.textRect.x = 0.0f;
  textItem.textRect.y = 0.0f;
  textItem.textRect.w = 0.0f;
  textItem.textRect.h = 0.0f;
  textItem.textalignment = textalign;
  textItem.textvalignment = textvalign;
  textItem.textalignx = text_x;
  textItem.textaligny = text_y;
  textItem.textscale = scale;
  textItem.textStyle = textStyle;

  // Utilise existing wrap code
  Item_Text_Wrapped_Paint( &textItem );
}

void Item_Text_Paint( itemDef_t *item )
{
  char text[1024];
  const char *textPtr;
  int height, width;
  vec4_t color;

  if( item->window.flags & WINDOW_WRAPPED )
  {
    Item_Text_Wrapped_Paint( item );
    return;
  }

  if( item->text == NULL )
  {
    if( item->cvar == NULL )
      return;
    else
    {
      DC->getCVarString( item->cvar, text, sizeof( text ) );
      textPtr = text;
    }
  }
  else
    textPtr = item->text;

  // this needs to go here as it sets extents for cvar types as well
  Item_SetTextExtents( item, &width, &height, textPtr );

  if( *textPtr == '\0' )
    return;


  Item_TextColor( item, &color );

  UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, color, textPtr, 0, 0, item->textStyle );
}



void Item_TextField_Paint( itemDef_t *item )
{
  char            buff[1024];
  vec4_t          newColor;
  int             offset = ( item->text && *item->text ) ? ITEM_VALUE_OFFSET : 0;
  menuDef_t       *parent = ( menuDef_t* )item->parent;
  editFieldDef_t  *editPtr = item->typeData.edit;
  char            cursor = DC->getOverstrikeMode() ? '|' : '_';
  qboolean        editing = ( item->window.flags & WINDOW_HASFOCUS && g_editingField );
  const int       cursorWidth = editing ? EDIT_CURSOR_WIDTH : 0;

  //FIXME: causes duplicate printing if item->text is not set (NULL)
  Item_Text_Paint( item );

  buff[0] = '\0';

  if( item->cvar )
    DC->getCVarString( item->cvar, buff, sizeof( buff ) );

  // maxFieldWidth hasn't been set, so use the item's rect
  if( editPtr->maxFieldWidth == 0 )
  {
    editPtr->maxFieldWidth = item->window.rect.w -
                             ( item->textRect.w + offset + ( item->textRect.x - item->window.rect.x ) );

    if( editPtr->maxFieldWidth < MIN_FIELD_WIDTH )
      editPtr->maxFieldWidth = MIN_FIELD_WIDTH;
  }

  if( !editing )
    editPtr->paintOffset = 0;

  // Shorten string to max viewable
  while( UI_Text_Width( buff + editPtr->paintOffset, item->textscale, 0 ) >
         ( editPtr->maxFieldWidth - cursorWidth ) && strlen( buff ) > 0 )
    buff[ strlen( buff ) - 1 ] = '\0';

  parent = ( menuDef_t* )item->parent;

  if( item->window.flags & WINDOW_HASFOCUS )
    memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
  else
    memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );

  if( editing )
  {
    UI_Text_PaintWithCursor( item->textRect.x + item->textRect.w + offset,
                             item->textRect.y, item->textscale, newColor,
                             buff + editPtr->paintOffset,
                             item->cursorPos - editPtr->paintOffset,
                             cursor, editPtr->maxPaintChars, item->textStyle );
  }
  else
  {
    UI_Text_Paint( item->textRect.x + item->textRect.w + offset,
                   item->textRect.y, item->textscale, newColor,
                   buff + editPtr->paintOffset, 0,
                   editPtr->maxPaintChars, item->textStyle );
  }

}

void Item_YesNo_Paint( itemDef_t *item )
{
  vec4_t newColor;
  float value;
  int offset;
  menuDef_t *parent = ( menuDef_t* )item->parent;

  value = ( item->cvar ) ? DC->getCVarValue( item->cvar ) : 0;

  if( item->window.flags & WINDOW_HASFOCUS )
    memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
  else
    memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );

  offset = ( item->text && *item->text ) ? ITEM_VALUE_OFFSET : 0;

  if( item->text )
  {
    Item_Text_Paint( item );
    UI_Text_Paint( item->textRect.x + item->textRect.w + offset, item->textRect.y, item->textscale,
                   newColor, ( value != 0 ) ? "Yes" : "No", 0, 0, item->textStyle );
  }
  else
    UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, newColor, ( value != 0 ) ? "Yes" : "No", 0, 0, item->textStyle );
}

void Item_Multi_Paint( itemDef_t *item )
{
  vec4_t newColor;
  const char *text = "";
  menuDef_t *parent = ( menuDef_t* )item->parent;

  if( item->window.flags & WINDOW_HASFOCUS )
    memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
  else
    memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );

  text = Item_Multi_Setting( item );

  if( item->text )
  {
    Item_Text_Paint( item );
    UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET, item->textRect.y,
                   item->textscale, newColor, text, 0, 0, item->textStyle );
  }
  else
    UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, newColor, text, 0, 0, item->textStyle );
}

void Item_Combobox_Paint( itemDef_t *item )
{
  vec4_t newColor;
  const char *text = "";
  menuDef_t *parent = (menuDef_t *)item->parent;

  if( item->window.flags & WINDOW_HASFOCUS )
    memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
  else
    memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );

  if( item->typeData.combo )
    text = DC->feederItemText( item->feederID, item->typeData.combo->cursorPos,
                               0, NULL );

  if( item->text )
  {
    Item_Text_Paint( item );
    UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET, item->textRect.y,
                   item->textscale, newColor, text, 0, 0, item->textStyle );
  }
  else
    UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, newColor, text, 0, 0, item->textStyle );
}


typedef struct
{
  char  *command;
  int   id;
  int   defaultbind1;
  int   defaultbind2;
  int   bind1;
  int   bind2;
}
bind_t;

typedef struct
{
  char *name;
  float defaultvalue;
  float value;
}
configcvar_t;


static bind_t g_bindings[] =
  {
    { "+scores",      K_TAB,         -1, -1, -1 },
    { "+button2",     K_ENTER,       -1, -1, -1 },
    { "+speed",       K_SHIFT,       -1, -1, -1 },
    { "+button6",     'z',           -1, -1, -1 }, // human dodging
    { "+button8",     'x',           -1, -1, -1 },
    { "+forward",     K_UPARROW,     -1, -1, -1 },
    { "+back",        K_DOWNARROW,   -1, -1, -1 },
    { "+moveleft",    ',',           -1, -1, -1 },
    { "+moveright",   '.',           -1, -1, -1 },
    { "+moveup",      K_SPACE,       -1, -1, -1 },
    { "+movedown",    'c',           -1, -1, -1 },
    { "+left",        K_LEFTARROW,   -1, -1, -1 },
    { "+right",       K_RIGHTARROW,  -1, -1, -1 },
    { "+strafe",      K_ALT,         -1, -1, -1 },
    { "+lookup",      K_PGDN,        -1, -1, -1 },
    { "+lookdown",    K_DEL,         -1, -1, -1 },
    { "+mlook",       '/',           -1, -1, -1 },
    { "centerview",   K_END,         -1, -1, -1 },
    { "+zoom",        -1,            -1, -1, -1 },
    { "weapon 1",     '1',           -1, -1, -1 },
    { "weapon 2",     '2',           -1, -1, -1 },
    { "weapon 3",     '3',           -1, -1, -1 },
    { "weapon 4",     '4',           -1, -1, -1 },
    { "weapon 5",     '5',           -1, -1, -1 },
    { "weapon 6",     '6',           -1, -1, -1 },
    { "weapon 7",     '7',           -1, -1, -1 },
    { "weapon 8",     '8',           -1, -1, -1 },
    { "weapon 9",     '9',           -1, -1, -1 },
    { "weapon 10",    '0',           -1, -1, -1 },
    { "weapon 11",    -1,            -1, -1, -1 },
    { "weapon 12",    -1,            -1, -1, -1 },
    { "weapon 13",    -1,            -1, -1, -1 },
    { "+attack",      K_MOUSE1,      -1, -1, -1 },
    { "+button5",     K_MOUSE2,      -1, -1, -1 }, // secondary attack
    { "reload",       'r',           -1, -1, -1 }, // reload
    { "buy ammo",     'b',           -1, -1, -1 }, // buy ammo
    { "itemact medkit", 'm',         -1, -1, -1 }, // use medkit
    { "+button7",     'q',           -1, -1, -1 }, // buildable use
    { "deconstruct",  'e',           -1, -1, -1 }, // buildable destroy
    { "weapprev",     '[',           -1, -1, -1 },
    { "weapnext",     ']',           -1, -1, -1 },
    { "+button3",     K_MOUSE3,      -1, -1, -1 },
    { "+button4",     K_MOUSE4,      -1, -1, -1 },
    { "vote yes",     K_F1,          -1, -1, -1 },
    { "vote no",      K_F2,          -1, -1, -1 },
    { "teamvote yes", K_F3,          -1, -1, -1 },
    { "teamvote no",  K_F4,          -1, -1, -1 },
    { "scoresUp",      K_KP_PGUP,    -1, -1, -1 },
    { "scoresDown",    K_KP_PGDN,    -1, -1, -1 },
    { "screenshotJPEG",-1,           -1, -1, -1 },
    { "messagemode",  -1,            -1, -1, -1 },
    { "messagemode2", -1,            -1, -1, -1 },
    { "messagemode3", -1,            -1, -1, -1 },
    { "messagemode4", -1,            -1, -1, -1 },
    { "messagemode5", -1,            -1, -1, -1 },
    { "messagemode6", -1,            -1, -1, -1 },
    { "prompt",       -1,            -1, -1, -1 },
    { "squadmark",    'k',           -1, -1, -1 },
  };


static const int g_bindCount = sizeof( g_bindings ) / sizeof( bind_t );

/*
=================
Controls_GetKeyAssignment
=================
*/
static void Controls_GetKeyAssignment ( char *command, int *twokeys )
{
  int   count;
  int   j;
  char  b[256];

  twokeys[0] = twokeys[1] = -1;
  count = 0;

  for( j = 0; j < 256; j++ )
  {
    DC->getBindingBuf( j, b, 256 );

    if( *b == 0 )
      continue;

    if( !Q_stricmp( b, command ) )
    {
      twokeys[count] = j;
      count++;

      if( count == 2 )
        break;
    }
  }
}

/*
=================
Controls_GetConfig
=================
*/
void Controls_GetConfig( void )
{
  int   i;
  int   twokeys[ 2 ];

  // iterate each command, get its numeric binding

  for( i = 0; i < g_bindCount; i++ )
  {
    Controls_GetKeyAssignment( g_bindings[ i ].command, twokeys );

    g_bindings[ i ].bind1 = twokeys[ 0 ];
    g_bindings[ i ].bind2 = twokeys[ 1 ];
  }
}

/*
=================
Controls_SetConfig
=================
*/
void Controls_SetConfig( qboolean restart )
{
  int   i;

  // iterate each command, get its numeric binding

  for( i = 0; i < g_bindCount; i++ )
  {
    if( g_bindings[i].bind1 != -1 )
    {
      DC->setBinding( g_bindings[i].bind1, g_bindings[i].command );

      if( g_bindings[i].bind2 != -1 )
        DC->setBinding( g_bindings[i].bind2, g_bindings[i].command );
    }
  }

  DC->executeText( EXEC_APPEND, "in_restart\n" );
}

/*
=================
Controls_SetDefaults
=================
*/
void Controls_SetDefaults( void )
{
  int i;

  // iterate each command, set its default binding

  for( i = 0; i < g_bindCount; i++ )
  {
    g_bindings[i].bind1 = g_bindings[i].defaultbind1;
    g_bindings[i].bind2 = g_bindings[i].defaultbind2;
  }
}

int BindingIDFromName( const char *name )
{
  int i;

  for( i = 0; i < g_bindCount; i++ )
  {
    if( Q_stricmp( name, g_bindings[i].command ) == 0 )
      return i;
  }

  return -1;
}

char g_nameBind1[32];
char g_nameBind2[32];

void BindingFromName( const char *cvar )
{
  int i, b1, b2;

  // iterate each command, set its default binding

  for( i = 0; i < g_bindCount; i++ )
  {
    if( Q_stricmp( cvar, g_bindings[i].command ) == 0 )
    {
      b1 = g_bindings[i].bind1;

      if( b1 == -1 )
        break;

      DC->keynumToStringBuf( b1, g_nameBind1, 32 );
      Q_strupr( g_nameBind1 );

      b2 = g_bindings[i].bind2;

      if( b2 != -1 )
      {
        DC->keynumToStringBuf( b2, g_nameBind2, 32 );
        Q_strupr( g_nameBind2 );
        strcat( g_nameBind1, " or " );
        strcat( g_nameBind1, g_nameBind2 );
      }

      return;
    }
  }

  strcpy( g_nameBind1, "???" );
}

void Item_Slider_Paint( itemDef_t *item )
{
  vec4_t newColor;
  float x, y, value;
  menuDef_t *parent = ( menuDef_t* )item->parent;
  float vScale = Item_Slider_VScale( item );

  value = ( item->cvar ) ? DC->getCVarValue( item->cvar ) : 0;

  if( item->window.flags & WINDOW_HASFOCUS )
    memcpy( newColor, &parent->focusColor, sizeof( vec4_t ) );
  else
    memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );

  if( item->text )
  {
    Item_Text_Paint( item );
    x = item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET;
    y = item->textRect.y - item->textRect.h +
        ( ( item->textRect.h - ( SLIDER_HEIGHT * vScale ) ) / 2.0f );
  }
  else
  {
    x = item->window.rect.x;
    y = item->window.rect.y;
  }

  DC->setColor( newColor );
  DC->drawHandlePic( x, y, SLIDER_WIDTH, SLIDER_HEIGHT * vScale, DC->Assets.sliderBar );

  y = item->textRect.y - item->textRect.h +
      ( ( item->textRect.h - ( SLIDER_THUMB_HEIGHT * vScale ) ) / 2.0f );

  x = Item_Slider_ThumbPosition( item );
  DC->drawHandlePic( x, y, SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT * vScale, DC->Assets.sliderThumb );
}

void Item_Bind_Paint( itemDef_t *item )
{
  vec4_t newColor, lowLight;
  float value;
  int maxChars = 0;
  menuDef_t *parent = ( menuDef_t* )item->parent;

  if( item->typeData.edit )
    maxChars = item->typeData.edit->maxPaintChars;

  value = ( item->cvar ) ? DC->getCVarValue( item->cvar ) : 0;

  if( item->window.flags & WINDOW_HASFOCUS )
  {
    if( g_bindItem == item )
    {
      lowLight[0] = 0.8f * parent->focusColor[0];
      lowLight[1] = 0.8f * parent->focusColor[1];
      lowLight[2] = 0.8f * parent->focusColor[2];
      lowLight[3] = 0.8f * parent->focusColor[3];

      LerpColor( parent->focusColor, lowLight, newColor,
                 0.5 + 0.5 * sin( DC->realTime / PULSE_DIVISOR ) );
    }
    else
      memcpy( &newColor, &parent->focusColor, sizeof( vec4_t ) );
  }
  else
    memcpy( &newColor, &item->window.foreColor, sizeof( vec4_t ) );

  if( item->text )
  {
    Item_Text_Paint( item );

    if( g_bindItem == item && g_waitingForKey )
    {
      UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET, item->textRect.y,
                     item->textscale, newColor, "Press key", 0, maxChars, item->textStyle );
    }
    else
    {
      BindingFromName( item->cvar );
      UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET, item->textRect.y,
                     item->textscale, newColor, g_nameBind1, 0, maxChars, item->textStyle );
    }
  }
  else
    UI_Text_Paint( item->textRect.x, item->textRect.y, item->textscale, newColor,
                   ( value != 0 ) ? "FIXME" : "FIXME", 0, maxChars, item->textStyle );
}

qboolean Display_KeyBindPending( void )
{
  return g_waitingForKey;
}

qboolean Item_Bind_HandleKey( itemDef_t *item, int key, qboolean down )
{
  int     id;
  int     i;

  if( Rect_ContainsPoint( &item->window.rect, DC->cursorx, DC->cursory ) && !g_waitingForKey )
  {
    if( down && ( key == K_MOUSE1 || key == K_ENTER ) )
    {
      g_waitingForKey = qtrue;
      g_bindItem = item;
    }

    return qtrue;
  }
  else
  {
    if( !g_waitingForKey || g_bindItem == NULL )
      return qtrue;

    if( key & K_CHAR_FLAG )
      return qtrue;

    switch( key )
    {
      case K_ESCAPE:
        g_waitingForKey = qfalse;
        return qtrue;

      case K_BACKSPACE:
        id = BindingIDFromName( item->cvar );

        if( id != -1 )
        {
          g_bindings[id].bind1 = -1;
          g_bindings[id].bind2 = -1;
        }

        Controls_SetConfig( qtrue );
        g_waitingForKey = qfalse;
        g_bindItem = NULL;
        return qtrue;

      case '`':
        return qtrue;
    }
  }

  if( key != -1 )
  {
    for( i = 0; i < g_bindCount; i++ )
    {
      if( g_bindings[i].bind2 == key )
        g_bindings[i].bind2 = -1;

      if( g_bindings[i].bind1 == key )
      {
        g_bindings[i].bind1 = g_bindings[i].bind2;
        g_bindings[i].bind2 = -1;
      }
    }
  }


  id = BindingIDFromName( item->cvar );

  if( id != -1 )
  {
    if( key == -1 )
    {
      if( g_bindings[id].bind1 != -1 )
      {
        DC->setBinding( g_bindings[id].bind1, "" );
        g_bindings[id].bind1 = -1;
      }

      if( g_bindings[id].bind2 != -1 )
      {
        DC->setBinding( g_bindings[id].bind2, "" );
        g_bindings[id].bind2 = -1;
      }
    }
    else if( g_bindings[id].bind1 == -1 )
      g_bindings[id].bind1 = key;
    else if( g_bindings[id].bind1 != key && g_bindings[id].bind2 == -1 )
      g_bindings[id].bind2 = key;
    else
    {
      DC->setBinding( g_bindings[id].bind1, "" );
      DC->setBinding( g_bindings[id].bind2, "" );
      g_bindings[id].bind1 = key;
      g_bindings[id].bind2 = -1;
    }
  }

  Controls_SetConfig( qtrue );
  g_waitingForKey = qfalse;

  return qtrue;
}



void AdjustFrom640( float *x, float *y, float *w, float *h )
{
  //*x = *x * DC->scale + DC->bias;
  *x *= DC->xscale;
  *y *= DC->yscale;
  *w *= DC->xscale;
  *h *= DC->yscale;
}

void Item_Model_Paint( itemDef_t *item )
{
  float x, y, w, h;
  refdef_t refdef;
  refEntity_t   ent;
  vec3_t      mins, maxs, origin;
  vec3_t      angles;
  modelDef_t *modelPtr = item->typeData.model;

  qhandle_t hModel;
  int backLerpWhole;
//  vec3_t axis[3];

  if( modelPtr == NULL )
    return;

  if(!item->asset)
    return;

  hModel = item->asset;


  // setup the refdef
  memset( &refdef, 0, sizeof( refdef ) );

  refdef.rdflags = RDF_NOWORLDMODEL;

  AxisClear( refdef.viewaxis );

  x = item->window.rect.x + 1;
  y = item->window.rect.y + 1;
  w = item->window.rect.w - 2;
  h = item->window.rect.h - 2;

  AdjustFrom640( &x, &y, &w, &h );

  refdef.x = x;
  refdef.y = y;
  refdef.width = w;
  refdef.height = h;

  DC->modelBounds( hModel, mins, maxs );

  origin[2] = -0.5 * ( mins[2] + maxs[2] );
  origin[1] = 0.5 * ( mins[1] + maxs[1] );

  // calculate distance so the model nearly fills the box
  if( qtrue )
  {
    float len = 0.5 * ( maxs[2] - mins[2] );
    origin[0] = len / 0.268;  // len / tan( fov/2 )
    //origin[0] = len / tan(w/2);
  }
  else
    origin[0] = item->textscale;

  refdef.fov_x = ( modelPtr->fov_x ) ? modelPtr->fov_x : w;
  refdef.fov_y = ( modelPtr->fov_y ) ? modelPtr->fov_y : h;

  //refdef.fov_x = (int)((float)refdef.width / 640.0f * 90.0f);
  //xx = refdef.width / tan( refdef.fov_x / 360 * M_PI );
  //refdef.fov_y = atan2( refdef.height, xx );
  //refdef.fov_y *= ( 360 / M_PI );

  DC->clearScene();

  refdef.time = DC->realTime;

  // add the model

  memset( &ent, 0, sizeof( ent ) );

  //adjust = 5.0 * sin( (float)uis.realtime / 500 );
  //adjust = 360 % (int)((float)uis.realtime / 1000);
  //VectorSet( angles, 0, 0, 1 );

  // use item storage to track

  if( modelPtr->rotationSpeed )
  {
    if( DC->realTime > item->window.nextTime )
    {
      item->window.nextTime = DC->realTime + modelPtr->rotationSpeed;
      modelPtr->angle = ( int )( modelPtr->angle + 1 ) % 360;
    }
  }

  if(VectorLengthSquared(modelPtr->axis))
  {
    VectorNormalize(modelPtr->axis);
    angles[0] = AngleNormalize360(modelPtr->axis[0]*modelPtr->angle);
    angles[1] = AngleNormalize360(modelPtr->axis[1]*modelPtr->angle);
    angles[2] = AngleNormalize360(modelPtr->axis[2]*modelPtr->angle);
    AnglesToAxis( angles, ent.axis );
  }
  else
  {
    VectorSet( angles, 0, modelPtr->angle, 0 );
    AnglesToAxis( angles, ent.axis );
  }

  ent.hModel = hModel;


  if(modelPtr->frameTime)	// don't advance on the first frame
    modelPtr->backlerp+=( ((DC->realTime - modelPtr->frameTime)/1000.0f) * (float)modelPtr->fps );

  if(modelPtr->backlerp > 1)
  {
    backLerpWhole = floor(modelPtr->backlerp);

    modelPtr->frame+=(backLerpWhole);
    if((modelPtr->frame - modelPtr->startframe) > modelPtr->numframes)
      modelPtr->frame = modelPtr->startframe + modelPtr->frame % modelPtr->numframes;	// todo: ignoring loopframes

    modelPtr->oldframe+=(backLerpWhole);
    if((modelPtr->oldframe - modelPtr->startframe) > modelPtr->numframes)
      modelPtr->oldframe = modelPtr->startframe + modelPtr->oldframe % modelPtr->numframes;	// todo: ignoring loopframes

    modelPtr->backlerp = modelPtr->backlerp - backLerpWhole;
  }

  modelPtr->frameTime = DC->realTime;

  ent.frame		= modelPtr->frame;
  ent.oldframe	= modelPtr->oldframe;
  ent.backlerp	= 1.0f - modelPtr->backlerp;

  VectorCopy( origin, ent.origin );
  VectorCopy( origin, ent.lightingOrigin );
  ent.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;
  VectorCopy( ent.origin, ent.oldorigin );

  DC->addRefEntityToScene( &ent );

  // add an accent light
  origin[0] -= 100;	// + = behind, - = in front
  origin[1] += 100;	// + = left, - = right
  origin[2] += 100;	// + = above, - = below
  trap_R_AddLightToScene( origin, 500, 1.0, 1.0, 1.0 );

  origin[0] -= 100;
  origin[1] -= 100;
  origin[2] -= 100;
  trap_R_AddLightToScene( origin, 500, 1.0, 0.0, 0.0 );

  DC->renderScene( &refdef );

}


void Item_Image_Paint( itemDef_t *item )
{
  if( item == NULL )
    return;

  DC->drawHandlePic( item->window.rect.x + 1, item->window.rect.y + 1,
                     item->window.rect.w - 2, item->window.rect.h - 2, item->asset );
}

void Item_ListBox_Paint( itemDef_t *item )
{
  float x, y, size, thumb;
  int i, count;
  qhandle_t image;
  qhandle_t optionalImage;
  listBoxDef_t *listPtr = item->typeData.list;
  menuDef_t *menu = ( menuDef_t * )item->parent;
  float one, two;

  if( menu->window.aspectBias != ASPECT_NONE || item->window.aspectBias != ASPECT_NONE )
  {
    one = 1.0f * DC->aspectScale;
    two = 2.0f * DC->aspectScale;
  }
  else
  {
    one = 1.0f;
    two = 2.0f;
  }

  // the listbox is horizontal or vertical and has a fixed size scroll bar going either direction
  // elements are enumerated from the DC and either text or image handles are acquired from the DC as well
  // textscale is used to size the text, textalignx and textaligny are used to size image elements
  // there is no clipping available so only the last completely visible item is painted
  count = DC->feederCount( item->feederID );

  // default is vertical if horizontal flag is not here
  if( item->window.flags & WINDOW_HORIZONTAL )
  {
    //FIXME: unmaintained cruft?

    if( !listPtr->noscrollbar )
    {
      // draw scrollbar in bottom of the window
      // bar
      x = item->window.rect.x + 1;
      y = item->window.rect.y + item->window.rect.h - SCROLLBAR_HEIGHT - 1;
      DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarArrowLeft );
      x += SCROLLBAR_WIDTH - 1;
      size = item->window.rect.w - ( SCROLLBAR_WIDTH * 2 );
      DC->drawHandlePic( x, y, size + 1, SCROLLBAR_HEIGHT, DC->Assets.scrollBar );
      x += size - 1;
      DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarArrowRight );
      // thumb
      thumb = Item_ListBox_ThumbDrawPosition( item );//Item_ListBox_ThumbPosition(item);

      if( thumb > x - SCROLLBAR_WIDTH - 1 )
        thumb = x - SCROLLBAR_WIDTH - 1;

      DC->drawHandlePic( thumb, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarThumb );
      //
      listPtr->endPos = listPtr->startPos;
    }

    size = item->window.rect.w - 2;
    // items
    // size contains max available space

    if( listPtr->elementStyle == LISTBOX_IMAGE )
    {
      // fit = 0;
      x = item->window.rect.x + 1;
      y = item->window.rect.y + 1;

      for( i = listPtr->startPos; i < count; i++ )
      {
        // always draw at least one
        // which may overdraw the box if it is too small for the element
        image = DC->feederItemImage( item->feederID, i );

        if( image )
          DC->drawHandlePic( x + 1, y + 1, listPtr->elementWidth - 2, listPtr->elementHeight - 2, image );

        if( i == item->cursorPos )
        {
          DC->drawRect( x, y, listPtr->elementWidth - 1, listPtr->elementHeight - 1,
                        item->window.borderSize, item->window.borderColor );
        }

        listPtr->endPos++;
        size -= listPtr->elementWidth;

        if( size < listPtr->elementWidth )
        {
          listPtr->drawPadding = size; //listPtr->elementWidth - size;
          break;
        }

        x += listPtr->elementWidth;
        // fit++;
      }
    }
  }
  else
  {
    if( !listPtr->noscrollbar )
    {
      // draw scrollbar to right side of the window
      x = item->window.rect.x + item->window.rect.w - SCROLLBAR_WIDTH - one;
      y = item->window.rect.y + 2;
      DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarArrowUp );
      y += SCROLLBAR_HEIGHT;

      listPtr->endPos = listPtr->startPos;
      size = item->window.rect.h - ( SCROLLBAR_HEIGHT * 2 );
      DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, size -4 , DC->Assets.scrollBar );
      y += size - 4;
      DC->drawHandlePic( x, y, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarArrowDown );
      // thumb
      thumb = Item_ListBox_ThumbDrawPosition( item );//Item_ListBox_ThumbPosition(item);
      thumb += 1;
      if( thumb > y - SCROLLBAR_HEIGHT )
        thumb = y - SCROLLBAR_HEIGHT;

      DC->drawHandlePic( x, thumb, SCROLLBAR_WIDTH, SCROLLBAR_HEIGHT, DC->Assets.scrollBarThumb );
    }

    // adjust size for item painting
    size = item->window.rect.h - 2;

    if( listPtr->elementStyle == LISTBOX_IMAGE )
    {
      // fit = 0;
      x = item->window.rect.x + one;
      y = item->window.rect.y + 1;

      for( i = listPtr->startPos; i < count; i++ )
      {
        // always draw at least one
        // which may overdraw the box if it is too small for the element
        image = DC->feederItemImage( item->feederID, i );

        if( image )
          DC->drawHandlePic( x + one, y + 1, listPtr->elementWidth - two, listPtr->elementHeight - 2, image );

        if( i == item->cursorPos )
        {
          DC->drawRect( x, y, listPtr->elementWidth - one, listPtr->elementHeight - 1,
              item->window.borderSize, item->window.borderColor );
        }

        listPtr->endPos++;
        size -= listPtr->elementWidth;

        if( size < listPtr->elementHeight )
        {
          listPtr->drawPadding = listPtr->elementHeight - size;
          break;
        }

        y += listPtr->elementHeight;
        // fit++;
      }
    }
    else
    {
      float m = UI_Text_EmHeight( item->textscale );
      x = item->window.rect.x + one;
      y = item->window.rect.y + 1;

      for( i = listPtr->startPos; i < count; i++ )
      {
        char text[ MAX_STRING_CHARS ];
        // always draw at least one
        // which may overdraw the box if it is too small for the element

        if( listPtr->numColumns > 0 )
        {
          int j;

          for( j = 0; j < listPtr->numColumns; j++ )
          {
            float columnPos;
            float width, height;

            if( menu->window.aspectBias != ASPECT_NONE || item->window.aspectBias != ASPECT_NONE )
            {
              columnPos = ( listPtr->columnInfo[ j ].pos + 4.0f ) * DC->aspectScale;
              width = listPtr->columnInfo[ j ].width * DC->aspectScale;
            }
            else
            {
              columnPos = ( listPtr->columnInfo[ j ].pos + 4.0f );
              width = listPtr->columnInfo[ j ].width;
            }

            height = listPtr->columnInfo[ j ].width;

            Q_strncpyz( text, DC->feederItemText( item->feederID, i, j, &optionalImage ), sizeof( text ) );

            if( optionalImage >= 0 )
            {
              DC->drawHandlePic( x + columnPos, y + ( ( listPtr->elementHeight - height ) / 2.0f ),
                  width, height, optionalImage );
            }
            else if( text[ 0 ] )
            {
              int alignOffset = 0.0f, tw;

              tw = UI_Text_Width( text, item->textscale, 0 );

              // Shorten the string if it's too long

              while( tw > width && strlen( text ) > 0 )
              {
                text[ strlen( text ) - 1 ] = '\0';
                tw = UI_Text_Width( text, item->textscale, 0 );
              }

              switch( listPtr->columnInfo[ j ].align )
              {
                case ALIGN_LEFT:
                  alignOffset = 0.0f;
                  break;

                case ALIGN_RIGHT:
                  alignOffset = width - tw;
                  break;

                case ALIGN_CENTER:
                  alignOffset = ( width / 2.0f ) - ( tw / 2.0f );
                  break;

                default:
                  alignOffset = 0.0f;
              }

              UI_Text_Paint( x + columnPos + alignOffset,
                  y + m + ( ( listPtr->elementHeight - m ) / 2.0f ),
                  item->textscale, item->window.foreColor, text, 0,
                  0, item->textStyle );
            }
          }
        }
        else
        {
          float offset;

          if( menu->window.aspectBias != ASPECT_NONE || item->window.aspectBias != ASPECT_NONE )
            offset = 4.0f * DC->aspectScale;
          else
            offset = 4.0f;

          Q_strncpyz( text, DC->feederItemText( item->feederID, i, 0, &optionalImage ), sizeof( text ) );

          if( optionalImage >= 0 )
            DC->drawHandlePic( x + offset, y, listPtr->elementHeight, listPtr->elementHeight, optionalImage );
          else if( text[ 0 ] )
          {
            UI_Text_Paint( x + offset, y + m + ( ( listPtr->elementHeight - m ) / 2.0f ),
                item->textscale, item->window.foreColor, text, 0,
                0, item->textStyle );
          }
        }

        if( i == item->cursorPos )
        {
          DC->fillRect( x, y, item->window.rect.w - SCROLLBAR_WIDTH - ( two * item->window.borderSize ),
              listPtr->elementHeight, item->window.outlineColor );
        }

        listPtr->endPos++;
        size -= listPtr->elementHeight;

        if( size < listPtr->elementHeight )
        {
          listPtr->drawPadding = listPtr->elementHeight - size;
          break;
        }

        y += listPtr->elementHeight;
        // fit++;
      }
    }
  }

  // FIXME: hacky fix to off-by-one bug
  listPtr->endPos--;
}

void Item_OwnerDraw_Paint( itemDef_t *item )
{
  menuDef_t *parent;
  const char *text;

  if( item == NULL )
    return;

  parent = ( menuDef_t* )item->parent;

  if( DC->ownerDrawItem )
  {
    vec4_t color, lowLight;
    menuDef_t *parent = ( menuDef_t* )item->parent;
    Fade( &item->window.flags, &item->window.foreColor[3], parent->fadeClamp, &item->window.nextTime,
          parent->fadeCycle, qtrue, parent->fadeAmount );
    memcpy( &color, &item->window.foreColor, sizeof( color ) );

    if( item->numColors > 0 && DC->getValue )
    {
      // if the value is within one of the ranges then set color to that, otherwise leave at default
      int i;
      float f = DC->getValue( item->window.ownerDraw );

      for( i = 0; i < item->numColors; i++ )
      {
        if( f >= item->colorRanges[i].low && f <= item->colorRanges[i].high )
        {
          memcpy( &color, &item->colorRanges[i].color, sizeof( color ) );
          break;
        }
      }
    }

    if( item->window.flags & WINDOW_HASFOCUS )
      memcpy( color, &parent->focusColor, sizeof( vec4_t ) );
    else if( item->textStyle == ITEM_TEXTSTYLE_BLINK && !( ( DC->realTime / BLINK_DIVISOR ) & 1 ) )
    {
      lowLight[0] = 0.8 * item->window.foreColor[0];
      lowLight[1] = 0.8 * item->window.foreColor[1];
      lowLight[2] = 0.8 * item->window.foreColor[2];
      lowLight[3] = 0.8 * item->window.foreColor[3];
      LerpColor( item->window.foreColor, lowLight, color, 0.5 + 0.5 * sin( DC->realTime / PULSE_DIVISOR ) );
    }

    if( item->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) && !Item_EnableShowViaCvar( item, CVAR_ENABLE ) )
      Com_Memcpy( color, parent->disableColor, sizeof( vec4_t ) );

    if( DC->ownerDrawText && ( text = DC->ownerDrawText( item->window.ownerDraw ) ) )
    {
      if( item->text && *item->text )
      {
        Item_Text_Paint( item );

        UI_Text_Paint( item->textRect.x + item->textRect.w + ITEM_VALUE_OFFSET,
            item->textRect.y, item->textscale,
            color, text, 0, 0, item->textStyle );
      }
      else
      {
        item->text = text;
        Item_Text_Paint( item );
        item->text = NULL;
      }
    }
    else
    {
      DC->ownerDrawItem( item->window.rect.x, item->window.rect.y,
          item->window.rect.w, item->window.rect.h,
          item->textalignx, item->textaligny,
          item->window.ownerDraw, item->window.ownerDrawFlags,
          item->alignment, item->textalignment, item->textvalignment,
          item->window.borderSize, item->textscale, color, item->window.backColor,
          item->window.background, item->textStyle, item->modifier );
    }
  }
}


void Item_Paint( itemDef_t *item )
{
  int i;
  vec4_t red;
  menuDef_t *parent = ( menuDef_t* )item->parent;
  red[0] = red[3] = 1;
  red[1] = red[2] = 0;

  if( item == NULL )
    return;

  // check for delays
  for( i = 0; i < MAX_DELAYED_COMMANDS; i++ )
  {
    if( delayed_cmd[ i ].item == item && delayed_cmd[ i ].time && delayed_cmd[ i ].time < DC->realTime )
    {
      Item_RunScript( item, item->delayEvent );
      delayed_cmd[ i ].time = 0;
    }
  }

  if( item->window.flags & WINDOW_ORBITING )
  {
    if( DC->realTime > item->window.nextTime )
    {
      float rx, ry, a, c, s, w, h;

      item->window.nextTime = DC->realTime + item->window.offsetTime;
      // translate
      w = item->window.rectClient.w / 2;
      h = item->window.rectClient.h / 2;
      rx = item->window.rectClient.x + w - item->window.rectEffects.x;
      ry = item->window.rectClient.y + h - item->window.rectEffects.y;
      a = 3 * M_PI / 180;
      c = cos( a );
      s = sin( a );
      item->window.rectClient.x = ( rx * c - ry * s ) + item->window.rectEffects.x - w;
      item->window.rectClient.y = ( rx * s + ry * c ) + item->window.rectEffects.y - h;
      Item_UpdatePosition( item );

    }
  }


  if( item->window.flags & WINDOW_INTRANSITION )
  {
    int done = 0;
    int timePast = DC->realTime - item->window.offsetTime;
    item->window.offsetTime = DC->realTime;
    // transition the x,y

    if( item->window.rectClient.x == item->window.rectEffects.x )
      done++;
    else
    {
      if( item->window.rectClient.x < item->window.rectEffects.x )
      {
        item->window.rectClient.x += item->window.rectEffects2.x * timePast;

        if( item->window.rectClient.x > item->window.rectEffects.x )
        {
          item->window.rectClient.x = item->window.rectEffects.x;
          done++;
        }
      }
      else
      {
        item->window.rectClient.x -= item->window.rectEffects2.x * timePast;

        if( item->window.rectClient.x < item->window.rectEffects.x )
        {
          item->window.rectClient.x = item->window.rectEffects.x;
          done++;
        }
      }
    }

    if( item->window.rectClient.y == item->window.rectEffects.y )
      done++;
    else
    {
      if( item->window.rectClient.y < item->window.rectEffects.y )
      {
        item->window.rectClient.y += item->window.rectEffects2.y * timePast;

        if( item->window.rectClient.y > item->window.rectEffects.y )
        {
          item->window.rectClient.y = item->window.rectEffects.y;
          done++;
        }
      }
      else
      {
        item->window.rectClient.y -= item->window.rectEffects2.y * timePast;

        if( item->window.rectClient.y < item->window.rectEffects.y )
        {
          item->window.rectClient.y = item->window.rectEffects.y;
          done++;
        }
      }
    }

    if( item->window.rectClient.w == item->window.rectEffects.w )
      done++;
    else
    {
      if( item->window.rectClient.w < item->window.rectEffects.w )
      {
        item->window.rectClient.w += item->window.rectEffects2.w * timePast;

        if( item->window.rectClient.w > item->window.rectEffects.w )
        {
          item->window.rectClient.w = item->window.rectEffects.w;
          done++;
        }
      }
      else
      {
        item->window.rectClient.w -= item->window.rectEffects2.w * timePast;

        if( item->window.rectClient.w < item->window.rectEffects.w )
        {
          item->window.rectClient.w = item->window.rectEffects.w;
          done++;
        }
      }
    }

    if( item->window.rectClient.h == item->window.rectEffects.h )
      done++;
    else
    {
      if( item->window.rectClient.h < item->window.rectEffects.h )
      {
        item->window.rectClient.h += item->window.rectEffects2.h * timePast;

        if( item->window.rectClient.h > item->window.rectEffects.h )
        {
          item->window.rectClient.h = item->window.rectEffects.h;
          done++;
        }
      }
      else
      {
        item->window.rectClient.h -= item->window.rectEffects2.h * timePast;

        if( item->window.rectClient.h < item->window.rectEffects.h )
        {
          item->window.rectClient.h = item->window.rectEffects.h;
          done++;
        }
      }
    }

    Item_UpdatePosition( item );

    if( done == 4 )
      item->window.flags &= ~WINDOW_INTRANSITION;
  }

  if( item->window.ownerDrawFlags && DC->ownerDrawVisible )
  {
    if( !DC->ownerDrawVisible( item->window.ownerDrawFlags ) )
      item->window.flags &= ~WINDOW_VISIBLE;
    else
      item->window.flags |= WINDOW_VISIBLE;
  }

  if( item->window.ownerDraw == UI_SCREEN && DC->hideScreen )
  {
    if( DC->hideScreen( item->modifier ) )
      item->window.flags &= ~WINDOW_VISIBLE;
    else
      item->window.flags |= WINDOW_VISIBLE;
  }

  if( item->cvarFlags & ( CVAR_SHOW | CVAR_HIDE ) )
  {
    if( !Item_EnableShowViaCvar( item, CVAR_SHOW ) )
      return;
  }

  if( item->window.flags & WINDOW_TIMEDVISIBLE )
  {
  }

  if( !( item->window.flags & WINDOW_VISIBLE ) )
    return;

  Window_Paint( &item->window, parent->fadeAmount , parent->fadeClamp, parent->fadeCycle );

  if( DC->getCVarValue( "ui_developer" ) )
  {
    vec4_t color;
    rectDef_t *r = Item_CorrectedTextRect( item );
    color[1] = color[3] = 1;
    color[0] = color[2] = 0;
    DC->drawRect( r->x, r->y, r->w, r->h, 1, color );
  }

  switch( item->type )
  {
    case ITEM_TYPE_OWNERDRAW:
      Item_OwnerDraw_Paint( item );
      break;

    case ITEM_TYPE_TEXT:
    case ITEM_TYPE_BUTTON:
      Item_Text_Paint( item );
      break;

    case ITEM_TYPE_RADIOBUTTON:
      break;

    case ITEM_TYPE_CHECKBOX:
      break;

    case ITEM_TYPE_COMBO:
      Item_Combobox_Paint( item );
      break;

    case ITEM_TYPE_LISTBOX:
      Item_ListBox_Paint( item );
      break;

    //case ITEM_TYPE_IMAGE:
    //  Item_Image_Paint(item);
    //  break;

    case ITEM_TYPE_MODEL:
      Item_Model_Paint( item );
      break;

    case ITEM_TYPE_YESNO:
      Item_YesNo_Paint( item );
      break;

    case ITEM_TYPE_MULTI:
      Item_Multi_Paint( item );
      break;

    case ITEM_TYPE_BIND:
      Item_Bind_Paint( item );
      break;

    case ITEM_TYPE_SLIDER:
      Item_Slider_Paint( item );
      break;

    default:
      if( Item_IsEditField( item ) )
        Item_TextField_Paint( item );

      break;
  }

  Border_Paint( &item->window );
}

void Menu_Init( menuDef_t *menu )
{
  memset( menu, 0, sizeof( menuDef_t ) );
  menu->cursorItem = -1;
  menu->fadeAmount = DC->Assets.fadeAmount;
  menu->fadeClamp = DC->Assets.fadeClamp;
  menu->fadeCycle = DC->Assets.fadeCycle;
  Window_Init( &menu->window );
  menu->window.aspectBias = ALIGN_CENTER;
}

itemDef_t *Menu_GetFocusedItem( menuDef_t *menu )
{
  int i;

  if( menu )
  {
    for( i = 0; i < menu->itemCount; i++ )
    {
      if( menu->items[i]->window.flags & WINDOW_HASFOCUS )
        return menu->items[i];
    }
  }

  return NULL;
}

menuDef_t *Menu_GetFocused( void )
{
  int i;

  for( i = 0; i < menuCount; i++ )
  {
    if( Menus[i].window.flags & WINDOW_HASFOCUS && Menus[i].window.flags & WINDOW_VISIBLE )
      return & Menus[i];
  }

  return NULL;
}

void Menu_ScrollFeeder( menuDef_t *menu, int feeder, qboolean down )
{
  if( menu )
  {
    int i;

    for( i = 0; i < menu->itemCount; i++ )
    {
      if( menu->items[i]->feederID == feeder )
      {
        Item_ListBox_HandleKey( menu->items[i], ( down ) ? K_DOWNARROW : K_UPARROW, qtrue, qtrue );
        return;
      }
    }
  }
}



void Menu_SetFeederSelection( menuDef_t *menu, int feeder, int index, const char *name )
{
  if( menu == NULL )
  {
    if( name == NULL )
      menu = Menu_GetFocused();
    else
      menu = Menus_FindByName( name );
  }

  if( menu )
  {
    int i;

    for( i = 0; i < menu->itemCount; i++ )
    {
      if( menu->items[i]->feederID == feeder )
      {
        if( menu->items[i]->type == ITEM_TYPE_LISTBOX && index == 0 )
        {
          menu->items[ i ]->typeData.list->cursorPos = 0;
          menu->items[ i ]->typeData.list->startPos = 0;
        }

        menu->items[i]->cursorPos = index;
        DC->feederSelection( menu->items[i]->feederID, menu->items[i]->cursorPos );
        return;
      }
    }
  }
}

qboolean Menus_AnyFullScreenVisible( void )
{
  int i;

  for( i = 0; i < menuCount; i++ )
  {
    if( Menus[i].window.flags & WINDOW_VISIBLE && Menus[i].fullScreen )
      return qtrue;
  }

  return qfalse;
}

menuDef_t *Menus_ActivateByName( const char *p )
{
  int i;
  menuDef_t *m = NULL;

  // Activate one menu

  for( i = 0; i < menuCount; i++ )
  {
    if( Q_stricmp( Menus[i].window.name, p ) == 0 )
    {
      m = &Menus[i];
      Menus_Activate( m );
      break;
    }
  }

  // Defocus the others
  for( i = 0; i < menuCount; i++ )
  {
    if( Q_stricmp( Menus[i].window.name, p ) != 0 )
      Menus[i].window.flags &= ~WINDOW_HASFOCUS;
  }

  return m;
}

menuDef_t *Menus_ReplaceActiveByName( const char *p )
{
  int i;
  menuDef_t *m = NULL;

  // Activate one menu

  for( i = 0; i < menuCount; i++ )
  {
    if( Q_stricmp( Menus[i].window.name, p ) == 0 )
    {
      m = &Menus[i];
      if(! Menus_ReplaceActive( m ) )
        return NULL;
      break;
    }
  }
  return m;
}


void Item_Init( itemDef_t *item )
{
  memset( item, 0, sizeof( itemDef_t ) );
  item->textscale = 0.55f;
  Window_Init( &item->window );
  item->window.aspectBias = ASPECT_NONE;
}

void Menu_HandleMouseMove( menuDef_t *menu, float x, float y )
{
  int i, pass;
  qboolean focusSet = qfalse;

  itemDef_t *overItem;

  if( menu == NULL )
    return;

  if( !( menu->window.flags & ( WINDOW_VISIBLE | WINDOW_FORCED ) ) )
    return;

  if( itemCapture )
  {
    //Item_MouseMove(itemCapture, x, y);
    return;
  }

  if( g_waitingForKey || g_editingField )
    return;

  // KTW: Draggable windows
  if ( (menu->window.flags & WINDOW_HASFOCUS) && (menu->window.flags & WINDOW_DRAG))
  {
    menu->window.rect.x +=  DC->cursordx;
    menu->window.rect.y +=  DC->cursordy;

    if (menu->window.rect.x < 0)
      menu->window.rect.x = 0;
    if (menu->window.rect.x + menu->window.rect.w > SCREEN_WIDTH)
      menu->window.rect.x = SCREEN_WIDTH-menu->window.rect.w;

    if (menu->window.rect.y < 0)
      menu->window.rect.y = 0;
    if (menu->window.rect.y + menu->window.rect.h > SCREEN_HEIGHT)
      menu->window.rect.y = SCREEN_HEIGHT-menu->window.rect.h;

    Menu_UpdatePosition(menu);

    for (i = 0; i < menu->itemCount; i++)
      Item_UpdatePosition(menu->items[i]);
  }

  // FIXME: this is the whole issue of focus vs. mouse over..
  // need a better overall solution as i don't like going through everything twice
  for( pass = 0; pass < 2; pass++ )
  {
    for( i = 0; i < menu->itemCount; i++ )
    {
      // turn off focus each item
      // menu->items[i].window.flags &= ~WINDOW_HASFOCUS;

      if( !( menu->items[i]->window.flags & ( WINDOW_VISIBLE | WINDOW_FORCED ) ) )
        continue;

      // items can be enabled and disabled based on cvars
      if( menu->items[i]->cvarFlags & ( CVAR_ENABLE | CVAR_DISABLE ) &&
          !Item_EnableShowViaCvar( menu->items[i], CVAR_ENABLE ) )
        continue;

      if( menu->items[i]->cvarFlags & ( CVAR_SHOW | CVAR_HIDE ) &&
          !Item_EnableShowViaCvar( menu->items[i], CVAR_SHOW ) )
        continue;

      if( Rect_ContainsPoint( &menu->items[i]->window.rect, x, y ) )
      {
        if( pass == 1 )
        {
          overItem = menu->items[i];

          if( overItem->type == ITEM_TYPE_TEXT && overItem->text )
          {
            if( !Rect_ContainsPoint( Item_CorrectedTextRect( overItem ), x, y ) )
              continue;
          }

          // if we are over an item
          if( IsVisible( overItem->window.flags ) )
          {
            // different one
            Item_MouseEnter( overItem, x, y );
            // Item_SetMouseOver(overItem, qtrue);

            // if item is not a decoration see if it can take focus

            if( !focusSet )
              focusSet = Item_SetFocus( overItem, x, y );
          }
        }
      }
      else if( menu->items[i]->window.flags & WINDOW_MOUSEOVER )
      {
        Item_MouseLeave( menu->items[i] );
        Item_SetMouseOver( menu->items[i], qfalse );
      }
    }
  }

}

void Menu_Paint( menuDef_t *menu, qboolean forcePaint )
{
  int i;
  itemDef_t item;
  char listened_text[1024];
  
  if( menu == NULL )
    return;

  if( !( menu->window.flags & WINDOW_VISIBLE ) &&  !forcePaint )
    return;

  if( menu->window.ownerDrawFlags && DC->ownerDrawVisible && !DC->ownerDrawVisible( menu->window.ownerDrawFlags ) )
    return;

  if( forcePaint )
    menu->window.flags |= WINDOW_FORCED;

  //Executes the text stored in the listened cvar as an UIscript
  if( menu->listenCvar && menu->listenCvar[0] )
  {
    DC->getCVarString( menu->listenCvar, listened_text, sizeof(listened_text) );
    if( listened_text[0] )
    {
      item.parent = menu;
      Item_RunScript( &item, listened_text );
      DC->setCVar( menu->listenCvar, "" );
    }
  }

  // draw the background if necessary
  if( menu->fullScreen )
  {
    // implies a background shader
    // FIXME: make sure we have a default shader if fullscreen is set with no background
    DC->drawHandlePic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, menu->window.background );
  }

  // paint the background and or border
  Window_Paint( &menu->window, menu->fadeAmount, menu->fadeClamp, menu->fadeCycle );

  Border_Paint( &menu->window );

  for( i = 0; i < menu->itemCount; i++ )
    Item_Paint( menu->items[i] );

  if( DC->getCVarValue( "ui_developer" ) )
  {
    vec4_t color;
    color[0] = color[2] = color[3] = 1;
    color[1] = 0;
    DC->drawRect( menu->window.rect.x, menu->window.rect.y, menu->window.rect.w, menu->window.rect.h, 1, color );
  }
}

/*
===============
Keyword Hash
===============
*/

#define KEYWORDHASH_SIZE  512

typedef struct keywordHash_s
{
  char *keyword;
  qboolean ( *func )( itemDef_t *item, int handle );

  int param;

  struct keywordHash_s *next;
}

keywordHash_t;

int KeywordHash_Key( char *keyword )
{
  int register hash, i;

  hash = 0;

  for( i = 0; keyword[i] != '\0'; i++ )
  {
    if( keyword[i] >= 'A' && keyword[i] <= 'Z' )
      hash += ( keyword[i] + ( 'a' - 'A' ) ) * ( 119 + i );
    else
      hash += keyword[i] * ( 119 + i );
  }

  hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) ) & ( KEYWORDHASH_SIZE - 1 );
  return hash;
}

void KeywordHash_Add( keywordHash_t *table[], keywordHash_t *key )
{
  int hash;

  hash = KeywordHash_Key( key->keyword );
  /*
     if(table[hash])     int collision = qtrue;
     */
  key->next = table[hash];
  table[hash] = key;
}

keywordHash_t *KeywordHash_Find( keywordHash_t *table[], char *keyword )
{
  keywordHash_t *key;
  int hash;

  hash = KeywordHash_Key( keyword );

  for( key = table[hash]; key; key = key->next )
  {
    if( !Q_stricmp( key->keyword, keyword ) )
      return key;
  }

  return NULL;
}

/*
===============
Item_DataType

Give a numeric representation of which typeData union element this item uses
===============
*/
itemDataType_t Item_DataType( itemDef_t *item )
{
  switch( item->type )
  {
    default:
    case ITEM_TYPE_NONE:
      return TYPE_NONE;

    case ITEM_TYPE_LISTBOX:
      return TYPE_LIST;

    case ITEM_TYPE_COMBO:
      return TYPE_COMBO;

    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_SAYFIELD:
    case ITEM_TYPE_YESNO:
    case ITEM_TYPE_BIND:
    case ITEM_TYPE_SLIDER:
    case ITEM_TYPE_TEXT:
      return TYPE_EDIT;

    case ITEM_TYPE_MULTI:
      return TYPE_MULTI;

    case ITEM_TYPE_MODEL:
      return TYPE_MODEL;
  }
}

/*
===============
Item_IsEditField
===============
*/
static ID_INLINE qboolean Item_IsEditField( itemDef_t *item )
{
  switch( item->type )
  {
    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_SAYFIELD:
      return qtrue;

    default:
      return qfalse;
  }
}

/*
===============
Item Keyword Parse functions
===============
*/

// name <string>
qboolean ItemParse_name( itemDef_t *item, int handle )
{
  if( !PC_String_Parse( handle, &item->window.name ) )
    return qfalse;

  return qtrue;
}

// name <string>
qboolean ItemParse_focusSound( itemDef_t *item, int handle )
{
  const char *temp;

  if( !PC_String_Parse( handle, &temp ) )
    return qfalse;

  item->focusSound = DC->registerSound( temp, qfalse );
  return qtrue;
}


// text <string>
qboolean ItemParse_text( itemDef_t *item, int handle )
{
  if( !PC_String_Parse( handle, &item->text ) )
    return qfalse;

  return qtrue;
}

// group <string>
qboolean ItemParse_group( itemDef_t *item, int handle )
{
  if( !PC_String_Parse( handle, &item->window.group ) )
    return qfalse;

  return qtrue;
}

// asset_model <string>
qboolean ItemParse_asset_model( itemDef_t *item, int handle )
{
  const char *temp;

  if( !PC_String_Parse( handle, &temp ) )
    return qfalse;

  item->asset = DC->registerModel( temp );
  item->typeData.model->angle = rand() % 360;
  return qtrue;
}

// asset_shader <string>
qboolean ItemParse_asset_shader( itemDef_t *item, int handle )
{
  const char *temp;

  if( !PC_String_Parse( handle, &temp ) )
    return qfalse;

  item->asset = DC->registerShaderNoMip( temp );
  return qtrue;
}

// model_origin <number> <number> <number>
qboolean ItemParse_model_origin( itemDef_t *item, int handle )
{
  return ( PC_Float_Parse( handle, &item->typeData.model->origin[0] ) &&
           PC_Float_Parse( handle, &item->typeData.model->origin[1] ) &&
           PC_Float_Parse( handle, &item->typeData.model->origin[2] ) );
}

// model_fovx <number>
qboolean ItemParse_model_fovx( itemDef_t *item, int handle )
{
  return PC_Float_Parse( handle, &item->typeData.model->fov_x );
}

// model_fovy <number>
qboolean ItemParse_model_fovy( itemDef_t *item, int handle )
{
  return PC_Float_Parse( handle, &item->typeData.model->fov_y );
}

// model_rotation <integer>
qboolean ItemParse_model_rotation( itemDef_t *item, int handle )
{
  return PC_Int_Parse( handle, &item->typeData.model->rotationSpeed );
}

// model_angle <integer>
qboolean ItemParse_model_angle( itemDef_t *item, int handle )
{
  return PC_Int_Parse( handle, &item->typeData.model->angle );
}

// model_axis <number> <number> <number> //:://
qboolean ItemParse_model_axis( itemDef_t *item, int handle ) {
  return ( PC_Float_Parse( handle, &item->typeData.model->axis[0] ) &&
           PC_Float_Parse( handle, &item->typeData.model->axis[1] ) &&
           PC_Float_Parse( handle, &item->typeData.model->axis[2] ) );
}

// model_animplay <int(startframe)> <int(numframes)> <int(fps)>
qboolean ItemParse_model_animplay(itemDef_t *item, int handle ) {
  modelDef_t *modelPtr = item->typeData.model;
  modelPtr->animated = 1;

  if (!PC_Int_Parse(handle, &modelPtr->startframe)) return qfalse;
  if (!PC_Int_Parse(handle, &modelPtr->numframes)) return qfalse;
  if (!PC_Int_Parse(handle, &modelPtr->fps)) return qfalse;

  modelPtr->frame = modelPtr->startframe + 1;
  modelPtr->oldframe = modelPtr->startframe;
  modelPtr->backlerp = 0.0f;
  modelPtr->frameTime = DC->realTime;

  return qtrue;
}

// rect <rectangle>
qboolean ItemParse_rect( itemDef_t *item, int handle )
{
  if( !PC_Rect_Parse( handle, &item->window.rectClient ) )
    return qfalse;

  return qtrue;
}

// aspectBias <bias>
qboolean ItemParse_aspectBias( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->window.aspectBias ) )
    return qfalse;

  return qtrue;
}

// style <integer>
qboolean ItemParse_style( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->window.style ) )
    return qfalse;

  return qtrue;
}

// decoration
qboolean ItemParse_decoration( itemDef_t *item, int handle )
{
  item->window.flags |= WINDOW_DECORATION;
  return qtrue;
}

// notselectable
qboolean ItemParse_notselectable( itemDef_t *item, int handle )
{
  item->typeData.list->notselectable = qtrue;
  return qtrue;
}

// noscrollbar
qboolean ItemParse_noscrollbar( itemDef_t *item, int handle )
{
  item->typeData.list->noscrollbar = qtrue;
  return qtrue;
}

// auto wrapped
qboolean ItemParse_wrapped( itemDef_t *item, int handle )
{
  item->window.flags |= WINDOW_WRAPPED;
  return qtrue;
}


// horizontalscroll
qboolean ItemParse_horizontalscroll( itemDef_t *item, int handle )
{
  item->window.flags |= WINDOW_HORIZONTAL;
  return qtrue;
}

// type <integer>
qboolean ItemParse_type( itemDef_t *item, int handle )
{
  if( item->type != ITEM_TYPE_NONE )
  {
    PC_SourceError( handle, "item already has a type" );
    return qfalse;
  }

  if( !PC_Int_Parse( handle, &item->type ) )
    return qfalse;

  if( item->type == ITEM_TYPE_NONE )
  {
    PC_SourceError( handle, "type must not be none" );
    return qfalse;
  }

  // allocate the relevant type data
  switch( item->type )
  {
    case ITEM_TYPE_LISTBOX:
      item->typeData.list = UI_Alloc( sizeof( listBoxDef_t ) );
      memset( item->typeData.list, 0, sizeof( listBoxDef_t ) );
      break;

    case ITEM_TYPE_COMBO:
      item->typeData.combo = UI_Alloc( sizeof( comboBoxDef_t ) );
      memset( item->typeData.combo, 0, sizeof( comboBoxDef_t ) );
      break;

    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_SAYFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_YESNO:
    case ITEM_TYPE_BIND:
    case ITEM_TYPE_SLIDER:
    case ITEM_TYPE_TEXT:
      item->typeData.edit = UI_Alloc( sizeof( editFieldDef_t ) );
      memset( item->typeData.edit, 0, sizeof( editFieldDef_t ) );

      if( item->type == ITEM_TYPE_EDITFIELD || item->type == ITEM_TYPE_SAYFIELD )
        item->typeData.edit->maxPaintChars = MAX_EDITFIELD;
      break;

    case ITEM_TYPE_MULTI:
      item->typeData.multi = UI_Alloc( sizeof( multiDef_t ) );
      memset( item->typeData.multi, 0, sizeof( multiDef_t ) );
      break;

    case ITEM_TYPE_MODEL:
      item->typeData.model = UI_Alloc( sizeof( modelDef_t ) );
      memset( item->typeData.model, 0, sizeof( modelDef_t ) );
      break;

    default:
      break;
  }

  return qtrue;
}

// elementwidth, used for listbox image elements
// uses textalignx for storage
qboolean ItemParse_elementwidth( itemDef_t *item, int handle )
{
  return PC_Float_Parse( handle, &item->typeData.list->elementWidth );
}

// elementheight, used for listbox image elements
// uses textaligny for storage
qboolean ItemParse_elementheight( itemDef_t *item, int handle )
{
  return PC_Float_Parse( handle, &item->typeData.list->elementHeight );
}

// feeder <int>
qboolean ItemParse_feeder( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->feederID ) )
    return qfalse;

  return qtrue;
}

// elementtype, used to specify what type of elements a listbox contains
// uses textstyle for storage
qboolean ItemParse_elementtype( itemDef_t *item, int handle )
{
  return PC_Int_Parse( handle, &item->typeData.list->elementStyle );
}

// columns sets a number of columns and an x pos and width per..
qboolean ItemParse_columns( itemDef_t *item, int handle )
{
  int i;

  if( !PC_Int_Parse( handle, &item->typeData.list->numColumns ) )
    return qfalse;

  if( item->typeData.list->numColumns > MAX_LB_COLUMNS )
  {
    PC_SourceError( handle, "exceeded maximum allowed columns (%d)",
                    MAX_LB_COLUMNS );
    return qfalse;
  }

  for( i = 0; i < item->typeData.list->numColumns; i++ )
  {
    int pos, width, align;

    if( !PC_Int_Parse( handle, &pos ) ||
        !PC_Int_Parse( handle, &width ) ||
        !PC_Int_Parse( handle, &align ) )
      return qfalse;

    item->typeData.list->columnInfo[i].pos = pos;
    item->typeData.list->columnInfo[i].width = width;
    item->typeData.list->columnInfo[i].align = align;
  }

  return qtrue;
}

qboolean ItemParse_border( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->window.border ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_bordersize( itemDef_t *item, int handle )
{
  if( !PC_Float_Parse( handle, &item->window.borderSize ) )
    return qfalse;

  return qtrue;
}

// FIXME: why does this require a parameter? visible MENU_FALSE does nothing
qboolean ItemParse_visible( itemDef_t *item, int handle )
{
  int i;

  if( !PC_Int_Parse( handle, &i ) )
    return qfalse;

  item->window.flags &= ~WINDOW_VISIBLE;
  if( i )
    item->window.flags |= WINDOW_VISIBLE;

  return qtrue;
}

// ownerdraw <number>, implies ITEM_TYPE_OWNERDRAW
qboolean ItemParse_ownerdraw( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->window.ownerDraw ) )
    return qfalse;

  if( item->type != ITEM_TYPE_NONE && item->type != ITEM_TYPE_OWNERDRAW )
  {
    PC_SourceError( handle, "ownerdraws cannot have an item type" );
    return qfalse;
  }

  item->type = ITEM_TYPE_OWNERDRAW;

  return qtrue;
}

qboolean ItemParse_align( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->alignment ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_textalign( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->textalignment ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_textvalign( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->textvalignment ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_textalignx( itemDef_t *item, int handle )
{
  if( !PC_Float_Parse( handle, &item->textalignx ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_textaligny( itemDef_t *item, int handle )
{
  if( !PC_Float_Parse( handle, &item->textaligny ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_textscale( itemDef_t *item, int handle )
{
  if( !PC_Float_Parse( handle, &item->textscale ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_textstyle( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->textStyle ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_backcolor( itemDef_t *item, int handle )
{
  int i;
  float f;

  for( i = 0; i < 4; i++ )
  {
    if( !PC_Float_Parse( handle, &f ) )
      return qfalse;

    item->window.backColor[i]  = f;
  }

  return qtrue;
}

qboolean ItemParse_forecolor( itemDef_t *item, int handle )
{
  int i;
  float f;

  for( i = 0; i < 4; i++ )
  {
    if( !PC_Float_Parse( handle, &f ) )
      return qfalse;

    item->window.foreColor[i]  = f;
    item->window.flags |= WINDOW_FORECOLORSET;
  }

  return qtrue;
}

qboolean ItemParse_bordercolor( itemDef_t *item, int handle )
{
  int i;
  float f;

  for( i = 0; i < 4; i++ )
  {
    if( !PC_Float_Parse( handle, &f ) )
      return qfalse;

    item->window.borderColor[i]  = f;
  }

  return qtrue;
}

qboolean ItemParse_outlinecolor( itemDef_t *item, int handle )
{
  if( !PC_Color_Parse( handle, &item->window.outlineColor ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_background( itemDef_t *item, int handle )
{
  const char *temp;

  if( !PC_String_Parse( handle, &temp ) )
    return qfalse;

  item->window.background = DC->registerShaderNoMip( temp );
  return qtrue;
}

qboolean ItemParse_cinematic( itemDef_t *item, int handle )
{
  if( !PC_String_Parse( handle, &item->window.cinematicName ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_doubleClick( itemDef_t *item, int handle )
{
  return ( item->typeData.list &&
           PC_Script_Parse( handle, &item->typeData.list->doubleClick ) );
}

qboolean ItemParse_onFocus( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->onFocus ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_leaveFocus( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->leaveFocus ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_mouseEnter( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->mouseEnter ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_mouseExit( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->mouseExit ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_mouseEnterText( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->mouseEnterText ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_mouseExitText( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->mouseExitText ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_onTextEntry( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->onTextEntry ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_delayEvent( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->delayEvent ) )
    return qfalse;

  return qtrue;
}


qboolean ItemParse_onCharEntry( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->onCharEntry ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_action( itemDef_t *item, int handle )
{
  if( !PC_Script_Parse( handle, &item->action ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_modifier( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->modifier ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_cvarTest( itemDef_t *item, int handle )
{
  if( !PC_String_Parse( handle, &item->cvarTest ) )
    return qfalse;

  return qtrue;
}

qboolean ItemParse_cvar( itemDef_t *item, int handle )
{
  if( !PC_String_Parse( handle, &item->cvar ) )
    return qfalse;

  if( Item_DataType( item ) == TYPE_EDIT )
  {
    item->typeData.edit->minVal = -1;
    item->typeData.edit->maxVal = -1;
    item->typeData.edit->defVal = -1;
  }

  return qtrue;
}

qboolean ItemParse_maxChars( itemDef_t *item, int handle )
{
  return PC_Int_Parse( handle, &item->typeData.edit->maxChars );
}

qboolean ItemParse_maxPaintChars( itemDef_t *item, int handle )
{
  return PC_Int_Parse( handle, &item->typeData.edit->maxPaintChars );
}

qboolean ItemParse_maxFieldWidth( itemDef_t *item, int handle )
{
  if( !PC_Int_Parse( handle, &item->typeData.edit->maxFieldWidth ) )
    return qfalse;

  if( item->typeData.edit->maxFieldWidth < MIN_FIELD_WIDTH )
  {
    PC_SourceError( handle, "max field width must be at least %d",
                    MIN_FIELD_WIDTH );
    return qfalse;
  }

  return qtrue;
}



qboolean ItemParse_cvarFloat( itemDef_t *item, int handle )
{
  return ( PC_String_Parse( handle, &item->cvar ) &&
           PC_Float_Parse( handle, &item->typeData.edit->defVal ) &&
           PC_Float_Parse( handle, &item->typeData.edit->minVal ) &&
           PC_Float_Parse( handle, &item->typeData.edit->maxVal ) );
}

qboolean ItemParse_cvarStrList( itemDef_t *item, int handle )
{
  pc_token_t token;
  multiDef_t *multiPtr;
  int pass;

  multiPtr = item->typeData.multi;
  multiPtr->count = 0;
  multiPtr->strDef = qtrue;

  if( !trap_Parse_ReadToken( handle, &token ) )
    return qfalse;

  if( *token.string != '{' )
    return qfalse;

  pass = 0;

  while( 1 )
  {
    if( !trap_Parse_ReadToken( handle, &token ) )
    {
      PC_SourceError( handle, "end of file inside menu item\n" );
      return qfalse;
    }

    if( *token.string == '}' )
      return qtrue;

    if( *token.string == ',' || *token.string == ';' )
      continue;

    if( pass == 0 )
    {
      multiPtr->cvarList[multiPtr->count] = String_Alloc( token.string );
      pass = 1;
    }
    else
    {
      multiPtr->cvarStr[multiPtr->count] = String_Alloc( token.string );
      pass = 0;
      multiPtr->count++;

      if( multiPtr->count >= MAX_MULTI_CVARS )
      {
        PC_SourceError( handle, "cvar string list may not exceed %d cvars",
                        MAX_MULTI_CVARS );
        return qfalse;
      }
    }

  }

  return qfalse;
}

qboolean ItemParse_cvarFloatList( itemDef_t *item, int handle )
{
  pc_token_t token;
  multiDef_t *multiPtr;

  multiPtr = item->typeData.multi;
  multiPtr->count = 0;
  multiPtr->strDef = qfalse;

  if( !trap_Parse_ReadToken( handle, &token ) )
    return qfalse;

  if( *token.string != '{' )
    return qfalse;

  while( 1 )
  {
    if( !trap_Parse_ReadToken( handle, &token ) )
    {
      PC_SourceError( handle, "end of file inside menu item\n" );
      return qfalse;
    }

    if( *token.string == '}' )
      return qtrue;

    if( *token.string == ',' || *token.string == ';' )
      continue;

    multiPtr->cvarList[multiPtr->count] = String_Alloc( token.string );

    if( !PC_Float_Parse( handle, &multiPtr->cvarValue[multiPtr->count] ) )
      return qfalse;

    multiPtr->count++;

    if( multiPtr->count >= MAX_MULTI_CVARS )
    {
      PC_SourceError( handle, "cvar string list may not exceed %d cvars",
                      MAX_MULTI_CVARS );
      return qfalse;
    }
  }

  return qfalse;
}



qboolean ItemParse_addColorRange( itemDef_t *item, int handle )
{
  colorRangeDef_t color;

  if( PC_Float_Parse( handle, &color.low ) &&
      PC_Float_Parse( handle, &color.high ) &&
      PC_Color_Parse( handle, &color.color ) )
  {
    if( item->numColors < MAX_COLOR_RANGES )
    {
      memcpy( &item->colorRanges[item->numColors], &color, sizeof( color ) );
      item->numColors++;
    }
    else
    {
      PC_SourceError( handle, "may not exceed %d color ranges",
                      MAX_COLOR_RANGES );
      return qfalse;
    }

    return qtrue;
  }

  return qfalse;
}

qboolean ItemParse_ownerdrawFlag( itemDef_t *item, int handle )
{
  int i;

  if( !PC_Int_Parse( handle, &i ) )
    return qfalse;

  item->window.ownerDrawFlags |= i;
  return qtrue;
}

qboolean ItemParse_enableCvar( itemDef_t *item, int handle )
{
  if( PC_Script_Parse( handle, &item->enableCvar ) )
  {
    item->cvarFlags = CVAR_ENABLE;
    return qtrue;
  }

  return qfalse;
}

qboolean ItemParse_disableCvar( itemDef_t *item, int handle )
{
  if( PC_Script_Parse( handle, &item->enableCvar ) )
  {
    item->cvarFlags = CVAR_DISABLE;
    return qtrue;
  }

  return qfalse;
}

qboolean ItemParse_showCvar( itemDef_t *item, int handle )
{
  if( PC_Script_Parse( handle, &item->enableCvar ) )
  {
    item->cvarFlags = CVAR_SHOW;
    return qtrue;
  }

  return qfalse;
}

qboolean ItemParse_hideCvar( itemDef_t *item, int handle )
{
  if( PC_Script_Parse( handle, &item->enableCvar ) )
  {
    item->cvarFlags = CVAR_HIDE;
    return qtrue;
  }

  return qfalse;
}


keywordHash_t itemParseKeywords[] = {
  {"name", ItemParse_name, TYPE_ANY},
  {"type", ItemParse_type, TYPE_ANY},
  {"text", ItemParse_text, TYPE_ANY},
  {"group", ItemParse_group, TYPE_ANY},
  {"asset_model", ItemParse_asset_model, TYPE_MODEL},
  {"asset_shader", ItemParse_asset_shader, TYPE_ANY}, // ?
  {"model_origin", ItemParse_model_origin, TYPE_MODEL},
  {"model_fovx", ItemParse_model_fovx, TYPE_MODEL},
  {"model_fovy", ItemParse_model_fovy, TYPE_MODEL},
  {"model_rotation", ItemParse_model_rotation, TYPE_MODEL},
  {"model_angle", ItemParse_model_angle, TYPE_MODEL},
  {"model_axis", ItemParse_model_axis, TYPE_MODEL},
  {"model_animplay", ItemParse_model_animplay, TYPE_MODEL},
  {"rect", ItemParse_rect, TYPE_ANY},
  {"aspectBias", ItemParse_aspectBias, TYPE_ANY},
  {"style", ItemParse_style, TYPE_ANY},
  {"decoration", ItemParse_decoration, TYPE_ANY},
  {"notselectable", ItemParse_notselectable, TYPE_LIST},
  {"noscrollbar", ItemParse_noscrollbar, TYPE_LIST},
  {"wrapped", ItemParse_wrapped, TYPE_ANY},
  {"horizontalscroll", ItemParse_horizontalscroll, TYPE_ANY},
  {"elementwidth", ItemParse_elementwidth, TYPE_LIST},
  {"elementheight", ItemParse_elementheight, TYPE_LIST},
  {"feeder", ItemParse_feeder, TYPE_ANY},
  {"elementtype", ItemParse_elementtype, TYPE_LIST},
  {"columns", ItemParse_columns, TYPE_LIST},
  {"border", ItemParse_border, TYPE_ANY},
  {"bordersize", ItemParse_bordersize, TYPE_ANY},
  {"visible", ItemParse_visible, TYPE_ANY},
  {"ownerdraw", ItemParse_ownerdraw, TYPE_ANY},
  {"align", ItemParse_align, TYPE_ANY},
  {"textalign", ItemParse_textalign, TYPE_ANY},
  {"textvalign", ItemParse_textvalign, TYPE_ANY},
  {"textalignx", ItemParse_textalignx, TYPE_ANY},
  {"textaligny", ItemParse_textaligny, TYPE_ANY},
  {"textscale", ItemParse_textscale, TYPE_ANY},
  {"textstyle", ItemParse_textstyle, TYPE_ANY},
  {"backcolor", ItemParse_backcolor, TYPE_ANY},
  {"forecolor", ItemParse_forecolor, TYPE_ANY},
  {"bordercolor", ItemParse_bordercolor, TYPE_ANY},
  {"outlinecolor", ItemParse_outlinecolor, TYPE_ANY},
  {"background", ItemParse_background, TYPE_ANY},
  {"onFocus", ItemParse_onFocus, TYPE_ANY},
  {"leaveFocus", ItemParse_leaveFocus, TYPE_ANY},
  {"mouseEnter", ItemParse_mouseEnter, TYPE_ANY},
  {"mouseExit", ItemParse_mouseExit, TYPE_ANY},
  {"mouseEnterText", ItemParse_mouseEnterText, TYPE_ANY},
  {"mouseExitText", ItemParse_mouseExitText, TYPE_ANY},
  {"onTextEntry", ItemParse_onTextEntry, TYPE_ANY},
  {"onCharEntry", ItemParse_onCharEntry, TYPE_ANY},
  {"delayEvent", ItemParse_delayEvent, TYPE_ANY},
  {"action", ItemParse_action, TYPE_ANY},
  {"modifier", ItemParse_modifier, TYPE_ANY},
  {"cvar", ItemParse_cvar, TYPE_ANY},
  {"maxChars", ItemParse_maxChars, TYPE_EDIT},
  {"maxPaintChars", ItemParse_maxPaintChars, TYPE_EDIT},
  {"maxFieldWidth", ItemParse_maxFieldWidth, TYPE_EDIT},
  {"focusSound", ItemParse_focusSound, TYPE_ANY},
  {"cvarFloat", ItemParse_cvarFloat, TYPE_EDIT},
  {"cvarStrList", ItemParse_cvarStrList, TYPE_MULTI},
  {"cvarFloatList", ItemParse_cvarFloatList, TYPE_MULTI},
  {"addColorRange", ItemParse_addColorRange, TYPE_ANY},
  {"ownerdrawFlag", ItemParse_ownerdrawFlag, TYPE_ANY}, // hm.
  {"enableCvar", ItemParse_enableCvar, TYPE_ANY},
  {"cvarTest", ItemParse_cvarTest, TYPE_ANY},
  {"disableCvar", ItemParse_disableCvar, TYPE_ANY},
  {"showCvar", ItemParse_showCvar, TYPE_ANY},
  {"hideCvar", ItemParse_hideCvar, TYPE_ANY},
  {"cinematic", ItemParse_cinematic, TYPE_ANY},
  {"doubleclick", ItemParse_doubleClick, TYPE_LIST},
  {NULL, voidFunction2}
};

keywordHash_t *itemParseKeywordHash[KEYWORDHASH_SIZE];

/*
===============
Item_SetupKeywordHash
===============
*/
void Item_SetupKeywordHash( void )
{
  int i;

  memset( itemParseKeywordHash, 0, sizeof( itemParseKeywordHash ) );

  for( i = 0; itemParseKeywords[ i ].keyword; i++ )
    KeywordHash_Add( itemParseKeywordHash, &itemParseKeywords[ i ] );
}

/*
===============
Item_Parse
===============
*/
qboolean Item_Parse( int handle, itemDef_t *item )
{
  pc_token_t token;
  keywordHash_t *key;


  if( !trap_Parse_ReadToken( handle, &token ) )
    return qfalse;

  if( *token.string != '{' )
    return qfalse;

  while( 1 )
  {
    if( !trap_Parse_ReadToken( handle, &token ) )
    {
      PC_SourceError( handle, "end of file inside menu item\n" );
      return qfalse;
    }

    if( *token.string == '}' )
      return qtrue;

    key = KeywordHash_Find( itemParseKeywordHash, token.string );

    if( !key )
    {
      PC_SourceError( handle, "unknown menu item keyword %s", token.string );
      continue;
    }

    // do type-checks
    if( key->param != TYPE_ANY )
    {
      itemDataType_t test = Item_DataType( item );

      if( test != key->param )
      {
        if( test == ITEM_TYPE_NONE )
          PC_SourceError( handle, "menu item keyword %s requires "
                          "type specification", token.string );
        else
          PC_SourceError( handle, "menu item keyword %s is incompatible with "
                          "specified item type", token.string );
        continue;
      }
    }

    if( !key->func( item, handle ) )
    {
      PC_SourceError( handle, "couldn't parse menu item keyword %s", token.string );
      return qfalse;
    }
  }

  return qfalse;
}


// Item_InitControls
// init's special control types
void Item_InitControls( itemDef_t *item )
{
  if( item == NULL )
    return;

  if( item->type == ITEM_TYPE_LISTBOX )
  {
    item->cursorPos = 0;

    if( item->typeData.list )
    {
      item->typeData.list->cursorPos = 0;
      item->typeData.list->startPos = 0;
      item->typeData.list->endPos = 0;
      item->typeData.list->cursorPos = 0;
    }
  }
}

/*
===============
Menu Keyword Parse functions
===============
*/

qboolean MenuParse_font( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_String_Parse( handle, &menu->font ) )
    return qfalse;

  if( !DC->Assets.fontRegistered )
  {
    DC->registerFont( menu->font, 48, &DC->Assets.textFont );
    DC->Assets.fontRegistered = qtrue;
  }

  return qtrue;
}

qboolean MenuParse_name( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_String_Parse( handle, &menu->window.name ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_fullscreen( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Int_Parse( handle, ( int* ) & menu->fullScreen ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_rect( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Rect_Parse( handle, &menu->window.rect ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_aspectBias( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Int_Parse( handle, &menu->window.aspectBias ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_style( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Int_Parse( handle, &menu->window.style ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_visible( itemDef_t *item, int handle )
{
  int i;
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Int_Parse( handle, &i ) )
    return qfalse;

  if( i )
    menu->window.flags |= WINDOW_VISIBLE;

  return qtrue;
}

qboolean MenuParse_dontCloseAll( itemDef_t *item, int handle )
{
  int i;
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Int_Parse( handle, &i ) )
    return qfalse;

  if( i )
    menu->window.flags |= WINDOW_DONTCLOSEALL;

  return qtrue;
}

qboolean MenuParse_onOpen( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Script_Parse( handle, &menu->onOpen ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_onClose( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Script_Parse( handle, &menu->onClose ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_onESC( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Script_Parse( handle, &menu->onESC ) )
    return qfalse;

  return qtrue;
}



qboolean MenuParse_border( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Int_Parse( handle, &menu->window.border ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_borderSize( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Float_Parse( handle, &menu->window.borderSize ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_backcolor( itemDef_t *item, int handle )
{
  int i;
  float f;
  menuDef_t *menu = ( menuDef_t* )item;

  for( i = 0; i < 4; i++ )
  {
    if( !PC_Float_Parse( handle, &f ) )
      return qfalse;

    menu->window.backColor[i]  = f;
  }

  return qtrue;
}

qboolean MenuParse_forecolor( itemDef_t *item, int handle )
{
  int i;
  float f;
  menuDef_t *menu = ( menuDef_t* )item;

  for( i = 0; i < 4; i++ )
  {
    if( !PC_Float_Parse( handle, &f ) )
      return qfalse;

    menu->window.foreColor[i]  = f;
    menu->window.flags |= WINDOW_FORECOLORSET;
  }

  return qtrue;
}

qboolean MenuParse_bordercolor( itemDef_t *item, int handle )
{
  int i;
  float f;
  menuDef_t *menu = ( menuDef_t* )item;

  for( i = 0; i < 4; i++ )
  {
    if( !PC_Float_Parse( handle, &f ) )
      return qfalse;

    menu->window.borderColor[i]  = f;
  }

  return qtrue;
}

qboolean MenuParse_focuscolor( itemDef_t *item, int handle )
{
  int i;
  float f;
  menuDef_t *menu = ( menuDef_t* )item;

  for( i = 0; i < 4; i++ )
  {
    if( !PC_Float_Parse( handle, &f ) )
      return qfalse;

    menu->focusColor[i]  = f;
  }

  return qtrue;
}

qboolean MenuParse_disablecolor( itemDef_t *item, int handle )
{
  int i;
  float f;
  menuDef_t *menu = ( menuDef_t* )item;

  for( i = 0; i < 4; i++ )
  {
    if( !PC_Float_Parse( handle, &f ) )
      return qfalse;

    menu->disableColor[i]  = f;
  }

  return qtrue;
}


qboolean MenuParse_outlinecolor( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Color_Parse( handle, &menu->window.outlineColor ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_background( itemDef_t *item, int handle )
{
  const char *buff;
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_String_Parse( handle, &buff ) )
    return qfalse;

  menu->window.background = DC->registerShaderNoMip( buff );
  return qtrue;
}

qboolean MenuParse_cinematic( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_String_Parse( handle, &menu->window.cinematicName ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_ownerdrawFlag( itemDef_t *item, int handle )
{
  int i;
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Int_Parse( handle, &i ) )
    return qfalse;

  menu->window.ownerDrawFlags |= i;
  return qtrue;
}

qboolean MenuParse_ownerdraw( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Int_Parse( handle, &menu->window.ownerDraw ) )
    return qfalse;

  return qtrue;
}


// decoration
qboolean MenuParse_popup( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;
  menu->window.flags |= WINDOW_POPUP;
  return qtrue;
}


qboolean MenuParse_outOfBounds( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  menu->window.flags |= WINDOW_OOB_CLICK;
  return qtrue;
}

qboolean MenuParse_soundLoop( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_String_Parse( handle, &menu->soundName ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_listenTo( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_String_Parse( handle, &menu->listenCvar ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_fadeClamp( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Float_Parse( handle, &menu->fadeClamp ) )
    return qfalse;

  return qtrue;
}

qboolean MenuParse_fadeAmount( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Float_Parse( handle, &menu->fadeAmount ) )
    return qfalse;

  return qtrue;
}


qboolean MenuParse_fadeCycle( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( !PC_Int_Parse( handle, &menu->fadeCycle ) )
    return qfalse;

  return qtrue;
}


qboolean MenuParse_itemDef( itemDef_t *item, int handle )
{
  menuDef_t *menu = ( menuDef_t* )item;

  if( menu->itemCount < MAX_MENUITEMS )
  {
    if(DC->hudloading)
      menu->items[menu->itemCount] = UI_HUDAlloc(sizeof(itemDef_t));
    else
      menu->items[menu->itemCount] = UI_Alloc(sizeof(itemDef_t));
    Item_Init( menu->items[menu->itemCount] );

    if( !Item_Parse( handle, menu->items[menu->itemCount] ) )
      return qfalse;

    Item_InitControls( menu->items[menu->itemCount] );
    menu->items[menu->itemCount++]->parent = menu;
  }
  else
  {
    PC_SourceError( handle, "itemDefs per menu may not exceed %d",
                    MAX_MENUITEMS );
    return qfalse;
  }

  return qtrue;
}

keywordHash_t menuParseKeywords[] = {
  {"font", MenuParse_font},
  {"name", MenuParse_name},
  {"fullscreen", MenuParse_fullscreen},
  {"rect", MenuParse_rect},
  {"aspectBias", MenuParse_aspectBias},
  {"style", MenuParse_style},
  {"visible", MenuParse_visible},
  {"dontCloseAll", MenuParse_dontCloseAll},
  {"onOpen", MenuParse_onOpen},
  {"onClose", MenuParse_onClose},
  {"onESC", MenuParse_onESC},
  {"border", MenuParse_border},
  {"borderSize", MenuParse_borderSize},
  {"backcolor", MenuParse_backcolor},
  {"forecolor", MenuParse_forecolor},
  {"bordercolor", MenuParse_bordercolor},
  {"focuscolor", MenuParse_focuscolor},
  {"disablecolor", MenuParse_disablecolor},
  {"outlinecolor", MenuParse_outlinecolor},
  {"background", MenuParse_background},
  {"ownerdraw", MenuParse_ownerdraw},
  {"ownerdrawFlag", MenuParse_ownerdrawFlag},
  {"outOfBoundsClick", MenuParse_outOfBounds},
  {"soundLoop", MenuParse_soundLoop},
  {"listento", MenuParse_listenTo},
  {"itemDef", MenuParse_itemDef},
  {"cinematic", MenuParse_cinematic},
  {"popup", MenuParse_popup},
  {"fadeClamp", MenuParse_fadeClamp},
  {"fadeCycle", MenuParse_fadeCycle},
  {"fadeAmount", MenuParse_fadeAmount},
  {NULL, voidFunction2}
};

keywordHash_t *menuParseKeywordHash[KEYWORDHASH_SIZE];

/*
===============
Menu_SetupKeywordHash
===============
*/
void Menu_SetupKeywordHash( void )
{
  int i;

  memset( menuParseKeywordHash, 0, sizeof( menuParseKeywordHash ) );

  for( i = 0; menuParseKeywords[ i ].keyword; i++ )
    KeywordHash_Add( menuParseKeywordHash, &menuParseKeywords[ i ] );
}

/*
===============
Menu_Parse
===============
*/
qboolean Menu_Parse( int handle, menuDef_t *menu )
{
  pc_token_t token;
  keywordHash_t *key;

  if( !trap_Parse_ReadToken( handle, &token ) )
    return qfalse;

  if( *token.string != '{' )
    return qfalse;

  while( 1 )
  {
    memset( &token, 0, sizeof( pc_token_t ) );

    if( !trap_Parse_ReadToken( handle, &token ) )
    {
      PC_SourceError( handle, "end of file inside menu\n" );
      return qfalse;
    }

    if( *token.string == '}' )
      return qtrue;

    key = KeywordHash_Find( menuParseKeywordHash, token.string );

    if( !key )
    {
      PC_SourceError( handle, "unknown menu keyword %s", token.string );
      continue;
    }

    if( !key->func( ( itemDef_t* )menu, handle ) )
    {
      PC_SourceError( handle, "couldn't parse menu keyword %s", token.string );
      return qfalse;
    }
  }

  return qfalse;
}

/*
===============
Menu_New
===============
*/
void Menu_New( int handle )
{
  menuDef_t *menu = &Menus[menuCount];

  if( menuCount < MAX_MENUS )
  {
    Menu_Init( menu );

    if( Menu_Parse( handle, menu ) )
    {
      Menu_PostParse( menu );
      menuCount++;
    }
  }
}

int Menu_Count( void )
{
  return menuCount;
}

void Menu_PaintAll( void )
{
  int i;

  if( g_editingField || g_waitingForKey )
    DC->setCVar( "ui_hideCursor", "1" );
  else
    DC->setCVar( "ui_hideCursor", "0" );

  if( captureFunc != voidFunction )
  {
    if( captureFuncExpiry > 0 && DC->realTime > captureFuncExpiry )
      UI_RemoveCaptureFunc( );
    else
      captureFunc( captureData );
  }

  for( i = 0; i < openMenuCount; i++ )
    Menu_Paint( menuStack[i], qfalse );

  if( DC->getCVarValue( "ui_developer" ) )
  {
    vec4_t v = {1, 1, 1, 1};
    UI_Text_Paint( 5, 25, .5, v, va( "fps: %f", DC->FPS ), 0, 0, 0 );
  }
}

void Menu_Reset( void )
{
  menuCount = 0;
}

displayContextDef_t *Display_GetContext( void )
{
  return DC;
}

void *Display_CaptureItem( int x, int y )
{
  int i;

  for( i = 0; i < menuCount; i++ )
  {
    if( Rect_ContainsPoint( &Menus[i].window.rect, x, y ) )
      return & Menus[i];
  }

  return NULL;
}


// FIXME:
qboolean Display_MouseMove( void *p, float x, float y )
{
  int i;
  menuDef_t *menu = p;

  if( menu == NULL )
  {
    menu = Menu_GetFocused();

    if( menu )
    {
      if( menu->window.flags & WINDOW_POPUP )
      {
        Menu_HandleMouseMove( menu, x, y );
        return qtrue;
      }
    }

    for( i = 0; i < menuCount; i++ )
      Menu_HandleMouseMove( &Menus[i], x, y );
  }
  else
  {
    menu->window.rect.x += x;
    menu->window.rect.y += y;
    Menu_UpdatePosition( menu );
  }

  return qtrue;

}

int Display_CursorType( int x, int y )
{
  int i;

  for( i = 0; i < menuCount; i++ )
  {
    rectDef_t r2;
    r2.x = Menus[i].window.rect.x - 3;
    r2.y = Menus[i].window.rect.y - 3;
    r2.w = r2.h = 7;

    if( Rect_ContainsPoint( &r2, x, y ) )
      return CURSOR_SIZER;
  }

  return CURSOR_ARROW;
}


void Display_HandleKey( int key, qboolean down, int x, int y )
{
  menuDef_t *menu = Display_CaptureItem( x, y );

  if( menu == NULL )
    menu = Menu_GetFocused();

  if( menu )
    Menu_HandleKey( menu, key, down );
}

static void Window_CacheContents( windowDef_t *window )
{
  if( window )
  {
    if( window->cinematicName )
    {
      int cin = DC->playCinematic( window->cinematicName, 0, 0, 0, 0 );
      DC->stopCinematic( cin );
    }
  }
}


static void Item_CacheContents( itemDef_t *item )
{
  if( item )
    Window_CacheContents( &item->window );

}

static void Menu_CacheContents( menuDef_t *menu )
{
  if( menu )
  {
    int i;
    Window_CacheContents( &menu->window );

    for( i = 0; i < menu->itemCount; i++ )
      Item_CacheContents( menu->items[i] );

    if( menu->soundName && *menu->soundName )
      DC->registerSound( menu->soundName, qfalse );
  }

}

void Display_CacheAll( void )
{
  int i;

  for( i = 0; i < menuCount; i++ )
    Menu_CacheContents( &Menus[i] );
}


static qboolean Menu_OverActiveItem( menuDef_t *menu, float x, float y )
{
  if( menu && menu->window.flags & ( WINDOW_VISIBLE | WINDOW_FORCED ) )
  {
    if( Rect_ContainsPoint( &menu->window.rect, x, y ) )
    {
      int i;

      for( i = 0; i < menu->itemCount; i++ )
      {
        if( !( menu->items[i]->window.flags & ( WINDOW_VISIBLE | WINDOW_FORCED ) ) )
          continue;

        if( menu->items[i]->window.flags & WINDOW_DECORATION )
          continue;

        if( Rect_ContainsPoint( &menu->items[i]->window.rect, x, y ) )
        {
          itemDef_t * overItem = menu->items[i];

          if( overItem->type == ITEM_TYPE_TEXT && overItem->text )
          {
            if( Rect_ContainsPoint( Item_CorrectedTextRect( overItem ), x, y ) )
              return qtrue;
            else
              continue;
          }
          else
            return qtrue;
        }
      }

    }
  }

  return qfalse;
}

