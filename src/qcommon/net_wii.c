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

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#include <errno.h>
#include <network.h>
#include <sys/types.h>

typedef int SOCKET;
typedef unsigned short int sa_family_t;

# define socketError      errno

static SOCKET ip_socket = INVALID_SOCKET;

static cvar_t *net_enabled;
static cvar_t *net_ip;
static cvar_t *net_port;

char *NET_ErrorString( void ) {
  return strerror (errno);
}

static void SockadrToNetadr( struct sockaddr *s, netadr_t *a ) {
  if (s->sa_family == AF_INET) {
    a->type = NA_IP;
    *(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
    a->port = ((struct sockaddr_in *)s)->sin_port;
  }
}

static void NetadrToSockadr( netadr_t *a, struct sockaddr *s ) {
  if( a->type == NA_BROADCAST ) {
    ((struct sockaddr_in *)s)->sin_family = AF_INET;
    ((struct sockaddr_in *)s)->sin_port = a->port;
    ((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
  }
  else if( a->type == NA_IP ) {
    ((struct sockaddr_in *)s)->sin_family = AF_INET;
    ((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a->ip;
    ((struct sockaddr_in *)s)->sin_port = a->port;
  }
}

/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
qboolean   Sys_StringToAdr ( const char *s, netadr_t *a, netadrtype_t family)
{
  if (!strcmp (s, "localhost")) {
    memset (a, 0, sizeof(*a));
    a->type = NA_LOOPBACK;
    return qtrue;
  }

  return qfalse;
}

/*
==================
Sys_SendPacket
==================
*/
void  Sys_SendPacket( int length, const void *data, netadr_t to ) {
  int       ret = SOCKET_ERROR;
  struct sockaddr addr;
  
  if( to.type != NA_BROADCAST && to.type != NA_IP)
  {
    Com_Error( ERR_FATAL, "Sys_SendPacket: bad address type" );
    return;
  }
  
  if( (ip_socket == INVALID_SOCKET && to.type == NA_IP))
    return;
  
  memset(&addr, 0, sizeof(addr));
  NetadrToSockadr( &to, (struct sockaddr *) &addr );
  
//  if( usingSocks && to.type == NA_IP ) {
//    socksBuf[0] = 0;  // reserved
//    socksBuf[1] = 0;
//    socksBuf[2] = 0;  // fragment (not fragmented)
//    socksBuf[3] = 1;  // address type: IPV4
//    *(int *)&socksBuf[4] = ((struct sockaddr_in *)&addr)->sin_addr.s_addr;
//    *(short *)&socksBuf[8] = ((struct sockaddr_in *)&addr)->sin_port;
//    memcpy( &socksBuf[10], data, length );
//    ret = net_sendto( ip_socket, socksBuf, length+10, 0, &socksRelayAddr, sizeof(socksRelayAddr) );
//  }
//  else {
//    if(addr.ss_family == AF_INET)
      ret = net_sendto( ip_socket, data, length, 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_in) );
//  }
  if( ret == SOCKET_ERROR ) {
    int err = socketError;
    
    // wouldblock is silent
    if( err == EAGAIN ) {
      return;
    }
    
    // some PPP links do not allow broadcasts and return an error
    if( ( err == EADDRNOTAVAIL ) && ( ( to.type == NA_BROADCAST ) ) ) {
      return;
    }
    
    Com_Printf( "NET_SendPacket: %s\n", NET_ErrorString() );
  }
}

/*
==================
Sys_GetPacket

Never called by the game logic, just the system event queing
==================
*/
qboolean  Sys_GetPacket ( netadr_t *net_from, msg_t *net_message ) {
  int   ret;
  struct sockaddr_in from;
  socklen_t fromlen;
  int   err;

  if(ip_socket != INVALID_SOCKET)
  {
    fromlen = sizeof(from);
    ret = net_recvfrom( ip_socket, (void *)net_message->data, net_message->maxsize, 0, (struct sockaddr *) &from, &fromlen );
    if (ret == SOCKET_ERROR)
    {
      err = socketError;

      if( err != EAGAIN && err != ECONNRESET )
        Com_Printf( "NET_GetPacket: %s\n", NET_ErrorString() );
    }
    else if(ret < 0)
      return qfalse;
    else
    {

      memset( ((struct sockaddr_in *)&from)->sin_zero, 0, 8 );

//      if ( usingSocks && memcmp( &from, &socksRelayAddr, fromlen ) == 0 ) {
//        if ( ret < 10 || net_message->data[0] != 0 || net_message->data[1] != 0 || net_message->data[2] != 0 || net_message->data[3] != 1 ) {
//          return qfalse;
//        }
//        net_from->type = NA_IP;
//        net_from->ip[0] = net_message->data[4];
//        net_from->ip[1] = net_message->data[5];
//        net_from->ip[2] = net_message->data[6];
//        net_from->ip[3] = net_message->data[7];
//        net_from->port = *(short *)&net_message->data[8];
//        net_message->readcount = 10;
//      }
//      else {
        SockadrToNetadr( (struct sockaddr *) &from, net_from );
        net_message->readcount = 0;
//      }

      if( ret == net_message->maxsize ) {
        Com_Printf( "Oversize packet from %s\n", NET_AdrToString (*net_from) );
        return qfalse;
      }

      net_message->cursize = ret;
      return qtrue;
    }
  }
  return qfalse;
}

qboolean  NET_CompareAdr (netadr_t a, netadr_t b) {
  if(!NET_CompareBaseAdr(a, b))
    return qfalse;
  
  if (a.type == NA_IP)
  {
    if (a.port == b.port)
      return qtrue;
  }
  else
    return qtrue;
  
  return qfalse;
}
qboolean  NET_CompareBaseAdr (netadr_t a, netadr_t b){
  if (a.type != b.type)
    return qfalse;
  
  if (a.type == NA_LOOPBACK)
    return qtrue;
  
  if (a.type == NA_IP)
  {
    if(!memcmp(a.ip, b.ip, sizeof(a.ip)))
      return qtrue;
    
    return qfalse;
  }
  
  Com_Printf ("NET_CompareBaseAdr: bad address type\n");
  return qfalse;
}
qboolean  NET_IsLocalAddress (netadr_t adr) {
  int   index, run, addrsize;
  qboolean differed;
  byte *compareadr, *comparemask, *compareip;

  if( adr.type == NA_LOOPBACK ) {
    return qtrue;
  }

  if( adr.type == NA_IP )
  {
    // RFC1918:
    // 10.0.0.0        -   10.255.255.255  (10/8 prefix)
    // 172.16.0.0      -   172.31.255.255  (172.16/12 prefix)
    // 192.168.0.0     -   192.168.255.255 (192.168/16 prefix)
    if(adr.ip[0] == 10)
      return qtrue;
    if(adr.ip[0] == 172 && (adr.ip[1]&0xf0) == 16)
      return qtrue;
    if(adr.ip[0] == 192 && adr.ip[1] == 168)
      return qtrue;

    if(adr.ip[0] == 127)
      return qtrue;
  }
}

/*
=============
Sys_StringToSockaddr
=============
*/
static qboolean Sys_StringToSockaddr(const char *s, struct sockaddr *sadr, int sadr_len, sa_family_t family)
{}

const char *NET_AdrToString (netadr_t a) {
  return "loopback";
}
const char *NET_AdrToStringwPort (netadr_t a) {
  return "loopback";
}
void    NET_JoinMulticast6(void) {
}
void    NET_LeaveMulticast6(void) {
}

qboolean Sys_IsLANAddress( netadr_t adr ) {
  return qfalse;
}

void NET_Sleep( int msec ) {
  struct timeval timeout;
  fd_set  fdset;
  int highestfd = -1;
  
  if (!com_dedicated->integer)
    return; // we're not a server, just run full speed
  
  if (ip_socket == INVALID_SOCKET)
    return;
  
  if (msec < 0 )
    return;
  
  FD_ZERO(&fdset);
  
  FD_SET(ip_socket, &fdset);
  
  timeout.tv_sec = msec/1000;
  timeout.tv_usec = (msec%1000)*1000;
  net_select(ip_socket+1, &fdset, NULL, NULL, &timeout);
}

int NET_IPSocket( char *net_interface, int port, int *err ) {
  SOCKET        newsocket;
  struct sockaddr_in  address;
  u_long        _true = 1;
  int         i = 1;

  *err = 0;

  if( net_interface ) {
    Com_Printf( "Opening IP socket: %s:%i\n", net_interface, port );
  }
  else {
    Com_Printf( "Opening IP socket: 0.0.0.0:%i\n", port );
  }
  Sys_Hold();
  if( ( newsocket = net_socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) == INVALID_SOCKET ) {
    *err = socketError;
    Com_Printf( "WARNING: NET_IPSocket: socket: %s\n", NET_ErrorString() );
    return newsocket;
  }
  Com_Printf("Socket Openned\n");
  Com_Printf("Setting socket non blocking...");
//  Sys_Hold();
  // make it non-blocking
  if( net_ioctl( newsocket, FIONBIO, &_true ) == SOCKET_ERROR ) {
    Com_Printf( "WARNING: NET_IPSocket: ioctl FIONBIO: %s\n", NET_ErrorString() );
    *err = socketError;
    net_close(newsocket);
    return INVALID_SOCKET;
  }
  Com_Printf("Done\n");
  Com_Printf("Setting broadcast opt..");
//  Sys_Hold();
  // make it broadcast capable
  if( net_setsockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *) &i, sizeof(i) ) == SOCKET_ERROR ) {
    Com_Printf( "WARNING: NET_IPSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString() );
//     it is not that bad if this one fails.
    return newsocket;
  }
  Com_Printf("Done\n");

  if( !net_interface || !net_interface[0]) {
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
  }
  else
  {
    if(!Sys_StringToSockaddr( net_interface, (struct sockaddr *)&address, sizeof(address), AF_INET))
    {
      Com_Printf("Error: Sys_StringToSockaddr returned false, closing socket\n");
//      Sys_Hold();
      net_close(newsocket);
      return INVALID_SOCKET;
    }
  }

  if( port == PORT_ANY ) {
    address.sin_port = 0;
  }
  else {
    address.sin_port = htons( (short)port );
  }
  Com_Printf("Binding port...");
//  Sys_Hold();
  if( net_bind( newsocket, (struct sockaddr *)&address, sizeof(address) ) == SOCKET_ERROR ) {
    Com_Printf("Failed\n");
    Sys_Hold();
    Com_Printf( "WARNING: NET_IPSocket: bind: %s\n", NET_ErrorString() );
    *err = socketError;
    net_close( newsocket );
    return INVALID_SOCKET;
  }
  Com_Printf("Done\n Socket Created and bound\n");
//  Sys_Hold();
  

  return newsocket;
}

/*
====================
NET_OpenIP
====================
*/
void NET_OpenIP( void ) {
  int   i;
  int   err;
  int   port;

  net_ip = Cvar_Get( "net_ip", "0.0.0.0", CVAR_LATCH );
  net_port = Cvar_Get( "net_port", va( "%i", PORT_SERVER ), CVAR_LATCH );
  net_enabled = Cvar_Get( "net_enabled", "1", CVAR_LATCH | CVAR_ARCHIVE );

  port = net_port->integer;

  // automatically scan for a valid port, so multiple
  // dedicated servers can be started without requiring
  // a different net_port for each one

  if(net_enabled->integer)
  {
    for( i = 0 ; i < 10 ; i++ ) {
      ip_socket = NET_IPSocket( net_ip->string, port + i, &err );
      if (ip_socket != INVALID_SOCKET) {
        Cvar_SetValue( "net_port", port + i );
        break;
      }
      else
      {
        if(err == EAFNOSUPPORT)
          break;
      }
    }

    if(ip_socket == INVALID_SOCKET)
      Com_Printf( "WARNING: Couldn't bind to a v4 ip address.\n");
  }
}

void NET_Init( void ) {
  int i, ret;
  
  u32 ip;
  u8 *splitip = (u8 *)&ip;
  printf("Starting Net_Init()...\n");
  for(i=0; i<10; i++) {
    ret = net_init();
    if(ret < 0) {
      printf("Net_Init() failed. Press A to retry, Home to quit.\n");
      Sys_Hold();
      continue;
    }
    else
      break;
  }
  printf("Net_Init() successful.\n");
  
  ip = net_gethostip();
  printf("Ip address is %d.%d.%d.%d\n", splitip[0], splitip[1], splitip[2], splitip[3]);
  NET_OpenIP();
}

