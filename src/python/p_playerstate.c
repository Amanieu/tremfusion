/*
===========================================================================
Copyright (C) 2008 John Black

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

#include "p_local.h"
#include <stddef.h>
#ifndef DEDICATED
#include "../client/client.h"
#else
#include "../server/server.h"
#endif

/*
 * p_playerstate.c
 * Python Object allowing read only access to the playerstate struct
 * */

typedef struct {
  PyObject_HEAD
  playerState_t *playerstate;
} PlayerState;

static void PlayerState_dealloc(PlayerState* self)
{
  self->ob_type->tp_free((PyObject*)self);
}

static PyObject *PlayerState_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PlayerState *self;

  self = (PlayerState *)type->tp_alloc(type, 0);

  return (PyObject *)self;
}

static int PlayerState_init(PlayerState *self, PyObject *args, PyObject *kwds)
{
#ifdef DEDICATED
  int player;
  if(!PyArg_ParseTuple(args, "i", &player))
    return -1;
  self->playerstate = SV_GameClientNum(player);
#else
  self->playerstate = &cl.snap.ps;
#endif

  return 0;
}


static PyObject *get_int(PlayerState *self, int offset)
{
  return Py_BuildValue("i", *(int*)((void*)self->playerstate + offset) );
}

static int set_int(PlayerState *self, PyObject *value, int offset)
{
  PyErr_SetString(PyExc_AttributeError, "Read Only");
  return -1;
}

static PyObject *get_vec3(PlayerState *self, int offset)
{
  int i;
  float *vec;
  PyObject *array;

  array = PyTuple_New(3);
  vec = (float*)((void*)self->playerstate + offset);

  for(i=0; i < 3; i++) {
    PyTuple_SET_ITEM(array, i, PyFloat_FromDouble(vec[i]));
  }

  return array;
}

static int set_vec3(PlayerState *self, PyObject *value, int offset)
{
  PyErr_SetString(PyExc_AttributeError, "Read Only");
  return -1;
}

static PyObject *get_intarray(PlayerState *self, int closure)
{
  int i, offset, size;
  PyObject *tuple;
  int *array;

  offset = (closure >> 4);
  size = (closure & 0x0f) + 1;

  tuple = PyTuple_New(size);
  array = (int*)((void*)self->playerstate + offset);

  for(i=0; i < size; i++) {
    PyTuple_SET_ITEM(tuple, i, PyInt_FromLong(array[i]));
  }

  return tuple;
}

static int set_intarray(PlayerState *self, PyObject *value, void *closure)
{
  PyErr_SetString(PyExc_AttributeError, "Read Only");
  return -1;
}

/* Doc strings */
PyDoc_STRVAR(delta_angles_doc, "add to command angles to get view direction \nchanged by spawns, rotating objects, and teleporters");

/* Use macros to make the next part much cleaner */
#define ps_offsetof(x) offsetof(playerState_t, x)
#define ps_getsetint(x, d) {#x, (getter)get_int, (setter)set_int, d, (void*)(ps_offsetof(x))}
#define ps_getsetintarray(x, s, d) {#x, (getter)get_intarray, (setter)set_intarray, d, (void*)(ps_offsetof(x) << 4) + (s - 1) }
#define ps_getsetvec3t(x, d) {#x, (getter)get_vec3, (setter)set_vec3, d, (void*)(ps_offsetof(x))}

static PyGetSetDef PlayerState_getseters[] =
{
    ps_getsetint(commandTime, "cmd->serverTime of last executed command"),
    ps_getsetint(pm_type, ""),
    ps_getsetint(bobCycle, "for view bobbing and footstep generation"),
    ps_getsetint(pm_flags, "ducked, jump_held, etc"),
    ps_getsetint(pm_time, ""),

    ps_getsetvec3t(origin, ""),
    ps_getsetvec3t(velocity, ""),
    ps_getsetint(weaponTime, ""),
    ps_getsetint(gravity, ""),
    ps_getsetint(speed, ""),
    ps_getsetintarray(delta_angles, 3, delta_angles_doc),

    ps_getsetint(groundEntityNum, "ENTITYNUM_NONE = in air"),

    ps_getsetint(legsTimer, "don't change low priority animations until this runs out"),
    ps_getsetint(legsAnim, "mask off ANIM_TOGGLEBIT"),

    ps_getsetint(torsoTimer, "don't change low priority animations until this runs out"),
    ps_getsetint(torsoAnim, "mask off ANIM_TOGGLEBIT"),

    ps_getsetint(movementDir, ""),

    ps_getsetvec3t(grapplePoint, "location of grapple to pull towards if PMF_GRAPPLE_PULL"),

    ps_getsetint(eFlags, "copied to entityState_t->eFlags"),
    ps_getsetint(eventSequence, "pmove generated events"),
    ps_getsetintarray(events, MAX_PS_EVENTS, ""),
    ps_getsetintarray(eventParms, MAX_PS_EVENTS, ""),
    ps_getsetint(externalEvent, "events set on player from another source"),
    ps_getsetint(externalEventParm, ""),
    ps_getsetint(externalEventTime, ""),
    ps_getsetint(clientNum, "ranges from 0 to MAX_CLIENTS-1"),
    ps_getsetint(weapon, "copied to entityState_t->weapon"),
    ps_getsetint(weaponstate, ""),

    ps_getsetvec3t(viewangles, "for fixed views"),
    ps_getsetint(viewheight, ""),

    ps_getsetint(damageEvent, "when it changes, latch the other parms"),
    ps_getsetint(damageYaw, ""),
    ps_getsetint(damagePitch, ""),
    ps_getsetint(damageCount, ""),

    ps_getsetintarray(stats, MAX_STATS, ""),
    ps_getsetintarray(persistant, MAX_PERSISTANT, ""),
    ps_getsetintarray(misc, MAX_MISC, ""),
    ps_getsetint(ammo, ""),
    ps_getsetint(clips, ""),

    ps_getsetintarray(ammo_extra, 14, ""),

    ps_getsetint(generic1, ""),
    ps_getsetint(loopSound, ""),
    ps_getsetint(otherEntityNum, ""),

    ps_getsetint(ping, "server to game info for scoreboard"),
    ps_getsetint(pmove_framecount, ""),
    ps_getsetint(jumppad_frame, ""),
    ps_getsetint(entityEventSequence, ""),
    {NULL}  /* Sentinel */
};

static PyTypeObject PlayerStateType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "noddy.PlayerState",       /*tp_name*/
    sizeof(PlayerState),       /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PlayerState_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "PlayerState objects",       /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    PlayerState_getseters,     /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PlayerState_init,  /* tp_init */
    0,                         /* tp_alloc */
    PlayerState_new,             /* tp_new */
};


void P_Init_PlayerState(PyObject *module)
{
    if (PyType_Ready(&PlayerStateType) < 0)
        return;

    Py_INCREF(&PlayerStateType);
    PyModule_AddObject(module, "PlayerState", (PyObject *)&PlayerStateType);
}
