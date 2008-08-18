/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_shade_calc.c
#include "tr_local.h"

#define	WAVEVALUE( table, base, amplitude, phase, freq )  ((base) + table[ myftol( ( ( (phase) + tess.shaderTime * (freq) ) * FUNCTABLE_SIZE ) ) & FUNCTABLE_MASK ] * (amplitude))

static float   *TableForFunc(genFunc_t func)
{
	switch (func)
	{
		case GF_SIN:
			return tr.sinTable;
		case GF_TRIANGLE:
			return tr.triangleTable;
		case GF_SQUARE:
			return tr.squareTable;
		case GF_SAWTOOTH:
			return tr.sawToothTable;
		case GF_INVERSE_SAWTOOTH:
			return tr.inverseSawToothTable;
		case GF_NONE:
		default:
			break;
	}

	ri.Error(ERR_DROP, "TableForFunc called with invalid function '%d' in shader '%s'\n", func, tess.surfaceShader->name);
	return NULL;
}

/*
** EvalWaveForm
**
** Evaluates a given waveForm_t, referencing backEnd.refdef.time directly
*/
static float EvalWaveForm(const waveForm_t * wf)
{
	float          *table;

	table = TableForFunc(wf->func);

	return WAVEVALUE(table, wf->base, wf->amplitude, wf->phase, wf->frequency);
}

static float EvalWaveFormClamped(const waveForm_t * wf)
{
	float           glow = EvalWaveForm(wf);

	if(glow < 0)
	{
		return 0;
	}

	if(glow > 1)
	{
		return 1;
	}

	return glow;
}

static float GetOpValue(const expOperation_t * op)
{
	float			value;
	float			inv255 = 1.0f / 255.0f;
	
	switch(op->type)
	{
		case OP_NUM:
			value = op->value;
			break;
		
		case OP_TIME:
			value = tess.shaderTime;
			break;
			
		case OP_PARM0:
			if(backEnd.currentLight)
			{
				value = backEnd.currentLight->l.color[0];
				break;
			}
			else if(backEnd.currentEntity)
			{
				value = backEnd.currentEntity->e.shaderRGBA[0] * inv255;
			}
			else
			{
				value = 1.0;
			}
			break;
			
		case OP_PARM1:
			if(backEnd.currentLight)
			{
				value = backEnd.currentLight->l.color[1];
				break;
			}
			else if(backEnd.currentEntity)
			{
				value = backEnd.currentEntity->e.shaderRGBA[1] * inv255;
			}
			else
			{
				value = 1.0;
			}
			break;
			
		case OP_PARM2:
			if(backEnd.currentLight)
			{
				value = backEnd.currentLight->l.color[2];
				break;
			}
			else if(backEnd.currentEntity)
			{
				value = backEnd.currentEntity->e.shaderRGBA[2] * inv255;
			}
			else
			{
				value = 1.0;
			}
			break;
			
		case OP_PARM3:
			if(backEnd.currentLight)
			{
				value = 1.0;
				break;
			}
			else if(backEnd.currentEntity)
			{
				value = backEnd.currentEntity->e.shaderRGBA[3] * inv255;
			}
			else
			{
				value = 1.0;
			}
			break;
		
		case OP_PARM4:
		case OP_PARM5:
		case OP_PARM6:
		case OP_PARM7:
		case OP_PARM8:
		case OP_PARM9:
		case OP_PARM10:
		case OP_PARM11:
		case OP_GLOBAL0:
		case OP_GLOBAL1:
		case OP_GLOBAL2:
		case OP_GLOBAL3:
		case OP_GLOBAL4:
		case OP_GLOBAL5:
		case OP_GLOBAL6:
		case OP_GLOBAL7:
			value = 1.0;
			break;
			
		case OP_FRAGMENTPROGRAMS:
			value = glConfig2.shadingLanguage100Available;
			break;
			
		case OP_SOUND:
			value = 0.5;
			break;
		
		default:
			value = 0.0;
			break;
	}
	
	return value;
}

