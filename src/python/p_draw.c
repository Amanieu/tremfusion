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
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "p_local.h"

typedef struct _drawstring {
        char *text;
        int x, y;
        float size, colour[4];
        qboolean forceColour, noColourEscape;
} drawstring;

#define DRAWING_CACHE_SIZE 20
static drawstring string_cache[DRAWING_CACHE_SIZE];
static int string_cache_size = 0;

/* Shutup the compiler */
void SCR_DrawStringExt(int, int, float, const char *, float *,
                       qboolean, qboolean);

static PyObject *draw_string(PyObject *self, PyObject *args, PyObject *kwargs) {
        PyObject *colour, *forceColour, *noColourEscape;
        drawstring *str;
        float alpha;
        static char *kwlist[] = {"text", "x", "y", "size", "colour", "alpha",
                                 "forceColour", "noColourEscape", NULL};
        
        /* Check if cache is overflowing and if not get a new drawstring struc*/
        if( string_cache_size > DRAWING_CACHE_SIZE)
                return NULL; //Raise ERROR
        str = &string_cache[string_cache_size];
        string_cache_size++;
        /* Default values */
        str->size = 8;
        colour = Py_None;
        alpha = -1.0;
        forceColour = Py_False;
        noColourEscape = Py_False;
        /* Parse tuples and keyward args */
        if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sii|fOfOO", kwlist, 
                                         &str->text, &str->x, &str->y,
                                         &str->size, &colour, &alpha, 
                                         &forceColour, &noColourEscape)) {
                string_cache_size--;
                return NULL;
        }
        /* Handle args */
        str->colour[0] = 1.0;
        str->colour[1] = 1.0;
        str->colour[2] = 1.0;
        if(alpha == -1.0)
                str->colour[3] = 1.0;
        else 
                str->colour[3] = alpha;
        str->forceColour = PyObject_IsTrue(forceColour);
        str->noColourEscape = PyObject_IsTrue(noColourEscape);
        
        Py_RETURN_NONE;
}

static PyMethodDef P_draw_methods[] =
{
 {"string", (PyCFunction)draw_string, METH_KEYWORDS,  "draw text on the screen for next second"},
 {NULL}      /* sentinel */
};


static int last_update_time = -1;

void P_Draw_Frame(void) {
        int i;
        drawstring *str;
        
        if(last_update_time == -1 || 
           (Sys_Milliseconds() - last_update_time) > 1000 ) {
                string_cache_size = 0;
                P_Event_Update_Draw();
                last_update_time = Sys_Milliseconds();
        }
        for (i=0; i < string_cache_size; i++) {
                str = &string_cache[i];
                SCR_DrawStringExt(string_cache[i].x, string_cache[i].y, 
                                  string_cache[i].size, string_cache[i].text, 
                                  string_cache[i].colour, 
                                  string_cache[i].forceColour, 
                                  string_cache[i].noColourEscape);
        }
}

void P_Draw_Init(void) {
        Py_InitModule("tremfusion.draw", P_draw_methods);
}

