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
}

/*
==================
Sys_GetPacket

Never called by the game logic, just the system event queing
==================
*/
qboolean	Sys_GetPacket ( netadr_t *net_from, msg_t *net_message ) {
	return qfalse;
}

qboolean  NET_CompareAdr (netadr_t a, netadr_t b) {
}
qboolean  NET_CompareBaseAdr (netadr_t a, netadr_t b){
}
qboolean  NET_IsLocalAddress (netadr_t adr) {
}
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
}

void NET_Init( void ) {
}