float RB_EvalExpression(const expression_t * exp, float defaultValue)
{
#if 1
	int				i;
	expOperation_t  op;
	expOperation_t  ops[MAX_EXPRESSION_OPS];
	int				numOps;
	float			value;
	float			value1;
	float			value2;
	extern const opstring_t opStrings[];
	
	numOps = 0;
	value = 0;
	value1 = 0;
	value2 = 0;
	
	if(!exp || !exp->active)
	{
		return defaultValue;	
	}
	
	// http://www.qiksearch.com/articles/cs/postfix-evaluation/
	// http://www.kyz.uklinux.net/evaluate/
	
	for(i = 0; i < exp->numOps; i++)
	{
		op = exp->ops[i];
		
		switch(op.type)
		{
			case OP_BAD:
				return defaultValue;
				
			case OP_NEG:
			{
				if(numOps < 1)
				{
					ri.Printf(PRINT_ALL, "WARNING: shader %s has numOps < 1 for unary - operator\n", tess.surfaceShader->name);
					return defaultValue;
				}
				
				value1 = GetOpValue(&ops[numOps -1]);
				numOps--;
				
				value = -value1;
				
				// push result
				op.type = OP_NUM;
				op.value = value;
				ops[numOps++] = op;
				break;	
			}
					
			case OP_NUM:
			case OP_TIME:
			case OP_PARM0:
			case OP_PARM1:
			case OP_PARM2:
			case OP_PARM3:
			case OP_PARM4:
			case OP_PARM5:
			case OP_PARM6:
			case OP_PARM7:
			case OP_PARM8:
			case OP_PARM9:
			case OP_PARM10:
			case OP_PARM11:
			case OP_GLOBAL0:
			case OP_GLOBAL1:
			case OP_GLOBAL2:
			case OP_GLOBAL3:
			case OP_GLOBAL4:
			case OP_GLOBAL5:
			case OP_GLOBAL6:
			case OP_GLOBAL7:
			case OP_FRAGMENTPROGRAMS:
			case OP_SOUND:
				ops[numOps++] = op;
				break;
				
			case OP_TABLE:
			{
				shaderTable_t  *table;
				int				numValues;
				float			index;
				float			lerp;
				int				oldIndex;
				int				newIndex;
				
				if(numOps < 1)
				{
					ri.Printf(PRINT_ALL, "WARNING: shader %s has numOps < 1 for table operator\n", tess.surfaceShader->name);
					return defaultValue;
				}
				
				value1 = GetOpValue(&ops[numOps -1]);
				numOps--;
				
				table = tr.shaderTables[(int)op.value];
				
				numValues = table->numValues;
	
				index = value1 * numValues;		// float index into the table?s elements
				lerp = index - floor(index);	// being inbetween two elements of the table
	
				oldIndex = (int)index;
				newIndex = (int)index + 1;
	
				if(table->clamp)
				{
					// clamp indices to table-range
					Q_clamp(oldIndex, 0, numValues-1);
					Q_clamp(newIndex, 0, numValues-1);
				}
				else
				{
					// wrap around indices
					oldIndex %= numValues;
					newIndex %= numValues;
				}

				if(table->snap)
				{
					// use fixed value
					value = table->values[oldIndex];
				}
				else
				{
					// lerp value
					value = table->values[oldIndex] + ((table->values[newIndex] - table->values[oldIndex]) * lerp);
				}
				
				//ri.Printf(PRINT_ALL, "%s: %i %i %f\n", table->name, oldIndex, newIndex, value);

				// push result
				op.type = OP_NUM;
				op.value = value;
				ops[numOps++] = op;
				break;
			}
				
			default:
			{
				if(numOps < 2)
				{
					ri.Printf(PRINT_ALL, "WARNING: shader %s has numOps < 2 for binary operator %s\n", tess.surfaceShader->name, opStrings[op.type].s);
					return defaultValue;
				}
				
				value2 = GetOpValue(&ops[numOps -1]);
				numOps--;
				
				value1 = GetOpValue(&ops[numOps -1]);
				numOps--;
				
				switch(op.type)
				{
					case OP_LAND:
						value = value1 && value2;
						break;
					
					case OP_LOR:
						value = value1 || value2;
						break;
						
					case OP_GE:
						value = value1 >= value2;
						break;
					
					case OP_LE:
						value = value1 <= value2;
						break;
					
					case OP_LEQ:
						value = value1 == value2;
						break;
						
					case OP_LNE:
						value = value1 != value2;
						break;
					
					case OP_ADD:
						value = value1 + value2;
						break;
						
					case OP_SUB:
						value = value1 - value2;
						break;
						
					case OP_DIV:
						if(value2 == 0)
						{
							// don't divide by zero
							value = value1;
						}
						else
						{
							value = value1 / value2;
						}
						break;
						
					case OP_MOD:
						value = (float)((int)value1 % (int)value2);
						break;
						
					case OP_MUL:
						value = value1 * value2;
						break;
						
					case OP_LT:
						value = value1 < value2;
						break;

					case OP_GT:
						value = value1 > value2;
						break;

					default:
						value = value1 = value2 = 0;
						break;
				}
			
				//ri.Printf(PRINT_ALL, "%s: %f %f %f\n", opStrings[op.type].s, value, value1, value2);
				
				// push result
				op.type = OP_NUM;
				op.value = value;
				ops[numOps++] = op;
				break;
			}
		}
	}
	
	return GetOpValue(&ops[0]);
#else
	return defaultValue;
#endif
}

/*
** RB_CalcStretchTexCoords
*/
void RB_CalcStretchTexCoords(const waveForm_t * wf, float *st)
{
	float           p;
	texModInfo_t    tmi;

	p = 1.0f / EvalWaveForm(wf);

	tmi.matrix[0][0] = p;
	tmi.matrix[1][0] = 0;
	tmi.translate[0] = 0.5f - 0.5f * p;

	tmi.matrix[0][1] = 0;
	tmi.matrix[1][1] = p;
	tmi.translate[1] = 0.5f - 0.5f * p;

	RB_CalcTransformTexCoords(&tmi, st);
}

/*
====================================================================

DEFORMATIONS

====================================================================
*/

/*
========================
RB_CalcDeformVertexes

========================
*/
void RB_CalcDeformVertexes(deformStage_t * ds)
{
	int             i;
	vec3_t          offset;
	float           scale;
	float          *xyz = (float *)tess.xyz;
	float          *normal = (float *)tess.normals;
	float          *table;

	if(ds->deformationWave.frequency == 0)
	{
		scale = EvalWaveForm(&ds->deformationWave);

		for(i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4)
		{
			VectorScale(normal, scale, offset);

			xyz[0] += offset[0];
			xyz[1] += offset[1];
			xyz[2] += offset[2];
		}
	}
	else
	{
		table = TableForFunc(ds->deformationWave.func);

		for(i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4)
		{
			float           off = (xyz[0] + xyz[1] + xyz[2]) * ds->deformationSpread;

			scale = WAVEVALUE(table, ds->deformationWave.base,
							  ds->deformationWave.amplitude,
							  ds->deformationWave.phase + off, ds->deformationWave.frequency);

			VectorScale(normal, scale, offset);

			xyz[0] += offset[0];
			xyz[1] += offset[1];
			xyz[2] += offset[2];
		}
	}
}

/*
=========================
RB_CalcDeformNormals

Wiggle the normals for wavy environment mapping
=========================
*/
void RB_CalcDeformNormals(deformStage_t * ds)
{
	int             i;
	float           scale;
	float          *xyz = (float *)tess.xyz;
	float          *normal = (float *)tess.normals;

	for(i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4)
	{
		scale = 0.98f;
		scale = R_NoiseGet4f(xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
							 tess.shaderTime * ds->deformationWave.frequency);
		normal[0] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f(100 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
							 tess.shaderTime * ds->deformationWave.frequency);
		normal[1] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f(200 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
							 tess.shaderTime * ds->deformationWave.frequency);
		normal[2] += ds->deformationWave.amplitude * scale;

		VectorNormalizeFast(normal);
	}
}

/*
========================
RB_CalcBulgeVertexes

========================
*/
void RB_CalcBulgeVertexes(deformStage_t * ds)
{
	int             i;
	const float    *st = (const float *)tess.texCoords[0];
	float          *xyz = (float *)tess.xyz;
	float          *normal = (float *)tess.normals;
	float           now;

	now = backEnd.refdef.time * ds->bulgeSpeed * 0.001f;

	for(i = 0; i < tess.numVertexes; i++, xyz += 4, st += 4, normal += 4)
	{
		int             off;
		float           scale;

		off = (float)(FUNCTABLE_SIZE / (M_PI * 2)) * (st[0] * ds->bulgeWidth + now);

		scale = tr.sinTable[off & FUNCTABLE_MASK] * ds->bulgeHeight;

		xyz[0] += normal[0] * scale;
		xyz[1] += normal[1] * scale;
		xyz[2] += normal[2] * scale;
	}
}


