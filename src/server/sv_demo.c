/*
===========================================================================
Copyright (C) 2008 Amanieu d'Antras

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

// sv_demo.c -- Server side demo recording

#include "server.h"

// Headers for demo messages
typedef enum {
	demo_endFrame,
	demo_configString,
	demo_serverCommand,
	demo_entityState,
	demo_entityShared,
	demo_playerState,
	demo_EOF
} demo_ops_e;

// Big fat buffer to store all our stuff
byte buf[0x400000];

/*
====================
SV_DemoWriteMessage

Write a message to the demo file
====================
*/
static void SV_DemoWriteMessage(msg_t *msg)
{
	int len;

	// Write the entire message to the file, prefixed by the length
	MSG_WriteByte(msg, demo_EOF);
	len = LittleLong(msg->cursize);
	FS_Write(&len, 4, sv.demoFile);
	FS_Write(msg->data, msg->cursize, sv.demoFile);
	MSG_Clear(msg);
}

/*
====================
SV_DemoWriteServerCommand

Write a server command to the demo file
====================
*/
void SV_DemoWriteServerCommand(const char *str)
{
	msg_t msg;

	MSG_Init(&msg, buf, sizeof(buf));
	MSG_WriteByte(&msg, demo_serverCommand);
	MSG_WriteString(&msg, str);
	SV_DemoWriteMessage(&msg);
}

/*
====================
SV_DemoWriteConfigString

Write a client configstring to the demo file
====================
*/
void SV_DemoWriteConfigString(int client)
{
	msg_t msg;

	MSG_Init(&msg, buf, sizeof(buf));
	MSG_WriteByte(&msg, demo_configString);
	MSG_WriteBits(&msg, client, CLIENTNUM_BITS);
	MSG_WriteString(&msg, sv.configstrings[CS_PLAYERS + client]);
	SV_DemoWriteMessage(&msg);
}

/*
====================
SV_DemoWriteFrame

Record all the entities and players at the end the frame
====================
*/
void SV_DemoWriteFrame(void)
{
	msg_t msg;
	playerState_t *player;
	sharedEntity_t *entity;
	int i;

	MSG_Init(&msg, buf, sizeof(buf));

	// Write entities
	MSG_WriteByte(&msg, demo_entityState);
	for (i = 0; i < MAX_GENTITIES; i++)
	{
		entity = SV_GentityNum(i);
		MSG_WriteDeltaEntity(&msg, &sv.demoEntities[i].s, &entity->s, qfalse);
		sv.demoEntities[i].s = entity->s;
	}
	MSG_WriteBits(&msg, ENTITYNUM_NONE, GENTITYNUM_BITS);
	MSG_WriteByte(&msg, demo_entityShared);
	for (i = 0; i < MAX_GENTITIES; i++)
	{
		entity = SV_GentityNum(i);
		MSG_WriteDeltaSharedEntity(&msg, &sv.demoEntities[i].r, &entity->r, qfalse, i);
		sv.demoEntities[i].r = entity->r;
	}
	MSG_WriteBits(&msg, ENTITYNUM_NONE, GENTITYNUM_BITS);
	SV_DemoWriteMessage(&msg);

	// Write clients
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (svs.clients[i].state < CS_ACTIVE)
			continue;
		player = SV_GameClientNum(i);
		MSG_WriteByte(&msg, demo_playerState);
		MSG_WriteBits(&msg, i, CLIENTNUM_BITS);
		MSG_WriteDeltaPlayerstate(&msg, &sv.demoPlayerStates[i], player);
		sv.demoPlayerStates[i] = *player;
	}
	MSG_WriteByte(&msg, demo_endFrame);
	SV_DemoWriteMessage(&msg);
}

/*
====================
SV_DemoReadFrame

Play a frame from the demo file
====================
*/
void SV_DemoReadFrame(void)
{
	msg_t msg;
	int cmd, r, num;
	playerState_t *player;
	sharedEntity_t *entity;

	MSG_Init(&msg, buf, sizeof(buf));

	while (1)
	{
exit_loop:
		// Get a message
		r = FS_Read(&msg.cursize, 4, sv.demoFile);
		if (r != 4)
		{
			SV_DemoStopPlayback();
			return;
		}
		msg.cursize = LittleLong(msg.cursize);
		if (msg.cursize == -1)
		{
			SV_DemoStopPlayback();
			return;
		}
		if (msg.cursize > msg.maxsize)
			Com_Error(ERR_DROP, "SV_DemoReadFrame: demo message too long");
		r = FS_Read(msg.data, msg.cursize, sv.demoFile);
		if (r != msg.cursize)
		{
			Com_Printf("Demo file was truncated.\n");
			SV_DemoStopPlayback();
			return;
		}

		// Parse the message
		while (1)
		{
			cmd = MSG_ReadByte(&msg);
			switch (cmd)
			{
			default:
				Com_Error(ERR_DROP, "SV_DemoReadFrame: Illegible demo message\n");
				return;	
			case demo_EOF:
				MSG_Clear(&msg);
				goto exit_loop;
			case demo_endFrame:
				return;
			case demo_configString:
				num = MSG_ReadBits(&msg, CLIENTNUM_BITS);
				SV_SetConfigstring(CS_PLAYERS + num, MSG_ReadString(&msg));
				break;
			case demo_serverCommand:
				SV_SendServerCommand(NULL, "%s", MSG_ReadString);
				break;
			case demo_playerState:
				num = MSG_ReadBits(&msg, CLIENTNUM_BITS);
				player = SV_GameClientNum(num);
				MSG_ReadDeltaPlayerstate(&msg, &sv.demoPlayerStates[num], player);
				sv.demoPlayerStates[num] = *player;
				break;
			case demo_entityState:
				while (1)
				{
					num = MSG_ReadBits(&msg, GENTITYNUM_BITS);
					if (num == ENTITYNUM_NONE)
						break;
					entity = SV_GentityNum(num);
					MSG_ReadDeltaEntity(&msg, &sv.demoEntities[num].s, &entity->s, num);
					sv.demoEntities[num].s = entity->s;
				}
				break;
			case demo_entityShared:
				while (1)
				{
					num = MSG_ReadBits(&msg, GENTITYNUM_BITS);
					if (num == ENTITYNUM_NONE)
						break;
					entity = SV_GentityNum(num);
					MSG_ReadDeltaSharedEntity(&msg, &sv.demoEntities[num].r, &entity->r, num);

					// Link/unlink the entity
					if ((entity->r.linked && !sv.demoEntities[num].r.linked) ||
					    (entity->r.linkcount != sv.demoEntities[num].r.linkcount))
						SV_LinkEntity(entity);
					else if (!entity->r.linked && sv.demoEntities[num].r.linked)
						SV_UnlinkEntity(entity);

					sv.demoEntities[num].r = entity->r;
				}
				break;
			}
		}
	}
}