/*
======================
RB_CalcMoveVertexes

A deformation that can move an entire surface along a wave path
======================
*/
void RB_CalcMoveVertexes(deformStage_t * ds)
{
	int             i;
	float          *xyz;
	float          *table;
	float           scale;
	vec3_t          offset;

	table = TableForFunc(ds->deformationWave.func);

	scale = WAVEVALUE(table, ds->deformationWave.base,
					  ds->deformationWave.amplitude,
					  ds->deformationWave.phase, ds->deformationWave.frequency);

	VectorScale(ds->moveVector, scale, offset);

	xyz = (float *)tess.xyz;
	for(i = 0; i < tess.numVertexes; i++, xyz += 4)
	{
		VectorAdd(xyz, offset, xyz);
	}
}


/*
=============
DeformText

Change a polygon into a bunch of text polygons
=============
*/
void DeformText(const char *text)
{
	int             i;
	vec3_t          origin, width, height;
	int             len;
	int             ch;
	byte            color[4];
	float           bottom, top;
	vec3_t          mid;

	height[0] = 0;
	height[1] = 0;
	height[2] = -1;
	CrossProduct(tess.normals[0], height, width);

	// find the midpoint of the box
	VectorClear(mid);
	bottom = 999999;
	top = -999999;
	for(i = 0; i < 4; i++)
	{
		VectorAdd(tess.xyz[i], mid, mid);
		if(tess.xyz[i][2] < bottom)
		{
			bottom = tess.xyz[i][2];
		}
		if(tess.xyz[i][2] > top)
		{
			top = tess.xyz[i][2];
		}
	}
	VectorScale(mid, 0.25f, origin);

	// determine the individual character size
	height[0] = 0;
	height[1] = 0;
	height[2] = (top - bottom) * 0.5f;

	VectorScale(width, height[2] * -0.75f, width);

	// determine the starting position
	len = strlen(text);
	VectorMA(origin, (len - 1), width, origin);

	// clear the shader indexes
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	color[0] = color[1] = color[2] = color[3] = 255;

	// draw each character
	for(i = 0; i < len; i++)
	{
		ch = text[i];
		ch &= 255;

		if(ch != ' ')
		{
			int             row, col;
			float           frow, fcol, size;

			row = ch >> 4;
			col = ch & 15;

			frow = row * 0.0625f;
			fcol = col * 0.0625f;
			size = 0.0625f;

			Tess_AddQuadStampExt(origin, width, height, color, fcol, frow, fcol + size, frow + size);
		}
		VectorMA(origin, -2, width, origin);
	}
}

/*
=================
RB_ProjectionShadowDeform

for a projection shadow:
point[x] += light vector * ( z - shadow plane )
point[y] +=
point[z] = shadow plane
1 0 light[x] / light[z]
=================
*/
void RB_ProjectionShadowDeform(void)
{
	float          *xyz;
	int             i;
	float           h;
	vec3_t          ground;
	vec3_t          light;
	float           groundDist;
	float           d;
	vec3_t          lightDir;

	xyz = (float *)tess.xyz;

	ground[0] = backEnd.or.axis[0][2];
	ground[1] = backEnd.or.axis[1][2];
	ground[2] = backEnd.or.axis[2][2];

	groundDist = backEnd.or.origin[2] - backEnd.currentEntity->e.shadowPlane;

	VectorCopy(backEnd.currentEntity->lightDir, lightDir);
	d = DotProduct(lightDir, ground);
	// don't let the shadows get too long or go negative
	if(d < 0.5)
	{
		VectorMA(lightDir, (0.5 - d), ground, lightDir);
		d = DotProduct(lightDir, ground);
	}
	d = 1.0 / d;

	light[0] = lightDir[0] * d;
	light[1] = lightDir[1] * d;
	light[2] = lightDir[2] * d;

	for(i = 0; i < tess.numVertexes; i++, xyz += 4)
	{
		h = DotProduct(xyz, ground) + groundDist;

		xyz[0] -= light[0] * h;
		xyz[1] -= light[1] * h;
		xyz[2] -= light[2] * h;
	}
}

/*
==================
GlobalVectorToLocal
==================
*/
static void GlobalVectorToLocal(const vec3_t in, vec3_t out)
{
	out[0] = DotProduct(in, backEnd.or.axis[0]);
	out[1] = DotProduct(in, backEnd.or.axis[1]);
	out[2] = DotProduct(in, backEnd.or.axis[2]);
}

/*
=====================
AutospriteDeform

Assuming all the triangles for this shader are independant
quads, rebuild them as forward facing sprites
=====================
*/
static void AutospriteDeform(void)
{
	int             i;
	int             oldVerts;
	float          *xyz;
	vec3_t          mid, delta;
	float           radius;
	vec3_t          left, up;
	vec3_t          leftDir, upDir;

	if(tess.numVertexes & 3)
	{
		ri.Printf(PRINT_WARNING, "Autosprite shader %s had odd vertex count", tess.surfaceShader->name);
	}
	if(tess.numIndexes != (tess.numVertexes >> 2) * 6)
	{
		ri.Printf(PRINT_WARNING, "Autosprite shader %s had odd index count", tess.surfaceShader->name);
	}

	oldVerts = tess.numVertexes;
	tess.numVertexes = 0;
	tess.numIndexes = 0;

	if(backEnd.currentEntity != &tr.worldEntity)
	{
		GlobalVectorToLocal(backEnd.viewParms.or.axis[1], leftDir);
		GlobalVectorToLocal(backEnd.viewParms.or.axis[2], upDir);
	}
	else
	{
		VectorCopy(backEnd.viewParms.or.axis[1], leftDir);
		VectorCopy(backEnd.viewParms.or.axis[2], upDir);
	}

	for(i = 0; i < oldVerts; i += 4)
	{
		// find the midpoint
		xyz = tess.xyz[i];

		mid[0] = 0.25f * (xyz[0] + xyz[4] + xyz[8] + xyz[12]);
		mid[1] = 0.25f * (xyz[1] + xyz[5] + xyz[9] + xyz[13]);
		mid[2] = 0.25f * (xyz[2] + xyz[6] + xyz[10] + xyz[14]);

		VectorSubtract(xyz, mid, delta);
		radius = VectorLength(delta) * 0.707f;	// / sqrt(2)

		VectorScale(leftDir, radius, left);
		VectorScale(upDir, radius, up);

		if(backEnd.viewParms.isMirror)
		{
			VectorSubtract(vec3_origin, left, left);
		}

		// compensate for scale in the axes if necessary
		if(backEnd.currentEntity->e.nonNormalizedAxes)
		{
			float           axisLength;

			axisLength = VectorLength(backEnd.currentEntity->e.axis[0]);
			if(!axisLength)
			{
				axisLength = 0;
			}
			else
			{
				axisLength = 1.0f / axisLength;
			}
			VectorScale(left, axisLength, left);
			VectorScale(up, axisLength, up);
		}

		Tess_AddQuadStamp(mid, left, up, tess.colors[i]);
	}
}


/*
=====================
Autosprite2Deform

Autosprite2 will pivot a rectangular quad along the center of its long axis
=====================
*/
int             edgeVerts[6][2] = {
	{0, 1},
	{0, 2},
	{0, 3},
	{1, 2},
	{1, 3},
	{2, 3}
};

static void Autosprite2Deform(void)
{
	int             i, j, k;
	int             indexes;
	float          *xyz;
	vec3_t          forward;

	if(tess.numVertexes & 3)
	{
		ri.Printf(PRINT_WARNING, "Autosprite2 shader %s had odd vertex count", tess.surfaceShader->name);
	}
	if(tess.numIndexes != (tess.numVertexes >> 2) * 6)
	{
		ri.Printf(PRINT_WARNING, "Autosprite2 shader %s had odd index count", tess.surfaceShader->name);
	}

	if(backEnd.currentEntity != &tr.worldEntity)
	{
		GlobalVectorToLocal(backEnd.viewParms.or.axis[0], forward);
	}
	else
	{
		VectorCopy(backEnd.viewParms.or.axis[0], forward);
	}

	// this is a lot of work for two triangles...
	// we could precalculate a lot of it is an issue, but it would mess up
	// the shader abstraction
	for(i = 0, indexes = 0; i < tess.numVertexes; i += 4, indexes += 6)
	{
		float           lengths[2];
		int             nums[2];
		vec3_t          mid[2];
		vec3_t          major, minor;
		float          *v1, *v2;

		// find the midpoint
		xyz = tess.xyz[i];

		// identify the two shortest edges
		nums[0] = nums[1] = 0;
		lengths[0] = lengths[1] = 999999;

		for(j = 0; j < 6; j++)
		{
			float           l;
			vec3_t          temp;

			v1 = xyz + 4 * edgeVerts[j][0];
			v2 = xyz + 4 * edgeVerts[j][1];

			VectorSubtract(v1, v2, temp);

			l = DotProduct(temp, temp);
			if(l < lengths[0])
			{
				nums[1] = nums[0];
				lengths[1] = lengths[0];
				nums[0] = j;
				lengths[0] = l;
			}
			else if(l < lengths[1])
			{
				nums[1] = j;
				lengths[1] = l;
			}
		}

		for(j = 0; j < 2; j++)
		{
			v1 = xyz + 4 * edgeVerts[nums[j]][0];
			v2 = xyz + 4 * edgeVerts[nums[j]][1];

			mid[j][0] = 0.5f * (v1[0] + v2[0]);
			mid[j][1] = 0.5f * (v1[1] + v2[1]);
			mid[j][2] = 0.5f * (v1[2] + v2[2]);
		}

		// find the vector of the major axis
		VectorSubtract(mid[1], mid[0], major);

		// cross this with the view direction to get minor axis
		CrossProduct(major, forward, minor);
		VectorNormalize(minor);

		// re-project the points
		for(j = 0; j < 2; j++)
		{
			float           l;

			v1 = xyz + 4 * edgeVerts[nums[j]][0];
			v2 = xyz + 4 * edgeVerts[nums[j]][1];

			l = 0.5 * sqrt(lengths[j]);

			// we need to see which direction this edge
			// is used to determine direction of projection
			for(k = 0; k < 5; k++)
			{
				if(tess.indexes[indexes + k] == i + edgeVerts[nums[j]][0]
				   && tess.indexes[indexes + k + 1] == i + edgeVerts[nums[j]][1])
				{
					break;
				}
			}

			if(k == 5)
			{
				VectorMA(mid[j], l, minor, v1);
				VectorMA(mid[j], -l, minor, v2);
			}
			else
			{
				VectorMA(mid[j], -l, minor, v1);
				VectorMA(mid[j], l, minor, v2);
			}
		}
	}
}


/*
=====================
RB_DeformTessGeometry

=====================
*/
void RB_DeformTessGeometry(void)
{
	int             i;
	deformStage_t  *ds;

	for(i = 0; i < tess.surfaceShader->numDeforms; i++)
	{
		ds = &tess.surfaceShader->deforms[i];

		switch (ds->deformation)
		{
			case DEFORM_NONE:
				break;
				
			case DEFORM_NORMALS:
				RB_CalcDeformNormals(ds);
				break;
				
			case DEFORM_WAVE:
				RB_CalcDeformVertexes(ds);
				break;
				
			case DEFORM_BULGE:
				RB_CalcBulgeVertexes(ds);
				break;
				
			case DEFORM_MOVE:
				RB_CalcMoveVertexes(ds);
				break;

			case DEFORM_PROJECTION_SHADOW: 	 
				RB_ProjectionShadowDeform(); 	 
				break;
	
			case DEFORM_AUTOSPRITE:
				AutospriteDeform();
				break;
				
			case DEFORM_AUTOSPRITE2:
				Autosprite2Deform();
				break;
				
			case DEFORM_TEXT0:
			case DEFORM_TEXT1:
			case DEFORM_TEXT2:
			case DEFORM_TEXT3:
			case DEFORM_TEXT4:
			case DEFORM_TEXT5:
			case DEFORM_TEXT6:
			case DEFORM_TEXT7:
				DeformText(backEnd.refdef.text[ds->deformation - DEFORM_TEXT0]);
				break;
				
			case DEFORM_SPRITE:
				//AutospriteDeform();
				break;
				
			case DEFORM_FLARE:
				//Autosprite2Deform();
				break;
		}
	}
}