/*
====================
SV_DemoStartRecord

sv.demo* have already been set and the demo file opened, start writing gamestate info
====================
*/
void SV_DemoStartRecord(void)
{
	msg_t msg;
	int i;

	MSG_Init(&msg, buf, sizeof(buf));

	// Write number of clients (sv_maxclients < MAX_CLIENTS or else we can't playback)
	MSG_WriteBits(&msg, sv_maxclients->integer, CLIENTNUM_BITS);
	// Write map name
	MSG_WriteString(&msg, sv_mapname->string);
	SV_DemoWriteMessage(&msg);

	// Write client configstrings
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (svs.clients[i].state == CS_ACTIVE && sv.configstrings[CS_PLAYERS + i])
			SV_DemoWriteConfigString(i);
	}
	SV_DemoWriteMessage(&msg);

	// Write entities and players
	Com_Memset(sv.demoEntities, 0, sizeof(sv.demoEntities));
	Com_Memset(sv.demoPlayerStates, 0, sizeof(sv.demoPlayerStates));
	SV_DemoWriteFrame();
	Com_Printf("Recording demo %s", sv.demoName);
}

/*
====================
SV_DemoStopRecord

Write end of file and close the demo file
====================
*/
void SV_DemoStopRecord(void)
{
	msg_t msg;

	// End the frame
	MSG_Init(&msg, buf, sizeof(buf));
	MSG_WriteByte(&msg, demo_endFrame);
	SV_DemoWriteMessage(&msg);

	FS_FCloseFile(sv.demoFile);
	sv.demoState = DS_NONE;
	Com_Printf("Stopped recording %s", sv.demoName);
}

/*
====================
SV_DemoStartPlayback

sv.demo* have already been set and the demo file opened, start reading gamestate info
====================
*/
void SV_DemoStartPlayback(void)
{
	msg_t msg;
	int r;
	char *s;

	MSG_Init(&msg, buf, sizeof(buf));

	// Get the demo header
	r = FS_Read(&msg.cursize, 4, sv.demoFile);
	if (r != 4)
	{
		SV_DemoStopPlayback();
		return;
	}
	msg.cursize = LittleLong(msg.cursize);
	if (msg.cursize == -1)
	{
		SV_DemoStopPlayback();
		return;
	}
	if (msg.cursize > msg.maxsize)
		Com_Error(ERR_DROP, "SV_DemoReadFrame: demo message too long");
	r = FS_Read(msg.data, msg.cursize, sv.demoFile);
	if (r != msg.cursize)
	{
		Com_Printf("Demo file was truncated.\n");
		SV_DemoStopPlayback();
		return;
	}

	// Check slots and map
	if (sv_democlients->integer < MSG_ReadBits(&msg, CLIENTNUM_BITS))
	{
		Com_Printf("Not enough demo slots, increase sv_democlients.\n");
		SV_DemoStopPlayback();
		return;
	}
	s = MSG_ReadString(&msg);
	if (strcmp(sv_mapname->string, s))
	{
		Com_Printf("Map does not match demo map: %s.\n", s);
		SV_DemoStopPlayback();
		return;
	}

	// Initialize our stuff
	Com_Memset(sv.demoEntities, 0, sizeof(sv.demoEntities));
	Com_Memset(sv.demoPlayerStates, 0, sizeof(sv.demoPlayerStates));
	SV_DemoReadFrame();
	Com_Printf("Playing demo %s", sv.demoName);
}

/*
====================
SV_DemoStopPlayback

Close the demo file
====================
*/
void SV_DemoStopPlayback(void)
{
	FS_FCloseFile(sv.demoFile);
	sv.demoState = DS_NONE;
	Com_Printf("Stopped playing %s", sv.demoName);
}