/*
====================================================================

COLORS

====================================================================
*/


/*
** RB_CalcColorFromEntity
*/
void RB_CalcColorFromEntity(unsigned char *dstColors)
{
	int             i;
	int            *pColors = (int *)dstColors;
	int             c;

	if(!backEnd.currentEntity)
		return;

	c = *(int *)backEnd.currentEntity->e.shaderRGBA;

	for(i = 0; i < tess.numVertexes; i++, pColors++)
	{
		*pColors = c;
	}
}

/*
** RB_CalcColorFromOneMinusEntity
*/
void RB_CalcColorFromOneMinusEntity(unsigned char *dstColors)
{
	int             i;
	int            *pColors = (int *)dstColors;
	unsigned char   invModulate[3];
	int             c;

	if(!backEnd.currentEntity)
		return;

	invModulate[0] = 255 - backEnd.currentEntity->e.shaderRGBA[0];
	invModulate[1] = 255 - backEnd.currentEntity->e.shaderRGBA[1];
	invModulate[2] = 255 - backEnd.currentEntity->e.shaderRGBA[2];
	invModulate[3] = 255 - backEnd.currentEntity->e.shaderRGBA[3];	// this trashes alpha, but the AGEN block fixes it

	c = *(int *)invModulate;

	for(i = 0; i < tess.numVertexes; i++, pColors++)
	{
		*pColors = *(int *)invModulate;
	}
}

/*
** RB_CalcAlphaFromEntity
*/
void RB_CalcAlphaFromEntity(unsigned char *dstColors)
{
	int             i;

	if(!backEnd.currentEntity)
		return;

	dstColors += 3;

	for(i = 0; i < tess.numVertexes; i++, dstColors += 4)
	{
		*dstColors = backEnd.currentEntity->e.shaderRGBA[3];
	}
}

/*
** RB_CalcAlphaFromOneMinusEntity
*/
void RB_CalcAlphaFromOneMinusEntity(unsigned char *dstColors)
{
	int             i;

	if(!backEnd.currentEntity)
		return;

	dstColors += 3;

	for(i = 0; i < tess.numVertexes; i++, dstColors += 4)
	{
		*dstColors = 0xff - backEnd.currentEntity->e.shaderRGBA[3];
	}
}

/*
** RB_CalcWaveColor
*/
void RB_CalcWaveColor(const waveForm_t * wf, unsigned char *dstColors)
{
	int             i;
	int             v;
	float           glow;
	int            *colors = (int *)dstColors;
	byte            color[4];


	if(wf->func == GF_NOISE)
	{
		glow =
			wf->base + R_NoiseGet4f(0, 0, 0, (tess.shaderTime + wf->phase) * wf->frequency) * wf->amplitude;
	}
	else
	{
		glow = EvalWaveForm(wf) * tr.identityLight;
	}

	if(glow < 0)
	{
		glow = 0;
	}
	else if(glow > 1)
	{
		glow = 1;
	}

	v = myftol(255 * glow);
	color[0] = color[1] = color[2] = v;
	color[3] = 255;
	v = *(int *)color;

	for(i = 0; i < tess.numVertexes; i++, colors++)
	{
		*colors = v;
	}
}

/*
** RB_CalcWaveAlpha
*/
void RB_CalcWaveAlpha(const waveForm_t * wf, unsigned char *dstColors)
{
	int             i;
	int             v;
	float           glow;

	glow = EvalWaveFormClamped(wf);

	v = 255 * glow;

	for(i = 0; i < tess.numVertexes; i++, dstColors += 4)
	{
		dstColors[3] = v;
	}
}

/*
** RB_CalcModulateColorsByFog
*/
void RB_CalcModulateColorsByFog(unsigned char *colors)
{
	int             i;
	static float    texCoords[SHADER_MAX_VERTEXES][2];

	// calculate texcoords so we can derive density
	// this is not wasted, because it would only have
	// been previously called if the surface was opaque
	RB_CalcFogTexCoords(texCoords[0]);

	for(i = 0; i < tess.numVertexes; i++, colors += 4)
	{
		float           f = 1.0 - R_FogFactor(texCoords[i][0], texCoords[i][1]);

		colors[0] *= f;
		colors[1] *= f;
		colors[2] *= f;
	}
}

/*
** RB_CalcModulateAlphasByFog
*/
void RB_CalcModulateAlphasByFog(unsigned char *colors)
{
	int             i;
	static float    texCoords[SHADER_MAX_VERTEXES][2];

	// calculate texcoords so we can derive density
	// this is not wasted, because it would only have
	// been previously called if the surface was opaque
	RB_CalcFogTexCoords(texCoords[0]);

	for(i = 0; i < tess.numVertexes; i++, colors += 4)
	{
		float           f = 1.0 - R_FogFactor(texCoords[i][0], texCoords[i][1]);

		colors[3] *= f;
	}
}

/*
** RB_CalcModulateRGBAsByFog
*/
void RB_CalcModulateRGBAsByFog(unsigned char *colors)
{
	int             i;
	static float    texCoords[SHADER_MAX_VERTEXES][2];

	// calculate texcoords so we can derive density
	// this is not wasted, because it would only have
	// been previously called if the surface was opaque
	RB_CalcFogTexCoords(texCoords[0]);

	for(i = 0; i < tess.numVertexes; i++, colors += 4)
	{
		float           f = 1.0 - R_FogFactor(texCoords[i][0], texCoords[i][1]);

		colors[0] *= f;
		colors[1] *= f;
		colors[2] *= f;
		colors[3] *= f;
	}
}

void RB_CalcCustomColor(const expression_t * rgbExp, unsigned char *dstColors)
{
	int             i;
	int				v;
	float           rgb;
	
	if(backEnd.currentEntity)
	{
		rgb = Q_bound(0.0, RB_EvalExpression(rgbExp, 1.0), 1.0);
	}
	else
	{
		// fullbright
		rgb = RB_EvalExpression(rgbExp, 1.0);
	}
	
	v = rgb * 255;

	for(i = 0; i < tess.numVertexes; i++, dstColors += 4)
	{
		dstColors[0] = v;
		dstColors[1] = v;
		dstColors[2] = v;
	}
}

/*
** RB_CalcCustomColors
*/
void RB_CalcCustomColors(const expression_t * redExp, const expression_t * greenExp, const expression_t * blueExp, unsigned char *dstColors)
{
	int             i;
	float           red;
	float			green;
	float			blue;
	
	if(backEnd.currentEntity)
	{
		red = Q_bound(0.0, RB_EvalExpression(redExp, backEnd.currentEntity->e.shaderRGBA[0] * (1.0 / 255.0)), 1.0);
		green = Q_bound(0.0, RB_EvalExpression(greenExp, backEnd.currentEntity->e.shaderRGBA[1] * (1.0 / 255.0)), 1.0);
		blue = Q_bound(0.0, RB_EvalExpression(blueExp, backEnd.currentEntity->e.shaderRGBA[2] * (1.0 / 255.0)), 1.0);
	}
	else
	{
		// fullbright
		red = Q_bound(0.0, RB_EvalExpression(redExp, 1.0), 1.0);
		green = Q_bound(0.0, RB_EvalExpression(greenExp, 1.0), 1.0);
		blue = Q_bound(0.0, RB_EvalExpression(blueExp, 1.0), 1.0);
	}

	for(i = 0; i < tess.numVertexes; i++, dstColors += 4)
	{
		dstColors[0] = red * 255;
		dstColors[1] = green * 255;
		dstColors[2] = blue * 255;
	}
}

/*
** RB_CalcCustomAlpha
*/
void RB_CalcCustomAlpha(const expression_t * alphaExp, unsigned char *dstColors)
{
	int             i;
	int             v;
	float           alpha;
	
	if(backEnd.currentEntity)
	{
		alpha = Q_bound(0.0, RB_EvalExpression(alphaExp, backEnd.currentEntity->e.shaderRGBA[3] * (1.0 / 255.0)), 1.0);
	}
	else
	{
		// default to opaque
		alpha = Q_bound(0.0, RB_EvalExpression(alphaExp, 1.0), 1.0);
	}

	v = alpha * 255;
	
	for(i = 0; i < tess.numVertexes; i++, dstColors += 4)
	{
		dstColors[3] = v;
	}
}


/*
====================================================================

TEX COORDS

====================================================================
*/

/*
========================
RB_CalcFogTexCoords

To do the clipped fog plane really correctly, we should use
projected textures, but I don't trust the drivers and it
doesn't fit our shader data.
========================
*/
void RB_CalcFogTexCoords(float *st)
{
	int             i;
	float          *v;
	float           s, t;
	float           eyeT;
	qboolean        eyeOutside;
	fog_t          *fog;
	vec3_t          local;
	vec4_t          fogDistanceVector, fogDepthVector;
//	matrix_t        modelViewMatrix;

	fog = tr.world->fogs + tess.fogNum;

	// all fogging distance is based on world Z units
	VectorSubtract(backEnd.or.origin, backEnd.viewParms.or.origin, local);

//	MatrixMultiply(quakeToOpenGLMatrix, backEnd.or.modelViewMatrix, modelViewMatrix);
//	fogDistanceVector[0] = -modelViewMatrix[2];
//	fogDistanceVector[1] = -modelViewMatrix[6];
//	fogDistanceVector[2] = -modelViewMatrix[10];
	
	fogDistanceVector[0] =-backEnd.or.modelViewMatrix[ 2];
	fogDistanceVector[1] =-backEnd.or.modelViewMatrix[ 6];
	fogDistanceVector[2] =-backEnd.or.modelViewMatrix[10];

	fogDistanceVector[3] = DotProduct(local, backEnd.viewParms.or.axis[0]);

	// scale the fog vectors based on the fog's thickness
	fogDistanceVector[0] *= fog->tcScale;
	fogDistanceVector[1] *= fog->tcScale;
	fogDistanceVector[2] *= fog->tcScale;
	fogDistanceVector[3] *= fog->tcScale;

	// rotate the gradient vector for this orientation
	if(fog->hasSurface)
	{
		fogDepthVector[0] = fog->surface[0] * backEnd.or.axis[0][0] +
			fog->surface[1] * backEnd.or.axis[0][1] + fog->surface[2] * backEnd.or.axis[0][2];

		fogDepthVector[1] = fog->surface[0] * backEnd.or.axis[1][0] +
			fog->surface[1] * backEnd.or.axis[1][1] + fog->surface[2] * backEnd.or.axis[1][2];

		fogDepthVector[2] = fog->surface[0] * backEnd.or.axis[2][0] +
			fog->surface[1] * backEnd.or.axis[2][1] + fog->surface[2] * backEnd.or.axis[2][2];

		fogDepthVector[3] = -fog->surface[3] + DotProduct(backEnd.or.origin, fog->surface);

		eyeT = DotProduct(backEnd.or.viewOrigin, fogDepthVector) + fogDepthVector[3];
	}
	else
	{
		fogDepthVector[0] = 0;
		fogDepthVector[1] = 0;
		fogDepthVector[2] = 0;
		fogDepthVector[3] = 0;
		
		eyeT = 1;				// non-surface fog always has eye inside
	}

	// see if the viewpoint is outside
	// this is needed for clipping distance even for constant fog

	if(eyeT < 0)
	{
		eyeOutside = qtrue;
	}
	else
	{
		eyeOutside = qfalse;
	}

	fogDistanceVector[3] += 1.0 / 512;

	// calculate density for each point
	for(i = 0, v = tess.xyz[0]; i < tess.numVertexes; i++, v += 4)
	{
		// calculate the length in fog
		s = DotProduct(v, fogDistanceVector) + fogDistanceVector[3];
		t = DotProduct(v, fogDepthVector) + fogDepthVector[3];

		// partially clipped fogs use the T axis        
		if(eyeOutside)
		{
			if(t < 1.0)
			{
				t = 1.0 / 32;	// point is outside, so no fogging
			}
			else
			{
				t = 1.0 / 32 + 30.0 / 32 * t / (t - eyeT);	// cut the distance at the fog plane
			}
		}
		else
		{
			if(t < 0)
			{
				t = 1.0 / 32;	// point is outside, so no fogging
			}
			else
			{
				t = 31.0 / 32;
			}
		}

		st[0] = s;
		st[1] = t;
		st += 2;
	}
}



/*
** RB_CalcEnvironmentTexCoords
*/
void RB_CalcEnvironmentTexCoords(float *st)
{
	int             i;
	float          *v, *normal;
	vec3_t          viewer, reflected;
	float           d;

	v = tess.xyz[0];
	normal = tess.normals[0];

	for(i = 0; i < tess.numVertexes; i++, v += 4, normal += 4, st += 2)
	{
		VectorSubtract(backEnd.or.viewOrigin, v, viewer);
		VectorNormalizeFast(viewer);

		d = DotProduct(normal, viewer);

		reflected[0] = normal[0] * 2 * d - viewer[0];
		reflected[1] = normal[1] * 2 * d - viewer[1];
		reflected[2] = normal[2] * 2 * d - viewer[2];

		st[0] = 0.5 + reflected[1] * 0.5;
		st[1] = 0.5 - reflected[2] * 0.5;
	}
}

/*
** RB_CalcTurbulentTexCoords
*/
void RB_CalcTurbulentTexCoords(const waveForm_t * wf, float *st)
{
	int             i;
	float           now;

	now = (wf->phase + tess.shaderTime * wf->frequency);

	for(i = 0; i < tess.numVertexes; i++, st += 2)
	{
		float           s = st[0];
		float           t = st[1];

		st[0] =
			s +
			tr.sinTable[((int)(((tess.xyz[i][0] + tess.xyz[i][2]) * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE)) &
					 (FUNCTABLE_MASK)] * wf->amplitude;
		st[1] =
			t +
			tr.sinTable[((int)((tess.xyz[i][1] * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE)) & (FUNCTABLE_MASK)]
			* wf->amplitude;
	}
}

/*
** RB_CalcScaleTexCoords
*/
void RB_CalcScaleTexCoords(const float scale[2], float *st)
{
	int             i;

	for(i = 0; i < tess.numVertexes; i++, st += 2)
	{
		st[0] *= scale[0];
		st[1] *= scale[1];
	}
}

/*
** RB_CalcScrollTexCoords
*/
void RB_CalcScrollTexCoords(const float scrollSpeed[2], float *st)
{
	int             i;
	float           timeScale = tess.shaderTime;
	float           adjustedScrollS, adjustedScrollT;

	adjustedScrollS = scrollSpeed[0] * timeScale;
	adjustedScrollT = scrollSpeed[1] * timeScale;

	// clamp so coordinates don't continuously get larger, causing problems
	// with hardware limits
	adjustedScrollS = adjustedScrollS - floor(adjustedScrollS);
	adjustedScrollT = adjustedScrollT - floor(adjustedScrollT);

	for(i = 0; i < tess.numVertexes; i++, st += 2)
	{
		st[0] += adjustedScrollS;
		st[1] += adjustedScrollT;
	}
}

/*
** RB_CalcTransformTexCoords
*/
void RB_CalcTransformTexCoords(const texModInfo_t * tmi, float *st)
{
	int             i;

	for(i = 0; i < tess.numVertexes; i++, st += 2)
	{
		float           s = st[0];
		float           t = st[1];

		st[0] = s * tmi->matrix[0][0] + t * tmi->matrix[1][0] + tmi->translate[0];
		st[1] = s * tmi->matrix[0][1] + t * tmi->matrix[1][1] + tmi->translate[1];
	}
}

/*
** RB_CalcRotateTexCoords
*/
void RB_CalcRotateTexCoords(float degsPerSecond, float *st)
{
	float           timeScale = tess.shaderTime;
	float           degs;
	int             index;
	float           sinValue, cosValue;
	texModInfo_t    tmi;

	degs = -degsPerSecond * timeScale;
	index = degs * (FUNCTABLE_SIZE / 360.0f);

	sinValue = tr.sinTable[index & FUNCTABLE_MASK];
	cosValue = tr.sinTable[(index + FUNCTABLE_SIZE / 4) & FUNCTABLE_MASK];

	tmi.matrix[0][0] = cosValue;
	tmi.matrix[1][0] = -sinValue;
	tmi.translate[0] = 0.5 - 0.5 * cosValue + 0.5 * sinValue;

	tmi.matrix[0][1] = sinValue;
	tmi.matrix[1][1] = cosValue;
	tmi.translate[1] = 0.5 - 0.5 * sinValue - 0.5 * cosValue;

	RB_CalcTransformTexCoords(&tmi, st);
}

/*
** RB_CalcScrollTexCoords2
*/
void RB_CalcScrollTexCoords2(const expression_t * sExp, const expression_t * tExp, float *st)
{
	int             i;
	float           adjustedScrollS, adjustedScrollT;

	adjustedScrollS = RB_EvalExpression(sExp, 0);
	adjustedScrollT = RB_EvalExpression(tExp, 0);

	// clamp so coordinates don't continuously get larger, causing problems
	// with hardware limits
	adjustedScrollS = adjustedScrollS - floor(adjustedScrollS);
	adjustedScrollT = adjustedScrollT - floor(adjustedScrollT);

	for(i = 0; i < tess.numVertexes; i++, st += 2)
	{
		st[0] += adjustedScrollS;
		st[1] += adjustedScrollT;
	}
}

/*
** RB_CalcScaleTexCoords2
*/
void RB_CalcScaleTexCoords2(const expression_t * sExp, const expression_t * tExp, float *st)
{
	int             i;
	float           scaleS;
	float           scaleT;

	scaleS = RB_EvalExpression(sExp, 0);
	scaleT = RB_EvalExpression(tExp, 0);

	for(i = 0; i < tess.numVertexes; i++, st += 2)
	{
		st[0] *= scaleS;
		st[1] *= scaleT;
	}
}

/*
** RB_CalcCenterScaleTexCoords
*/
void RB_CalcCenterScaleTexCoords(const expression_t * sExp, const expression_t * tExp, float *st)
{
	int             i;
	float           scaleS;
	float           scaleT;
	matrix_t		matrix;
	vec4_t			strw;
	vec4_t			strw2;

	scaleS = RB_EvalExpression(sExp, 0);
	scaleT = RB_EvalExpression(tExp, 0);
	
	MatrixSetupTranslation(matrix, 0.5, 0.5, 0.0);
	MatrixMultiplyScale(matrix, scaleS, scaleT, 1.0);
	MatrixMultiplyTranslation(matrix, -0.5, -0.5, 0.0);

	for(i = 0; i < tess.numVertexes; i++, st += 2)
	{
		strw[0] = st[0];
		strw[1] = st[1];
		strw[2] = 0;
		strw[3] = 1;
		
		MatrixTransform4(matrix, strw, strw2);
		
		st[0] = strw2[0];
		st[1] = strw2[1];
	}
}

/*
** RB_CalcShearTexCoords
*/
void RB_CalcShearTexCoords(const expression_t * sExp, const expression_t * tExp, float *st)
{
	int             i;
	float           shearS;
	float           shearT;
	matrix_t		matrix;
	vec4_t			strw;
	vec4_t			strw2;

	shearS = RB_EvalExpression(sExp, 0);
	shearT = RB_EvalExpression(tExp, 0);
	
	MatrixSetupTranslation(matrix, 0.5, 0.5, 0.0);
	MatrixMultiplyShear(matrix, shearS, shearT);
	MatrixMultiplyTranslation(matrix, -0.5, -0.5, 0.0);

	for(i = 0; i < tess.numVertexes; i++, st += 2)
	{
		strw[0] = st[0];
		strw[1] = st[1];
		strw[2] = 0;
		strw[3] = 1;
		
		MatrixTransform4(matrix, strw, strw2);
		
		st[0] = strw2[0];
		st[1] = strw2[1];
	}
}

/*
** RB_CalcRotateTexCoords2
*/
void RB_CalcRotateTexCoords2(const expression_t * rExp, float *st)
{
	int             i;
	float           degrees;
	matrix_t		matrix;
	vec4_t			strw;
	vec4_t			strw2;

	degrees = RAD2DEG(RB_EvalExpression(rExp, 0)) * 5.0;
	
	MatrixSetupTranslation(matrix, 0.5, 0.5, 0.0);
	MatrixMultiplyZRotation(matrix, degrees);
	MatrixMultiplyTranslation(matrix, -0.5, -0.5, 0.0);

	for(i = 0; i < tess.numVertexes; i++, st += 2)
	{
		strw[0] = st[0];
		strw[1] = st[1];
		strw[2] = 0;
		strw[3] = 1;
		
		MatrixTransform4(matrix, strw, strw2);
		
		st[0] = strw2[0];
		st[1] = strw2[1];
	}
}




#if id386 && !( (defined __linux__ || defined __FreeBSD__ ) && (defined __i386__ ) )	// rb010123

long myftol(float f)
{
#ifndef __MINGW32__
	static int tmp;
	__asm fld f
	__asm fistp tmp
	__asm mov eax, tmp
#else
	return (long)f;
#endif
}

#endif

/*
** RB_CalcSpecularAlpha
**
** Calculates specular coefficient and places it in the alpha channel
*/
vec3_t          lightOrigin = { -960, 1980, 96 };	// FIXME: track dynamically

void RB_CalcSpecularAlpha(unsigned char *alphas)
{
	int             i;
	float          *v, *normal;
	vec3_t          viewer, reflected;
	float           l, d;
	int             b;
	vec3_t          lightDir;
	int             numVertexes;

	v = tess.xyz[0];
	normal = tess.normals[0];

	alphas += 3;

	numVertexes = tess.numVertexes;
	for(i = 0; i < numVertexes; i++, v += 4, normal += 4, alphas += 4)
	{
		float           ilength;

		VectorSubtract(lightOrigin, v, lightDir);
//      ilength = Q_rsqrt( DotProduct( lightDir, lightDir ) );
		VectorNormalizeFast(lightDir);

		// calculate the specular color
		d = DotProduct(normal, lightDir);
//      d *= ilength;

		// we don't optimize for the d < 0 case since this tends to
		// cause visual artifacts such as faceted "snapping"
		reflected[0] = normal[0] * 2 * d - lightDir[0];
		reflected[1] = normal[1] * 2 * d - lightDir[1];
		reflected[2] = normal[2] * 2 * d - lightDir[2];

		VectorSubtract(backEnd.or.viewOrigin, v, viewer);
		ilength = Q_rsqrt(DotProduct(viewer, viewer));
		l = DotProduct(reflected, viewer);
		l *= ilength;

		if(l < 0)
		{
			b = 0;
		}
		else
		{
			l = l * l;
			l = l * l;
			b = l * 255;
			if(b > 255)
			{
				b = 255;
			}
		}

		*alphas = b;
	}
}

/*
** RB_CalcDiffuseColor
**
** The basic vertex lighting calc
*/
void RB_CalcDiffuseColor(unsigned char *colors)
{
	int             i, j;
	float          *v, *normal;
	float           incoming;
	trRefEntity_t  *ent;
	int             ambientLightInt;
	vec3_t          ambientLight;
	vec3_t          lightDir;
	vec3_t          directedLight;
	int             numVertexes;

	ent = backEnd.currentEntity;
	ambientLightInt = ent->ambientLightInt;

	VectorCopy(ent->ambientLight, ambientLight);
	VectorCopy(ent->directedLight, directedLight);
	VectorCopy(ent->lightDir, lightDir);

	v = tess.xyz[0];
	normal = tess.normals[0];

	numVertexes = tess.numVertexes;
	for(i = 0; i < numVertexes; i++, v += 4, normal += 4)
	{
		incoming = DotProduct(normal, lightDir);
		if(incoming <= 0)
		{
			*(int *)&colors[i * 4] = ambientLightInt;
			continue;
		}
		j = myftol(ambientLight[0] + incoming * directedLight[0]);
		if(j > 255)
		{
			j = 255;
		}
		colors[i * 4 + 0] = j;

		j = myftol(ambientLight[1] + incoming * directedLight[1]);
		if(j > 255)
		{
			j = 255;
		}
		colors[i * 4 + 1] = j;

		j = myftol(ambientLight[2] + incoming * directedLight[2]);
		if(j > 255)
		{
			j = 255;
		}
		colors[i * 4 + 2] = j;

		colors[i * 4 + 3] = 255;
	}
}
