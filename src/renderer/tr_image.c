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
// tr_image.c
#include "tr_local.h"

static byte     s_intensitytable[256];
static unsigned char s_gammatable[256];

int             gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int             gl_filter_max = GL_LINEAR;

#define FILE_HASH_SIZE		(1024 * 2)
static image_t *hashTable[FILE_HASH_SIZE];

/*
** R_GammaCorrect
*/
void R_GammaCorrect(byte * buffer, int bufSize)
{
	int             i;

	for(i = 0; i < bufSize; i++)
	{
		buffer[i] = s_gammatable[buffer[i]];
	}
}

typedef struct
{
	char           *name;
	int             minimize, maximize;
} textureMode_t;

textureMode_t   modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};


/*
================
return a hash value for the filename
================
*/
static long generateHashValue(const char *fname)
{
	int             i;
	long            hash;
	char            letter;

//  ri.Printf(PRINT_ALL, "tr_image::generateHashValue: '%s'\n", fname);

	hash = 0;
	i = 0;
	while(fname[i] != '\0')
	{
		letter = tolower(fname[i]);

		//if(letter == '.')
		//  break;              // don't include extension

		if(letter == '\\')
			letter = '/';		// damn path names

		hash += (long)(letter) * (i + 119);
		i++;
	}
	hash &= (FILE_HASH_SIZE - 1);
	return hash;
}

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode(const char *string)
{
	int             i;
	image_t        *image;

	for(i = 0; i < 6; i++)
	{
		if(!Q_stricmp(modes[i].name, string))
		{
			break;
		}
	}

	// hack to prevent trilinear from being set on voodoo,
	// because their driver freaks...
	if(i == 5 && glConfig.hardwareType == GLHW_3DFX_2D3D)
	{
		ri.Printf(PRINT_ALL, "Refusing to set trilinear on a voodoo.\n");
		i = 3;
	}


	if(i == 6)
	{
		ri.Printf(PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// bound texture anisotropy
	if(glConfig2.textureAnisotropyAvailable)
	{
		if(r_ext_texture_filter_anisotropic->value > glConfig2.maxTextureAnisotropy)
		{
			ri.Cvar_Set("r_ext_texture_filter_anisotropic", va("%f", glConfig2.maxTextureAnisotropy));
		}
		else if(r_ext_texture_filter_anisotropic->value < 1.0)
		{
			ri.Cvar_Set("r_ext_texture_filter_anisotropic", "1.0");
		}
	}

	// change all the existing mipmap texture objects
	for(i = 0; i < tr.numImages; i++)
	{
		image = tr.images[i];

		if(image->filterType == FT_DEFAULT)
		{
			GL_Bind(image);

			// set texture filter
			qglTexParameterf(image->type, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(image->type, GL_TEXTURE_MAG_FILTER, gl_filter_max);

			// set texture anisotropy
			if(glConfig2.textureAnisotropyAvailable)
				qglTexParameterf(image->type, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_texture_filter_anisotropic->value);
		}
	}
}

/*
===============
R_SumOfUsedImages
===============
*/
int R_SumOfUsedImages(void)
{
	int             total;
	int             i;

	total = 0;
	for(i = 0; i < tr.numImages; i++)
	{
		if(tr.images[i]->frameUsed == tr.frameCount)
		{
			total += tr.images[i]->uploadWidth * tr.images[i]->uploadHeight;
		}
	}

	return total;
}

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f(void)
{
	int             i;
	image_t        *image;
	int             texels;
	const char     *yesno[] = {
		"no ", "yes"
	};

	ri.Printf(PRINT_ALL, "\n      -w-- -h-- -mm- -type- -if-- wrap --name-------\n");
	texels = 0;

	for(i = 0; i < tr.numImages; i++)
	{
		image = tr.images[i];

		texels += image->uploadWidth * image->uploadHeight;
		ri.Printf(PRINT_ALL, "%4i: %4i %4i  %s   ",
				  i, image->uploadWidth, image->uploadHeight, yesno[image->filterType == FT_DEFAULT]);

		switch (image->type)
		{
			case GL_TEXTURE_2D:
				ri.Printf(PRINT_ALL, "2D   ");
				break;

			case GL_TEXTURE_CUBE_MAP_ARB:
				ri.Printf(PRINT_ALL, "CUBE ");
				break;

			default:
				ri.Printf(PRINT_ALL, "???? ");
		}

		switch (image->internalFormat)
		{
			case 1:
				ri.Printf(PRINT_ALL, "I    ");
				break;

			case 2:
				ri.Printf(PRINT_ALL, "IA   ");
				break;

			case 3:
				ri.Printf(PRINT_ALL, "RGB  ");
				break;

			case 4:
				ri.Printf(PRINT_ALL, "RGBA ");
				break;

			case GL_RGBA8:
				ri.Printf(PRINT_ALL, "RGBA8");
				break;

			case GL_RGB8:
				ri.Printf(PRINT_ALL, "RGB8");
				break;

			case GL_RGB4_S3TC:
				ri.Printf(PRINT_ALL, "S3TC ");
				break;

			case GL_RGBA4:
				ri.Printf(PRINT_ALL, "RGBA4");
				break;

			case GL_RGB5:
				ri.Printf(PRINT_ALL, "RGB5 ");
				break;

			default:
				ri.Printf(PRINT_ALL, "???? ");
		}

		switch (image->wrapType)
		{
			case WT_REPEAT:
				ri.Printf(PRINT_ALL, "rept  ");
				break;

			case WT_CLAMP:
				ri.Printf(PRINT_ALL, "clmp  ");
				break;

			case WT_EDGE_CLAMP:
				ri.Printf(PRINT_ALL, "eclmp ");
				break;

			case WT_ZERO_CLAMP:
				ri.Printf(PRINT_ALL, "zclmp ");
				break;

			case WT_ALPHA_ZERO_CLAMP:
				ri.Printf(PRINT_ALL, "azclmp");
				break;

			default:
				ri.Printf(PRINT_ALL, "%4i  ", image->wrapType);
				break;
		}

		ri.Printf(PRINT_ALL, " %s\n", image->name);
	}
	ri.Printf(PRINT_ALL, " ---------\n");
	ri.Printf(PRINT_ALL, " %i total texels (not including mipmaps)\n", texels);
	ri.Printf(PRINT_ALL, " %i total images\n\n", tr.numImages);
}



//=======================================================================

/*
================
ResampleTexture

Used to resample images in a more general than quartering fashion.

This will only be filtered properly if the resampled size
is greater than half the original size.

If a larger shrinking is needed, use the mipmap function 
before or after.
================
*/
static void ResampleTexture(unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight,
							qboolean normalMap)
{
	int             x, y;
	unsigned       *inrow, *inrow2;
	unsigned        frac, fracstep;
	unsigned        p1[2048], p2[2048];
	byte           *pix1, *pix2, *pix3, *pix4;
	float           inv127 = 1.0f / 127.0f;
	vec3_t          n, n2, n3, n4;

	// NOTE: Tr3B - limitation not needed anymore
//  if(outwidth > 2048)
//      ri.Error(ERR_DROP, "ResampleTexture: max width");

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for(x = 0; x < outwidth; x++)
	{
		p1[x] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for(x = 0; x < outwidth; x++)
	{
		p2[x] = 4 * (frac >> 16);
		frac += fracstep;
	}

	if(normalMap)
	{
		for(y = 0; y < outheight; y++, out += outwidth)
		{
			inrow = in + inwidth * (int)((y + 0.25) * inheight / outheight);
			inrow2 = in + inwidth * (int)((y + 0.75) * inheight / outheight);

			//frac = fracstep >> 1;

			for(x = 0; x < outwidth; x++)
			{
				pix1 = (byte *) inrow + p1[x];
				pix2 = (byte *) inrow + p2[x];
				pix3 = (byte *) inrow2 + p1[x];
				pix4 = (byte *) inrow2 + p2[x];

				n[0] = (pix1[0] * inv127 - 1.0);
				n[1] = (pix1[1] * inv127 - 1.0);
				n[2] = (pix1[2] * inv127 - 1.0);

				n2[0] = (pix2[0] * inv127 - 1.0);
				n2[1] = (pix2[1] * inv127 - 1.0);
				n2[2] = (pix2[2] * inv127 - 1.0);

				n3[0] = (pix3[0] * inv127 - 1.0);
				n3[1] = (pix3[1] * inv127 - 1.0);
				n3[2] = (pix3[2] * inv127 - 1.0);

				n4[0] = (pix4[0] * inv127 - 1.0);
				n4[1] = (pix4[1] * inv127 - 1.0);
				n4[2] = (pix4[2] * inv127 - 1.0);

				VectorAdd(n, n2, n);
				VectorAdd(n, n3, n);
				VectorAdd(n, n4, n);

				if(!VectorNormalize(n))
					VectorSet(n, 0, 0, 1);

				((byte *) (out + x))[0] = (byte) (128 + 127 * n[0]);
				((byte *) (out + x))[1] = (byte) (128 + 127 * n[1]);
				((byte *) (out + x))[2] = (byte) (128 + 127 * n[2]);
				((byte *) (out + x))[3] = (byte) (128 + 127 * 1.0);
			}
		}
	}
	else
	{
		for(y = 0; y < outheight; y++, out += outwidth)
		{
			inrow = in + inwidth * (int)((y + 0.25) * inheight / outheight);
			inrow2 = in + inwidth * (int)((y + 0.75) * inheight / outheight);

			//frac = fracstep >> 1;

			for(x = 0; x < outwidth; x++)
			{
				pix1 = (byte *) inrow + p1[x];
				pix2 = (byte *) inrow + p2[x];
				pix3 = (byte *) inrow2 + p1[x];
				pix4 = (byte *) inrow2 + p2[x];

				((byte *) (out + x))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
				((byte *) (out + x))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
				((byte *) (out + x))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
				((byte *) (out + x))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
			}
		}
	}
}


/*
================
R_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
void R_LightScaleTexture(unsigned *in, int inwidth, int inheight, qboolean onlyGamma)
{
	if(onlyGamma)
	{
		if(!glConfig.deviceSupportsGamma)
		{
			int             i, c;
			byte           *p;

			p = (byte *) in;

			c = inwidth * inheight;
			for(i = 0; i < c; i++, p += 4)
			{
				p[0] = s_gammatable[p[0]];
				p[1] = s_gammatable[p[1]];
				p[2] = s_gammatable[p[2]];
			}
		}
	}
	else
	{
		int             i, c;
		byte           *p;

		p = (byte *) in;

		c = inwidth * inheight;

		if(glConfig.deviceSupportsGamma)
		{
			// raynorpat: small optimization
			if(r_intensity->value != 1.0f)
			{
				for(i = 0; i < c; i++, p += 4)
				{
					p[0] = s_intensitytable[p[0]];
					p[1] = s_intensitytable[p[1]];
					p[2] = s_intensitytable[p[2]];
				}
			}
		}
		else
		{
			for(i = 0; i < c; i++, p += 4)
			{
				p[0] = s_gammatable[s_intensitytable[p[0]]];
				p[1] = s_gammatable[s_intensitytable[p[1]]];
				p[2] = s_gammatable[s_intensitytable[p[2]]];
			}
		}
	}
}



/*
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter
================
*/
static void R_MipMap2(unsigned *in, int inWidth, int inHeight)
{
	int             i, j, k;
	byte           *outpix;
	int             inWidthMask, inHeightMask;
	int             total;
	int             outWidth, outHeight;
	unsigned       *temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;
	temp = ri.Hunk_AllocateTempMemory(outWidth * outHeight * 4);

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for(i = 0; i < outHeight; i++)
	{
		for(j = 0; j < outWidth; j++)
		{
			outpix = (byte *) (temp + i * outWidth + j);
			for(k = 0; k < 4; k++)
			{
				total =
					1 * ((byte *) & in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					2 * ((byte *) & in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					2 * ((byte *) & in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					1 * ((byte *) & in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +
					2 * ((byte *) & in[((i * 2) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					4 * ((byte *) & in[((i * 2) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					4 * ((byte *) & in[((i * 2) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					2 * ((byte *) & in[((i * 2) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +
					2 * ((byte *) & in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					4 * ((byte *) & in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					4 * ((byte *) & in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					2 * ((byte *) & in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +
					1 * ((byte *) & in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					2 * ((byte *) & in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					2 * ((byte *) & in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					1 * ((byte *) & in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k];
				outpix[k] = total / 36;
			}
		}
	}

	Com_Memcpy(in, temp, outWidth * outHeight * 4);
	ri.Hunk_FreeTempMemory(temp);
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap(byte * in, int width, int height)
{
	int             i, j;
	byte           *out;
	int             row;

	if(!r_simpleMipMaps->integer)
	{
		R_MipMap2((unsigned *)in, width, height);
		return;
	}

	if(width == 1 && height == 1)
	{
		return;
	}

	row = width * 4;
	out = in;
	width >>= 1;
	height >>= 1;

	if(width == 0 || height == 0)
	{
		width += height;		// get largest
		for(i = 0; i < width; i++, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4]) >> 1;
			out[1] = (in[1] + in[5]) >> 1;
			out[2] = (in[2] + in[6]) >> 1;
			out[3] = (in[3] + in[7]) >> 1;
		}
		return;
	}

	for(i = 0; i < height; i++, in += row)
	{
		for(j = 0; j < width; j++, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4] + in[row + 0] + in[row + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[row + 1] + in[row + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[row + 2] + in[row + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[row + 3] + in[row + 7]) >> 2;
		}
	}
}



/*
================
R_MipNormalMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipNormalMap(byte * in, int width, int height)
{
	int             i, j;
	byte           *out;
	vec4_t          n;
	vec_t           length;

	float			inv255 = 1.0f / 255.0f;

	if(width == 1 && height == 1)
	{
		return;
	}

	out = in;
//	width >>= 1;
	width <<= 2;
	height >>= 1;
	
	for(i = 0; i < height; i++, in += width)
	{
		for(j = 0; j < width; j += 8, out += 4, in += 8)
		{
			n[0] =	(in[0] * inv255 - 0.5) * 2.0 +
					(in[4] * inv255 - 0.5) * 2.0 +
					(in[width + 0] * inv255 - 0.5) * 2.0 +
					(in[width + 4] * inv255 - 0.5) * 2.0;

			n[1] =	(in[1] * inv255 - 0.5) * 2.0 +
					(in[5] * inv255 - 0.5) * 2.0 +
					(in[width + 1] * inv255 - 0.5) * 2.0 +
					(in[width + 5] * inv255 - 0.5) * 2.0;

			n[2] =	(in[2] * inv255 - 0.5) * 2.0 +
					(in[6] * inv255 - 0.5) * 2.0 +
					(in[width + 2] * inv255 - 0.5) * 2.0 +
					(in[width + 6] * inv255 - 0.5) * 2.0;
					
			n[3] =	(inv255 * in[3]) +
					(inv255 * in[7]) +
					(inv255 * in[width + 3]) +
					(inv255 * in[width + 7]);

			length = VectorLength(n);

			if(length)
			{
				n[0] /= length;
				n[1] /= length;
				n[2] /= length;
			}
			else
			{
				VectorSet(n, 0.0, 0.0, 1.0);
			}

			out[0] = (byte) (128 + 127 * n[0]);
			out[1] = (byte) (128 + 127 * n[1]);
			out[2] = (byte) (128 + 127 * n[2]);
			out[3] = (byte) (n[3] * 255.0 / 4.0);
			//out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}

static void R_HeightMapToNormalMap(byte * in, int width, int height, float scale)
{
	int             x, y;
	float           r, g, b;
	float           c, cx, cy;
	float           dcx, dcy;
	float           inv255 = 1.0f / 255.0f;
	vec3_t          n;
	byte           *out;

	out = in;

	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			// convert the pixel at x, y in the bump map to a normal (float)

			// expand [0,255] texel values to the [0,1] range
			r = in[4 * (y * width + x) + 0];
			g = in[4 * (y * width + x) + 1];
			b = in[4 * (y * width + x) + 2];

			c = (r + g + b) * inv255;

			// expand the texel to its right
			if(x == width - 1)
			{
				r = in[4 * (y * width + x) + 0];
				g = in[4 * (y * width + x) + 1];
				b = in[4 * (y * width + x) + 2];
			}
			else
			{
				r = in[4 * (y * width + ((x + 1) % width)) + 0];
				g = in[4 * (y * width + ((x + 1) % width)) + 1];
				b = in[4 * (y * width + ((x + 1) % width)) + 2];
			}

			cx = (r + g + b) * inv255;

			// expand the texel one up
			if(y == height - 1)
			{
				r = in[4 * (y * width + x) + 0];
				g = in[4 * (y * width + x) + 1];
				b = in[4 * (y * width + x) + 2];
			}
			else
			{
				r = in[4 * (((y + 1) % height) * width + x) + 0];
				g = in[4 * (((y + 1) % height) * width + x) + 1];
				b = in[4 * (((y + 1) % height) * width + x) + 2];
			}

			cy = (r + g + b) * inv255;

			dcx = scale * (c - cx);
			dcy = scale * (c - cy);
			
			// normalize the vector
			VectorSet(n, dcx, dcy, 1.0);	//scale);
			if(!VectorNormalize(n))
				VectorSet(n, 0, 0, 1);

			// repack the normalized vector into an RGB unsigned byte
			// vector in the normal map image
			*out++ = (byte) (128 + 127 * n[0]);
			*out++ = (byte) (128 + 127 * n[1]);
			*out++ = (byte) (128 + 127 * n[2]);
			
			// put in no height as displacement map by default
			*out++ = (byte) 0;	//(Q_bound(0, c * 255.0 / 3.0, 255));
		}
	}
}

static void R_DisplaceMap(byte * in, byte * in2, int width, int height)
{
	int             x, y;
	vec3_t          n;
	int             avg;
	float           inv255 = 1.0f / 255.0f;
	byte           *out;

	out = in;

	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			n[0] = (in[4 * (y * width + x) + 0] * inv255 - 0.5) * 2.0;
			n[1] = (in[4 * (y * width + x) + 1] * inv255 - 0.5) * 2.0;
			n[2] = (in[4 * (y * width + x) + 2] * inv255 - 0.5) * 2.0;
			
			avg = 0;
			avg += in2[4 * (y * width + x) + 0];
			avg += in2[4 * (y * width + x) + 1];
			avg += in2[4 * (y * width + x) + 2];
			avg /= 3;

			*out++ = (byte) (128 + 127 * n[0]);
			*out++ = (byte) (128 + 127 * n[1]);
			*out++ = (byte) (128 + 127 * n[2]);
			*out++ = (byte) (avg);
		}
	}
}

static void R_AddNormals(byte * in, byte * in2, int width, int height)
{
	int             x, y;
	vec3_t          n;
	byte            a;
	vec3_t          n2;
	byte            a2;
	float           inv255 = 1.0f / 255.0f;
	byte           *out;

	out = in;

	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			n[0] = (in[4 * (y * width + x) + 0] * inv255 - 0.5) * 2.0;
			n[1] = (in[4 * (y * width + x) + 1] * inv255 - 0.5) * 2.0;
			n[2] = (in[4 * (y * width + x) + 2] * inv255 - 0.5) * 2.0;
			a = in[4 * (y * width + x) + 3];

			n2[0] = (in2[4 * (y * width + x) + 0] * inv255 - 0.5) * 2.0;
			n2[1] = (in2[4 * (y * width + x) + 1] * inv255 - 0.5) * 2.0;
			n2[2] = (in2[4 * (y * width + x) + 2] * inv255 - 0.5) * 2.0;
			a2 = in2[4 * (y * width + x) + 3];

			VectorAdd(n, n2, n);

			if(!VectorNormalize(n))
				VectorSet(n, 0, 0, 1);

			*out++ = (byte) (128 + 127 * n[0]);
			*out++ = (byte) (128 + 127 * n[1]);
			*out++ = (byte) (128 + 127 * n[2]);
			*out++ = (byte) (Q_bound(0, a + a2, 255));
		}
	}
}

static void R_InvertAlpha(byte * in, int width, int height)
{
	int             x, y;
	byte           *out;

	out = in;

	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			out[4 * (y * width + x) + 3] = 255 - in[4 * (y * width + x) + 3];
		}
	}
}

static void R_InvertColor(byte * in, int width, int height)
{
	int             x, y;
	byte           *out;

	out = in;

	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			out[4 * (y * width + x) + 0] = 255 - in[4 * (y * width + x) + 0];
			out[4 * (y * width + x) + 1] = 255 - in[4 * (y * width + x) + 1];
			out[4 * (y * width + x) + 2] = 255 - in[4 * (y * width + x) + 2];
		}
	}
}

static void R_MakeIntensity(byte * in, int width, int height)
{
	int             x, y;
	byte           *out;
	byte            red;

	out = in;

	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			red = out[4 * (y * width + x) + 0];

			out[4 * (y * width + x) + 1] = red;
			out[4 * (y * width + x) + 2] = red;
			out[4 * (y * width + x) + 3] = red;
		}
	}
}

static void R_MakeAlpha(byte * in, int width, int height)
{
	int             x, y;
	byte           *out;
	int             avg;

	out = in;

	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			avg = 0;
			avg += out[4 * (y * width + x) + 0];
			avg += out[4 * (y * width + x) + 1];
			avg += out[4 * (y * width + x) + 2];
			avg /= 3;

			out[4 * (y * width + x) + 0] = 255;
			out[4 * (y * width + x) + 1] = 255;
			out[4 * (y * width + x) + 2] = 255;
			out[4 * (y * width + x) + 3] = (byte) avg;
		}
	}
}


/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================
*/
static void R_BlendOverTexture(byte * data, int pixelCount, byte blend[4])
{
	int             i;
	int             inverseAlpha;
	int             premult[3];

	inverseAlpha = 255 - blend[3];
	premult[0] = blend[0] * blend[3];
	premult[1] = blend[1] * blend[3];
	premult[2] = blend[2] * blend[3];

	for(i = 0; i < pixelCount; i++, data += 4)
	{
		data[0] = (data[0] * inverseAlpha + premult[0]) >> 9;
		data[1] = (data[1] * inverseAlpha + premult[1]) >> 9;
		data[2] = (data[2] * inverseAlpha + premult[2]) >> 9;
	}
}


byte            mipBlendColors[16][4] = {
	{0, 0, 0, 0}
	,
	{255, 0, 0, 128}
	,
	{0, 255, 0, 128}
	,
	{0, 0, 255, 128}
	,
	{255, 0, 0, 128}
	,
	{0, 255, 0, 128}
	,
	{0, 0, 255, 128}
	,
	{255, 0, 0, 128}
	,
	{0, 255, 0, 128}
	,
	{0, 0, 255, 128}
	,
	{255, 0, 0, 128}
	,
	{0, 255, 0, 128}
	,
	{0, 0, 255, 128}
	,
	{255, 0, 0, 128}
	,
	{0, 255, 0, 128}
	,
	{0, 0, 255, 128}
	,
};


/*
===============
R_UploadImage
===============
*/
extern qboolean charSet;
static void R_UploadImage(const byte ** dataArray, int numData, image_t * image)
{
	int             samples;
	const byte     *data = dataArray[0];
	byte           *scaledBuffer = NULL;
	int             scaledWidth, scaledHeight;
	int             i, c;
	const byte     *scan;
	GLenum          target;
	GLenum          format = GL_RGBA;
	GLenum          internalFormat = GL_RGB;
	float           rMax = 0, gMax = 0, bMax = 0;
	vec4_t          zeroClampBorder = { 0, 0, 0, 1 };
	vec4_t          alphaZeroClampBorder = { 0, 0, 0, 0 };

	if(glConfig2.textureNPOTAvailable)
	{
		scaledWidth = image->width;
		scaledHeight = image->height;
	}
	else
	{
		// convert to exact power of 2 sizes
		for(scaledWidth = 1; scaledWidth < image->width; scaledWidth <<= 1)
			;
		for(scaledHeight = 1; scaledHeight < image->height; scaledHeight <<= 1)
			;
	}

	if(r_roundImagesDown->integer && scaledWidth > image->width)
		scaledWidth >>= 1;
	if(r_roundImagesDown->integer && scaledHeight > image->height)
		scaledHeight >>= 1;

	// perform optional picmip operation
	if(!(image->bits & IF_NOPICMIP))
	{
		scaledWidth >>= r_picmip->integer;
		scaledHeight >>= r_picmip->integer;
	}

	// clamp to minimum size
	if(scaledWidth < 1)
	{
		scaledWidth = 1;
	}
	if(scaledHeight < 1)
	{
		scaledHeight = 1;
	}

	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	if(image->type == GL_TEXTURE_CUBE_MAP_ARB || (image->bits & IF_CUBEMAP))
	{
		while(scaledWidth > glConfig2.maxCubeMapTextureSize || scaledHeight > glConfig2.maxCubeMapTextureSize)
		{
			scaledWidth >>= 1;
			scaledHeight >>= 1;
		}
	}
	else
	{
		while(scaledWidth > glConfig.maxTextureSize || scaledHeight > glConfig.maxTextureSize)
		{
			scaledWidth >>= 1;
			scaledHeight >>= 1;
		}
	}

	scaledBuffer = ri.Hunk_AllocateTempMemory(sizeof(byte) * scaledWidth * scaledHeight * 4);

	// set target
	switch (image->type)
	{
		case GL_TEXTURE_CUBE_MAP_ARB:
			target = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
			break;

		default:
			target = GL_TEXTURE_2D;
			break;
	}

	// scan the texture for each channel's max values
	// and verify if the alpha channel is being used or not
	c = scaledWidth * scaledHeight;
	scan = data;
	samples = 3;
	if(!(image->bits & IF_LIGHTMAP))
	{
		// Tr3B: normalmaps have the displacement maps in the alpha channel
		// samples 3 would cause an opaque alpha channel and odd displacements!
		if(image->bits & IF_NORMALMAP)
		{
			samples = 4;	
		}
		else
		{		
			for(i = 0; i < c; i++)
			{
				if(scan[i * 4 + 0] > rMax)
				{
					rMax = scan[i * 4 + 0];
				}
				if(scan[i * 4 + 1] > gMax)
				{
					gMax = scan[i * 4 + 1];
				}
				if(scan[i * 4 + 2] > bMax)
				{
					bMax = scan[i * 4 + 2];
				}
				if(scan[i * 4 + 3] != 255)
				{
					samples = 4;
					break;
				}
			}
		}
	
		// select proper internal format
		if(samples == 3)
		{
			if(glConfig.textureCompression == TC_S3TC)
			{
				internalFormat = GL_RGB4_S3TC;
			}
			else if(r_texturebits->integer == 16)
			{
				internalFormat = GL_RGB5;
			}
			else if(r_texturebits->integer == 32)
			{
				internalFormat = GL_RGB8;
			}
			else
			{
				internalFormat = 3;
			}
		}
		else if(samples == 4)
		{
			if(image->bits & IF_INTENSITY)
			{
				internalFormat = GL_INTENSITY8;
			}
			else if(image->bits & IF_ALPHA)
			{
				internalFormat = GL_ALPHA8;
			}
			else if(r_texturebits->integer == 16)
			{
				internalFormat = GL_RGBA4;
			}
			else if(r_texturebits->integer == 32)
			{
				internalFormat = GL_RGBA8;
			}
			else
			{
				internalFormat = 4;
			}
		}
	}
	else
	{
		internalFormat = 3;
	}

	for(i = 0; i < numData; i++)
	{
		data = dataArray[i];

		// copy or resample data as appropriate for first MIP level
		if((scaledWidth == image->width) && (scaledHeight == image->height))
		{
			Com_Memcpy(scaledBuffer, data, scaledWidth * scaledHeight * 4);
		}
		else
		{
			ResampleTexture((unsigned *)data, image->width, image->height, (unsigned *)scaledBuffer, scaledWidth, scaledHeight,
							(image->bits & IF_NORMALMAP));
		}

		if(!(image->bits & IF_NORMALMAP))
		{
			R_LightScaleTexture((unsigned *)scaledBuffer, scaledWidth, scaledHeight, image->filterType == FT_DEFAULT);
		}

		image->uploadWidth = scaledWidth;
		image->uploadHeight = scaledHeight;
		image->internalFormat = internalFormat;

		if(image->filterType == FT_DEFAULT && glConfig2.generateMipmapAvailable)
		{
			// raynorpat: if hardware mipmap generation is available, use it
			qglHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST); // make sure its nice
			qglTexParameteri(image->type, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
			qglTexParameteri(image->type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // default to trilinear
		}

		switch (image->type)
		{
			case GL_TEXTURE_CUBE_MAP_ARB:
				qglTexImage2D(target + i, 0, internalFormat, scaledWidth, scaledHeight, 0, format, GL_UNSIGNED_BYTE,
							  scaledBuffer);
				break;

			default:
				qglTexImage2D(target, 0, internalFormat, scaledWidth, scaledHeight, 0, format, GL_UNSIGNED_BYTE, scaledBuffer);
				break;
		}

		if(!glConfig2.generateMipmapAvailable)
		{
			if(image->filterType == FT_DEFAULT)
			{
				int             mipLevel;
				int             mipWidth, mipHeight;
	
				mipLevel = 0;
				mipWidth = scaledWidth;
				mipHeight = scaledHeight;
	
				while(mipWidth > 1 || mipHeight > 1)
				{
					if(image->bits & IF_NORMALMAP)
						R_MipNormalMap(scaledBuffer, mipWidth, mipHeight);
					else
						R_MipMap(scaledBuffer, mipWidth, mipHeight);
	
					mipWidth >>= 1;
					mipHeight >>= 1;
	
					if(mipWidth < 1)
						mipWidth = 1;
	
					if(mipHeight < 1)
						mipHeight = 1;
	
					mipLevel++;
	
					if(r_colorMipLevels->integer && !(image->bits & IF_NORMALMAP))
					{
						R_BlendOverTexture(scaledBuffer, mipWidth * mipHeight, mipBlendColors[mipLevel]);
					}
	
					switch (image->type)
					{
						case GL_TEXTURE_CUBE_MAP_ARB:
							qglTexImage2D(target + i, mipLevel, internalFormat, mipWidth, mipHeight, 0, format, GL_UNSIGNED_BYTE,
										  scaledBuffer);
							break;
	
						default:
							qglTexImage2D(target, mipLevel, internalFormat, mipWidth, mipHeight, 0, format, GL_UNSIGNED_BYTE,
										  scaledBuffer);
							break;
					}
				}
			}
		}
	}

	// set filter type
	switch (image->filterType)
	{
			case FT_DEFAULT:
				// set texture anisotropy
				if(glConfig2.textureAnisotropyAvailable)
					qglTexParameterf(image->type, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_texture_filter_anisotropic->value);

				qglTexParameterf(image->type, GL_TEXTURE_MIN_FILTER, gl_filter_min);
				qglTexParameterf(image->type, GL_TEXTURE_MAG_FILTER, gl_filter_max);
				break;

			case FT_LINEAR:
				qglTexParameterf(image->type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				qglTexParameterf(image->type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				break;

			case FT_NEAREST:
				qglTexParameterf(image->type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				qglTexParameterf(image->type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				break;

			default:
				ri.Printf(PRINT_WARNING, "WARNING: unknown filter type for image '%s'\n", image->name);
				qglTexParameterf(image->type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				qglTexParameterf(image->type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				break;
	}
	// set wrap type
	switch (image->wrapType)
	{
			case WT_REPEAT:
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_S, GL_REPEAT);
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_T, GL_REPEAT);
				break;

			case WT_CLAMP:
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_S, GL_CLAMP);
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_T, GL_CLAMP);
				break;

			case WT_EDGE_CLAMP:
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				break;

			case WT_ZERO_CLAMP:
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
				qglTexParameterfv(image->type, GL_TEXTURE_BORDER_COLOR, zeroClampBorder);
				break;

			case WT_ALPHA_ZERO_CLAMP:
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
				qglTexParameterfv(image->type, GL_TEXTURE_BORDER_COLOR, alphaZeroClampBorder);
				break;

			default:
				ri.Printf(PRINT_WARNING, "WARNING: unknown wrap type for image '%s'\n", image->name);
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_S, GL_REPEAT);
				qglTexParameterf(image->type, GL_TEXTURE_WRAP_T, GL_REPEAT);
				break;
	}

	GL_CheckErrors();

	if(scaledBuffer != 0)
		ri.Hunk_FreeTempMemory(scaledBuffer);
}



/*
================
R_CreateImage
================
*/
image_t        *R_CreateImage(const char *name,
							  const byte * pic, int width, int height, int bits, filterType_t filterType, wrapType_t wrapType)
{
	image_t        *image;
	long            hash;
	char            buffer[1024];

//  if(strlen(name) >= MAX_QPATH)
	if(strlen(name) >= 1024)
	{
		ri.Error(ERR_DROP, "R_CreateImage: \"%s\" is too long\n", name);
	}

	if(tr.numImages == MAX_DRAWIMAGES)
	{
		ri.Error(ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit\n");
	}

	image = tr.images[tr.numImages] = ri.Hunk_Alloc(sizeof(image_t), h_low);
	image->texnum = 1024 + tr.numImages;
	tr.numImages++;

	Q_strncpyz(image->name, name, sizeof(image->name));
	image->type = GL_TEXTURE_2D;

	image->width = width;
	image->height = height;

	image->bits = bits;
	image->filterType = filterType;
	image->wrapType = wrapType;

	if(!strncmp(name, "_lightmap", 9))
	{
		image->bits |= IF_LIGHTMAP;
	}

	GL_Bind(image);

	R_UploadImage(&pic, 1, image);

	qglBindTexture(image->type, 0);

	Q_strncpyz(buffer, name, sizeof(buffer));
	hash = generateHashValue(buffer);
	image->next = hashTable[hash];
	hashTable[hash] = image;

	return image;
}

/*
================
R_CreateCubeImage
================
*/
image_t        *R_CreateCubeImage(const char *name,
								  const byte * pic[6],
								  int width, int height, int bits, filterType_t filterType, wrapType_t wrapType)
{
#if 1
	image_t        *image;
	long            hash;
	char            buffer[1024];

//  if(strlen(name) >= MAX_QPATH)
	if(strlen(name) >= 1024)
	{
		ri.Error(ERR_DROP, "R_CreateCubeImage: \"%s\" is too long\n", name);
	}

	if(tr.numImages == MAX_DRAWIMAGES)
	{
		ri.Error(ERR_DROP, "R_CreateCubeImage: MAX_DRAWIMAGES hit\n");
	}

	image = tr.images[tr.numImages] = ri.Hunk_Alloc(sizeof(image_t), h_low);
	image->texnum = 1024 + tr.numImages;
	tr.numImages++;

	Q_strncpyz(image->name, name, sizeof(image->name));
	image->type = GL_TEXTURE_CUBE_MAP_ARB;

	image->width = width;
	image->height = height;

	image->bits = bits;
	image->filterType = filterType;
	image->wrapType = wrapType;

	if(!strncmp(name, "_lightmap", 9))
	{
		image->bits |= IF_LIGHTMAP;
	}

	GL_Bind(image);

	R_UploadImage(pic, 6, image);

	qglBindTexture(image->type, 0);

	Q_strncpyz(buffer, name, sizeof(buffer));
	hash = generateHashValue(buffer);
	image->next = hashTable[hash];
	hashTable[hash] = image;

	return image;
#else
	return NULL;
#endif
}

//===================================================================

static void R_LoadImage(char **buffer, byte ** pic, int *width, int *height, int *bits);

static void ParseHeightMap(char **text, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;
	float           scale;

	token = COM_ParseExt(text, qfalse);
	if(token[0] != '(')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '(', found '%s' for heightMap\n", token);
		return;
	}

	R_LoadImage(text, pic, width, height, bits);
	if(!pic)
	{
		ri.Printf(PRINT_WARNING, "WARNING: failed loading of image for heightMap\n", token);
		return;
	}

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ',')
	{
		ri.Printf(PRINT_WARNING, "WARNING: no matching ',' found\n");
		return;
	}

	token = COM_ParseExt(text, qfalse);
	scale = atof(token);

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ')')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting ')', found '%s' for heightMap\n", token);
		return;
	}

	R_HeightMapToNormalMap(*pic, *width, *height, scale);

	*bits &= ~IF_INTENSITY;
	*bits &= ~IF_ALPHA;
	*bits |= IF_NORMALMAP;
}

static void ParseDisplaceMap(char **text, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;
	byte           *pic2;
	int             width2, height2;

	token = COM_ParseExt(text, qfalse);
	if(token[0] != '(')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '(', found '%s' for displaceMap\n", token);
		return;
	}

	R_LoadImage(text, pic, width, height, bits);
	if(!pic)
	{
		ri.Printf(PRINT_WARNING, "WARNING: failed loading of first image for displaceMap\n");
		return;
	}

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ',')
	{
		ri.Printf(PRINT_WARNING, "WARNING: no matching ',' found\n");
		return;
	}

	R_LoadImage(text, &pic2, &width2, &height2, bits);
	if(!pic2)
	{
		ri.Printf(PRINT_WARNING, "WARNING: failed loading of second image for displaceMap\n");
		return;
	}

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ')')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting ')', found '%s' for displaceMap\n", token);
	}

	if(*width != width2 || *height != height2)
	{
		ri.Printf(PRINT_WARNING, "WARNING: images for displaceMap have different dimensions (%i x %i != %i x %i)\n",
				  *width, *height, width2, height2);

		//ri.Free(*pic);
		//*pic = NULL;

		ri.Free(pic2);
		return;
	}

	R_DisplaceMap(*pic, pic2, *width, *height);

	ri.Free(pic2);

	*bits &= ~IF_INTENSITY;
	*bits &= ~IF_ALPHA;
	*bits |= IF_NORMALMAP;
}

static void ParseAddNormals(char **text, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;
	byte           *pic2;
	int             width2, height2;

	token = COM_ParseExt(text, qfalse);
	if(token[0] != '(')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '(', found '%s' for addNormals\n", token);
		return;
	}

	R_LoadImage(text, pic, width, height, bits);
	if(!pic)
	{
		ri.Printf(PRINT_WARNING, "WARNING: failed loading of first image for addNormals\n");
		return;
	}

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ',')
	{
		ri.Printf(PRINT_WARNING, "WARNING: no matching ',' found\n");
		return;
	}

	R_LoadImage(text, &pic2, &width2, &height2, bits);
	if(!pic2)
	{
		ri.Printf(PRINT_WARNING, "WARNING: failed loading of second image for addNormals\n");
		return;
	}

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ')')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting ')', found '%s' for addNormals\n", token);
	}

	if(*width != width2 || *height != height2)
	{
		ri.Printf(PRINT_WARNING, "WARNING: images for addNormals have different dimensions (%i x %i != %i x %i)\n",
				  *width, *height, width2, height2);

		//ri.Free(*pic);
		//*pic = NULL;

		ri.Free(pic2);
		return;
	}

	R_AddNormals(*pic, pic2, *width, *height);

	ri.Free(pic2);

	*bits &= ~IF_INTENSITY;
	*bits &= ~IF_ALPHA;
	*bits |= IF_NORMALMAP;
}

static void ParseInvertAlpha(char **text, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;

	token = COM_ParseExt(text, qfalse);
	if(token[0] != '(')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '(', found '%s' for invertAlpha\n", token);
		return;
	}

	R_LoadImage(text, pic, width, height, bits);
	if(!pic)
	{
		ri.Printf(PRINT_WARNING, "WARNING: failed loading of image for invertAlpha\n");
		return;
	}

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ')')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting ')', found '%s' for invertAlpha\n", token);
		return;
	}

	R_InvertAlpha(*pic, *width, *height);
}

static void ParseInvertColor(char **text, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;

	token = COM_ParseExt(text, qfalse);
	if(token[0] != '(')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '(', found '%s' for invertColor\n", token);
		return;
	}

	R_LoadImage(text, pic, width, height, bits);
	if(!pic)
	{
		ri.Printf(PRINT_WARNING, "WARNING: failed loading of image for invertColor\n");
		return;
	}

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ')')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting ')', found '%s' for invertColor\n", token);
		return;
	}

	R_InvertColor(*pic, *width, *height);
}

static void ParseMakeIntensity(char **text, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;

	token = COM_ParseExt(text, qfalse);
	if(token[0] != '(')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '(', found '%s' for makeIntensity\n", token);
		return;
	}

	R_LoadImage(text, pic, width, height, bits);
	if(!pic)
	{
		ri.Printf(PRINT_WARNING, "WARNING: failed loading of image for makeIntensity\n");
		return;
	}

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ')')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting ')', found '%s' for makeIntensity\n", token);
		return;
	}

	R_MakeIntensity(*pic, *width, *height);

	*bits |= IF_INTENSITY;
	*bits &= ~IF_ALPHA;
	*bits &= ~IF_NORMALMAP;
}

static void ParseMakeAlpha(char **text, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;

	token = COM_ParseExt(text, qfalse);
	if(token[0] != '(')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '(', found '%s' for makeAlpha\n", token);
		return;
	}

	R_LoadImage(text, pic, width, height, bits);
	if(!pic)
	{
		ri.Printf(PRINT_WARNING, "WARNING: failed loading of image for makeAlpha\n");
		return;
	}

	token = COM_ParseExt(text, qfalse);
	if(token[0] != ')')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting ')', found '%s' for makeAlpha\n", token);
		return;
	}

	R_MakeAlpha(*pic, *width, *height);

	*bits &= ~IF_INTENSITY;
	*bits |= IF_ALPHA;
	*bits &= IF_NORMALMAP;
}

/*
=================
R_LoadImage

Loads any of the supported image types into a cannonical
32 bit format.
=================
*/
typedef struct
{
	char *ext;
	void (*ImageLoader)( const char *, unsigned char **, int *, int *, byte );
} imageExtToLoaderMap_t;

void R_LoadTGA(const char *name, byte **pic, int *width, int *height, byte alphaByte);
void R_LoadJPG(const char *name, byte **pic, int *width, int *height, byte alphaByte );
void R_LoadPNG(const char *name, byte **pic, int *width, int *height, byte alphaByte);

// Note that the ordering indicates the order of preference used
// when there are multiple images of different formats available
static imageExtToLoaderMap_t imageLoaders[ ] =
{
	{ "tga",  R_LoadTGA },
	{ "jpg",  R_LoadJPG },
	{ "jpeg", R_LoadJPG },
	{ "png",  R_LoadPNG }
};

static int numImageLoaders = sizeof( imageLoaders ) / sizeof( imageLoaders[ 0 ] );

static void R_LoadImage(char **buffer, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;

	*pic = NULL;
	*width = 0;
	*height = 0;

	token = COM_ParseExt(buffer, qfalse);
	if(!token[0])
	{
		ri.Printf(PRINT_WARNING, "WARNING: NULL parameter for R_LoadImage\n");
		return;
	}

//  ri.Printf(PRINT_ALL, "R_LoadImage: token '%s'\n", token);

	// heightMap(<map>, <float>)  Turns a grayscale height map into a normal map. <float> varies the bumpiness
	if(!Q_stricmp(token, "heightMap"))
	{
		ParseHeightMap(buffer, pic, width, height, bits);
	}
	// displaceMap(<map>, <map>)  Sets the alpha channel to an average of the second image's RGB channels.
	else if(!Q_stricmp(token, "displaceMap"))
	{
		ParseDisplaceMap(buffer, pic, width, height, bits);
	}
	// addNormals(<map>, <map>)  Adds two normal maps together. Result is normalized.
	else if(!Q_stricmp(token, "addNormals"))
	{
		ParseAddNormals(buffer, pic, width, height, bits);
	}
	// smoothNormals(<map>)  Does a box filter on the normal map, and normalizes the result.
	else if(!Q_stricmp(token, "smoothNormals"))
	{
		ri.Printf(PRINT_WARNING, "WARNING: smoothNormals(<map>) keyword not supported\n");
	}
	// add(<map>, <map>)  Adds two images without normalizing the result
	else if(!Q_stricmp(token, "add"))
	{
		ri.Printf(PRINT_WARNING, "WARNING: add(<map>, <map>) keyword not supported\n");
	}
	// scale(<map>, <float> [,float] [,float] [,float])  Scales the RGBA by the specified factors. Defaults to 0.
	else if(!Q_stricmp(token, "scale"))
	{
		ri.Printf(PRINT_WARNING, "WARNING: scale(<map>, <float> [,float] [,float] [,float]) keyword not supported\n");
	}
	// invertAlpha(<map>)  Inverts the alpha channel (0 becomes 1, 1 becomes 0)
	else if(!Q_stricmp(token, "invertAlpha"))
	{
		ParseInvertAlpha(buffer, pic, width, height, bits);
	}
	// invertColor(<map>)  Inverts the R, G, and B channels
	else if(!Q_stricmp(token, "invertColor"))
	{
		ParseInvertColor(buffer, pic, width, height, bits);
	}
	// makeIntensity(<map>)  Copies the red channel to the G, B, and A channels
	else if(!Q_stricmp(token, "makeIntensity"))
	{
		ParseMakeIntensity(buffer, pic, width, height, bits);
	}
	// makeAlpha(<map>)  Sets the alpha channel to an average of the RGB channels. Sets the RGB channels to white.
	else if(!Q_stricmp(token, "makeAlpha"))
	{
		ParseMakeAlpha(buffer, pic, width, height, bits);
	}
	else
	{
		qboolean		orgNameFailed = qfalse;
		int				i;
		char			localName[ MAX_QPATH ];
		const char	   *ext;
		byte            alphaByte;
	
		ri.Printf( PRINT_DEVELOPER, "Loading image '%s' as a normal image\n", localName );

		// Tr3B: clear alpha of normalmaps for displacement mapping
		if(*bits & IF_NORMALMAP)
			alphaByte = 0x00;
		else
			alphaByte = 0xFF;

		Q_strncpyz( localName, token, MAX_QPATH );

		ext = COM_GetExtension( localName );

		if( *ext )
		{
			// Look for the correct loader and use it
			for( i = 0; i < numImageLoaders; i++ )
			{
				if( !Q_stricmp( ext, imageLoaders[ i ].ext ) )
				{
					// Load
					imageLoaders[ i ].ImageLoader( localName, pic, width, height, alphaByte );
					break;
				}
			}

			// A loader was found
			if( i < numImageLoaders )
			{
				if( *pic == NULL )
				{
					ri.Printf( PRINT_DEVELOPER, "Image loader failed, retrying with no extension\n" );
					// Loader failed, most likely because the file isn't there;
					// try again without the extension
					orgNameFailed = qtrue;
					COM_StripExtension( token, localName, MAX_QPATH );
				}
				else
				{
					// Something loaded
					return;
				}
			} else {
				// no loader was found
				ri.Printf( PRINT_DEVELOPER, "WARNING: couldn't find an image loader for a file of extension %s\n", ext);
			}
		}

		// Try and find a suitable match using all
		// the image formats supported
		for( i = 0; i < numImageLoaders; i++ )
		{
			char *altName = va( "%s.%s", localName, imageLoaders[ i ].ext );

			// Load
			ri.Printf( PRINT_DEVELOPER, "Testing image load for '%s' extension\n", imageLoaders[ i ].ext );
			imageLoaders[ i ].ImageLoader( altName, pic, width, height, alphaByte );

			if( *pic )
			{
				if( orgNameFailed )
					ri.Printf( PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n", token, altName );
				break;
			}
		}
	}
}



/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
image_t        *R_FindImageFile(const char *name, int bits, filterType_t filterType, wrapType_t wrapType)
{
	image_t        *image = NULL;
	int             width = 0, height = 0;
	byte           *pic = NULL;
	long            hash;
	char            buffer[1024];
	char           *buffer_p;
	unsigned long   diff;

	if(!name)
	{
		return NULL;
	}

	Q_strncpyz(buffer, name, sizeof(buffer));
	hash = generateHashValue(buffer);

//  ri.Printf(PRINT_ALL, "R_FindImageFile: buffer '%s'\n", buffer);

	// see if the image is already loaded
	for(image = hashTable[hash]; image; image = image->next)
	{
		if(!Q_stricmpn(buffer, image->name, sizeof(image->name)))
		{
			// the white image can be used with any set of parms, but other mismatches are errors
			if(Q_stricmp(buffer, "_white"))
			{
				diff = bits ^ image->bits;

				/*
				   if(diff & IF_NOMIPMAPS)
				   {
				   ri.Printf(PRINT_DEVELOPER, "WARNING: reused image %s with mixed mipmap parm\n", name);
				   }
				 */

				if(diff & IF_NOPICMIP)
				{
					ri.Printf(PRINT_DEVELOPER, "WARNING: reused image %s with mixed allowPicmip parm\n", name);
				}

				if(image->wrapType != wrapType)
				{
					ri.Printf(PRINT_ALL, "WARNING: reused image %s with mixed glWrapType parm\n", name);
				}
			}
			ri.Printf(PRINT_DEVELOPER, "Using image from hash table\n");
			return image;
		}
	}

	// load the pic from disk
	buffer_p = &buffer[0];
	R_LoadImage(&buffer_p, &pic, &width, &height, &bits);
	if(pic == NULL)
	{
		ri.Printf(PRINT_DEVELOPER, "WARNING: R_LoadImage returned a NULL pic\n");
		return NULL;
	}

	image = R_CreateImage((char *)buffer, pic, width, height, bits, filterType, wrapType);
	ri.Free(pic);
	return image;
}


/*
===============
R_FindCubeImage

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
image_t        *R_FindCubeImage(const char *name, int bits, filterType_t filterType, wrapType_t wrapType)
{
	int             i;
	image_t        *image = NULL;
	int             width = 0, height = 0;
	byte           *pic[6];
	long            hash;
	static char    *suf[6] = { "px", "nx", "py", "ny", "pz", "nz" };
	char            filename[MAX_QPATH];

	if(!name)
	{
		return NULL;
	}

	Q_strncpyz(filename, name, sizeof(filename));
	hash = generateHashValue(filename);

	// see if the image is already loaded
	for(image = hashTable[hash]; image; image = image->next)
	{
		if(!Q_stricmp(name, image->name))
		{
			return image;
		}
	}

	for(i = 0; i < 6; i++)
	{
		pic[i] = NULL;
	}

	// load the pic from disk
	for(i = 0; i < 6; i++)
	{
		Com_sprintf(filename, sizeof(filename), "%s_%s.tga", name, suf[i]);
		R_LoadTGA(filename, &pic[i], &width, &height, 0xFF);

		if(!pic[i] || width != height)
		{
			image = NULL;
			goto done;
		}
	}

	image = R_CreateCubeImage((char *)name, (const byte **)pic, width, height, bits, filterType, wrapType);

  done:
	for(i = 0; i < 6; i++)
	{
		if(pic[i])
			ri.Free(pic[i]);
	}
	return image;
}

/*
=================
R_InitFogTable
=================
*/
void R_InitFogTable(void)
{
	int             i;
	float           d;
	float           exp;

	exp = 0.5;

	for(i = 0; i < FOG_TABLE_SIZE; i++)
	{
		d = pow((float)i / (FOG_TABLE_SIZE - 1), exp);

		tr.fogTable[i] = d;
	}
}


/*
================
R_FogFactor

Returns a 0.0 to 1.0 fog density value
This is called for each texel of the fog texture on startup
and for each vertex of transparent shaders in fog dynamically
================
*/
float R_FogFactor(float s, float t)
{
	float           d;

	s -= 1.0 / 512;
	if(s < 0)
	{
		return 0;
	}
	if(t < 1.0 / 32)
	{
		return 0;
	}
	if(t < 31.0 / 32)
	{
		s *= (t - 1.0f / 32.0f) / (30.0f / 32.0f);
	}

	// we need to leave a lot of clamp range
	s *= 8;

	if(s > 1.0)
	{
		s = 1.0;
	}

	d = tr.fogTable[(int)(s * (FOG_TABLE_SIZE - 1))];

	return d;
}


/*
================
R_CreateFogImage
================
*/
#define	FOG_S	256
#define	FOG_T	32
static void R_CreateFogImage(void)
{
	int             x, y;
	byte           *data;
	float           g;
	float           d;
	float           borderColor[4];

	data = ri.Hunk_AllocateTempMemory(FOG_S * FOG_T * 4);

	g = 2.0;

	// S is distance, T is depth
	for(x = 0; x < FOG_S; x++)
	{
		for(y = 0; y < FOG_T; y++)
		{
			d = R_FogFactor((x + 0.5f) / FOG_S, (y + 0.5f) / FOG_T);

			data[(y * FOG_S + x) * 4 + 0] = data[(y * FOG_S + x) * 4 + 1] = data[(y * FOG_S + x) * 4 + 2] = 255;
			data[(y * FOG_S + x) * 4 + 3] = 255 * d;
		}
	}
	// standard openGL clamping doesn't really do what we want -- it includes
	// the border color at the edges.  OpenGL 1.2 has clamp-to-edge, which does
	// what we want.
	tr.fogImage = R_CreateImage("_fog", (byte *) data, FOG_S, FOG_T, IF_NOPICMIP, FT_LINEAR, WT_CLAMP);
	ri.Hunk_FreeTempMemory(data);

	borderColor[0] = 1.0;
	borderColor[1] = 1.0;
	borderColor[2] = 1.0;
	borderColor[3] = 1;

	qglTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
}


/*
==================
R_CreateDefaultImage
==================
*/
#define	DEFAULT_SIZE	128
static void R_CreateDefaultImage(void)
{
	int             x;
	byte            data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// the default image will be a box, to allow you to see the mapping coordinates
	Com_Memset(data, 32, sizeof(data));
	for(x = 0; x < DEFAULT_SIZE; x++)
	{
		data[0][x][0] = data[0][x][1] = data[0][x][2] = data[0][x][3] = 255;
		data[x][0][0] = data[x][0][1] = data[x][0][2] = data[x][0][3] = 255;

		data[DEFAULT_SIZE - 1][x][0] =
			data[DEFAULT_SIZE - 1][x][1] = data[DEFAULT_SIZE - 1][x][2] = data[DEFAULT_SIZE - 1][x][3] = 255;

		data[x][DEFAULT_SIZE - 1][0] =
			data[x][DEFAULT_SIZE - 1][1] = data[x][DEFAULT_SIZE - 1][2] = data[x][DEFAULT_SIZE - 1][3] = 255;
	}
	tr.defaultImage = R_CreateImage("_default", (byte *) data, DEFAULT_SIZE, DEFAULT_SIZE, IF_NOPICMIP, FT_DEFAULT, WT_REPEAT);
}

static void R_CreateNoFalloffImage(void)
{
	byte            data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// we use a solid white image instead of disabling texturing
	Com_Memset(data, 255, sizeof(data));
	tr.noFalloffImage = R_CreateImage("_noFalloff", (byte *) data, 8, 8, IF_NOPICMIP, FT_LINEAR, WT_EDGE_CLAMP);
}

#define	ATTENUATION_XY_SIZE	128
static void R_CreateAttenuationXYImage(void)
{
	int             x, y;
	byte            data[ATTENUATION_XY_SIZE][ATTENUATION_XY_SIZE][4];
	int             b;

	// make a centered inverse-square falloff blob for dynamic lighting
	for(x = 0; x < ATTENUATION_XY_SIZE; x++)
	{
		for(y = 0; y < ATTENUATION_XY_SIZE; y++)
		{
			float           d;

			d = (ATTENUATION_XY_SIZE / 2 - 0.5f - x) * (ATTENUATION_XY_SIZE / 2 - 0.5f - x) +
				(ATTENUATION_XY_SIZE / 2 - 0.5f - y) * (ATTENUATION_XY_SIZE / 2 - 0.5f - y);
			b = 4000 / d;
			if(b > 255)
			{
				b = 255;
			}
			else if(b < 75)
			{
				b = 0;
			}
			data[y][x][0] = data[y][x][1] = data[y][x][2] = b;
			data[y][x][3] = 255;
		}
	}
	tr.attenuationXYImage =
		R_CreateImage("_attenuationXY", (byte *) data, ATTENUATION_XY_SIZE, ATTENUATION_XY_SIZE, IF_NOPICMIP, FT_LINEAR,
					  WT_CLAMP);
}

static void R_CreateCurrentRenderImage(void)
{
	int             width, height;
	byte           *data;

	for(width = 1; width < glConfig.vidWidth; width <<= 1)
		;
	for(height = 1; height < glConfig.vidHeight; height <<= 1)
		;

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	tr.currentRenderImage = R_CreateImage("_currentRender", data, width, height, IF_NOPICMIP, FT_DEFAULT, WT_REPEAT);

	ri.Hunk_FreeTempMemory(data);
}

static void R_CreateCurrentRenderLinearImage(void)
{
	int             width, height;
	byte           *data;

	for(width = 1; width < glConfig.vidWidth; width <<= 1)
		;
	for(height = 1; height < glConfig.vidHeight; height <<= 1)
		;

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	tr.currentRenderLinearImage = R_CreateImage("_currentRenderLinear", data, width, height, IF_NOPICMIP, FT_LINEAR, WT_REPEAT);

	ri.Hunk_FreeTempMemory(data);
}

static void R_CreateCurrentRenderNearestImage(void)
{
	int             width, height;
	byte           *data;

	for(width = 1; width < glConfig.vidWidth; width <<= 1)
		;
	for(height = 1; height < glConfig.vidHeight; height <<= 1)
		;

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	tr.currentRenderNearestImage =
		R_CreateImage("_currentRenderNearest", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);

	ri.Hunk_FreeTempMemory(data);
}

/*
==================
R_CreateBuiltinImages
==================
*/
void R_CreateBuiltinImages(void)
{
	int             x, y;
	byte            data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	R_CreateDefaultImage();

	// we use a solid white image instead of disabling texturing
	Com_Memset(data, 255, sizeof(data));
	tr.whiteImage = R_CreateImage("_white", (byte *) data, 8, 8, IF_NOPICMIP, FT_LINEAR, WT_REPEAT);

	// we use a solid black image instead of disabling texturing
	Com_Memset(data, 0, sizeof(data));
	tr.blackImage = R_CreateImage("_black", (byte *) data, 8, 8, IF_NOPICMIP, FT_LINEAR, WT_REPEAT);

	// generate a default normalmap with a zero heightmap
	for(x = 0; x < DEFAULT_SIZE; x++)
	{
		for(y = 0; y < DEFAULT_SIZE; y++)
		{
			data[y][x][0] = 128;
			data[y][x][1] = 128;
			data[y][x][2] = 255;
			data[y][x][3] = 0;
		}
	}
	tr.flatImage = R_CreateImage("_flat", (byte *) data, 8, 8, IF_NOPICMIP | IF_NORMALMAP, FT_LINEAR, WT_REPEAT);

	// with overbright bits active, we need an image which is some fraction of full color,
	// for default lightmaps, etc
	for(x = 0; x < 16; x++)
	{
		for(y = 0; y < 16; y++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = tr.identityLightByte;
			data[y][x][3] = 255;
		}
	}

	tr.identityLightImage = R_CreateImage("_identityLight", (byte *) data, 8, 8, IF_NOPICMIP, FT_LINEAR, WT_REPEAT);


	for(x = 0; x < 32; x++)
	{
		// scratchimage is usually used for cinematic drawing
		tr.scratchImage[x] = R_CreateImage("_scratch", (byte *) data, DEFAULT_SIZE, DEFAULT_SIZE, IF_NONE, FT_LINEAR, WT_CLAMP);
	}

	R_CreateFogImage();
	R_CreateNoFalloffImage();
	R_CreateAttenuationXYImage();
	R_CreateCurrentRenderImage();
	R_CreateCurrentRenderLinearImage();
	R_CreateCurrentRenderNearestImage();
}




/*
===============
R_SetColorMappings
===============
*/
void R_SetColorMappings(void)
{
	int             i, j;
	float           g;
	int             inf;
	int             shift;

	// setup the overbright lighting
	tr.overbrightBits = r_overBrightBits->integer;
	if(!glConfig.deviceSupportsGamma)
	{
		tr.overbrightBits = 0;	// need hardware gamma for overbright
	}

	// never overbright in windowed mode
	if(!glConfig.isFullscreen)
	{
		tr.overbrightBits = 0;
	}

	// allow 2 overbright bits in 24 bit, but only 1 in 16 bit
	if(glConfig.colorBits > 16)
	{
		if(tr.overbrightBits > 2)
		{
			tr.overbrightBits = 2;
		}
	}
	else
	{
		if(tr.overbrightBits > 1)
		{
			tr.overbrightBits = 1;
		}
	}
	if(tr.overbrightBits < 0)
	{
		tr.overbrightBits = 0;
	}

	tr.identityLight = 1.0f / (1 << tr.overbrightBits);
	tr.identityLightByte = 255 * tr.identityLight;


	if(r_intensity->value <= 1)
	{
		ri.Cvar_Set("r_intensity", "1");
	}

	if(r_gamma->value < 0.5f)
	{
		ri.Cvar_Set("r_gamma", "0.5");
	}
	else if(r_gamma->value > 3.0f)
	{
		ri.Cvar_Set("r_gamma", "3.0");
	}

	g = r_gamma->value;

	shift = tr.overbrightBits;

	for(i = 0; i < 256; i++)
	{
		if(g == 1)
		{
			inf = i;
		}
		else
		{
			inf = 255 * pow(i / 255.0f, 1.0f / g) + 0.5f;
		}
		inf <<= shift;
		if(inf < 0)
		{
			inf = 0;
		}
		if(inf > 255)
		{
			inf = 255;
		}
		s_gammatable[i] = inf;
	}

	for(i = 0; i < 256; i++)
	{
		j = i * r_intensity->value;
		if(j > 255)
		{
			j = 255;
		}
		s_intensitytable[i] = j;
	}

	if(glConfig.deviceSupportsGamma)
	{
		GLimp_SetGamma(s_gammatable, s_gammatable, s_gammatable);
	}
}


/*
===============
R_InitImages
===============
*/
void R_InitImages(void)
{
	Com_Memset(hashTable, 0, sizeof(hashTable));
	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();
}


/*
===============
R_ShutdownImages
===============
*/
void R_ShutdownImages(void)
{
	int             i;

	for(i = 0; i < tr.numImages; i++)
	{
		qglDeleteTextures(1, &tr.images[i]->texnum);
	}
	Com_Memset(tr.images, 0, sizeof(tr.images));

	tr.numImages = 0;

	Com_Memset(glState.currenttextures, 0, sizeof(glState.currenttextures));
	if(qglBindTexture)
	{
		if(qglActiveTextureARB)
		{
			for(i = 8 - 1; i >= 0; i--)
			{
				GL_SelectTexture(i);
				qglBindTexture(GL_TEXTURE_2D, 0);
			}
		}
		else
		{
			qglBindTexture(GL_TEXTURE_2D, 0);
		}
	}
}


/*
============================================================================

SKINS

============================================================================
*/

/*
==================
CommaParse

This is unfortunate, but the skin files aren't
compatable with our normal parsing rules.
==================
*/
static char    *CommaParse(char **data_p)
{
	int             c = 0, len;
	char           *data;
	static char     com_token[MAX_TOKEN_CHARS];

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	// make sure incoming data is valid
	if(!data)
	{
		*data_p = NULL;
		return com_token;
	}

	while(1)
	{
		// skip whitespace
		while((c = *data) <= ' ')
		{
			if(!c)
			{
				break;
			}
			data++;
		}


		c = *data;

		// skip double slash comments
		if(c == '/' && data[1] == '/')
		{
			while(*data && *data != '\n')
				data++;
		}
		// skip /* */ comments
		else if(c == '/' && data[1] == '*')
		{
			while(*data && (*data != '*' || data[1] != '/'))
			{
				data++;
			}
			if(*data)
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	if(c == 0)
	{
		return "";
	}

	// handle quoted strings
	if(c == '\"')
	{
		data++;
		while(1)
		{
			c = *data++;
			if(c == '\"' || !c)
			{
				com_token[len] = 0;
				*data_p = (char *)data;
				return com_token;
			}
			if(len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if(len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while(c > 32 && c != ',');

	if(len == MAX_TOKEN_CHARS)
	{
//      Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = (char *)data;
	return com_token;
}



/*
===============
RE_RegisterSkin

===============
*/
qhandle_t RE_RegisterSkin(const char *name)
{
	qhandle_t       hSkin;
	skin_t         *skin;
	skinSurface_t  *surf;
	char           *text, *text_p;
	char           *token;
	char            surfName[MAX_QPATH];

	if(!name || !name[0])
	{
		Com_Printf("Empty name passed to RE_RegisterSkin\n");
		return 0;
	}

	if(strlen(name) >= MAX_QPATH)
	{
		Com_Printf("Skin name exceeds MAX_QPATH\n");
		return 0;
	}


	// see if the skin is already loaded
	for(hSkin = 1; hSkin < tr.numSkins; hSkin++)
	{
		skin = tr.skins[hSkin];
		if(!Q_stricmp(skin->name, name))
		{
			if(skin->numSurfaces == 0)
			{
				return 0;		// default skin
			}
			return hSkin;
		}
	}

	// allocate a new skin
	if(tr.numSkins == MAX_SKINS)
	{
		ri.Printf(PRINT_WARNING, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name);
		return 0;
	}
	tr.numSkins++;
	skin = ri.Hunk_Alloc(sizeof(skin_t), h_low);
	tr.skins[hSkin] = skin;
	Q_strncpyz(skin->name, name, sizeof(skin->name));
	skin->numSurfaces = 0;

	// make sure the render thread is stopped
	R_SyncRenderThread();

	// If not a .skin file, load as a single shader
	if(strcmp(name + strlen(name) - 5, ".skin"))
	{
		skin->numSurfaces = 1;
		skin->surfaces[0] = ri.Hunk_Alloc(sizeof(skin->surfaces[0]), h_low);
		skin->surfaces[0]->shader = R_FindShader(name, SHADER_3D_DYNAMIC, qtrue);
		return hSkin;
	}

	// load and parse the skin file
	ri.FS_ReadFile(name, (void **)&text);
	if(!text)
	{
		return 0;
	}

	text_p = text;
	while(text_p && *text_p)
	{
		// get surface name
		token = CommaParse(&text_p);
		Q_strncpyz(surfName, token, sizeof(surfName));

		if(!token[0])
		{
			break;
		}
		// lowercase the surface name so skin compares are faster
		Q_strlwr(surfName);

		if(*text_p == ',')
		{
			text_p++;
		}

		if(strstr(token, "tag_"))
		{
			continue;
		}

		// parse the shader name
		token = CommaParse(&text_p);

		surf = skin->surfaces[skin->numSurfaces] = ri.Hunk_Alloc(sizeof(*skin->surfaces[0]), h_low);
		Q_strncpyz(surf->name, surfName, sizeof(surf->name));
		surf->shader = R_FindShader(token, SHADER_3D_DYNAMIC, qtrue);
		skin->numSurfaces++;
	}

	ri.FS_FreeFile(text);


	// never let a skin have 0 shaders
	if(skin->numSurfaces == 0)
	{
		return 0;				// use default skin
	}

	return hSkin;
}



/*
===============
R_InitSkins
===============
*/
void R_InitSkins(void)
{
	skin_t         *skin;

	tr.numSkins = 1;

	// make the default skin have all default shaders
	skin = tr.skins[0] = ri.Hunk_Alloc(sizeof(skin_t), h_low);
	Q_strncpyz(skin->name, "<default skin>", sizeof(skin->name));
	skin->numSurfaces = 1;
	skin->surfaces[0] = ri.Hunk_Alloc(sizeof(*skin->surfaces), h_low);
	skin->surfaces[0]->shader = tr.defaultShader;
}

/*
===============
R_GetSkinByHandle
===============
*/
skin_t         *R_GetSkinByHandle(qhandle_t hSkin)
{
	if(hSkin < 1 || hSkin >= tr.numSkins)
	{
		return tr.skins[0];
	}
	return tr.skins[hSkin];
}

/*
===============
R_SkinList_f
===============
*/
void R_SkinList_f(void)
{
	int             i, j;
	skin_t         *skin;

	ri.Printf(PRINT_ALL, "------------------\n");

	for(i = 0; i < tr.numSkins; i++)
	{
		skin = tr.skins[i];

		ri.Printf(PRINT_ALL, "%3i:%s\n", i, skin->name);
		for(j = 0; j < skin->numSurfaces; j++)
		{
			ri.Printf(PRINT_ALL, "       %s = %s\n", skin->surfaces[j]->name, skin->surfaces[j]->shader->name);
		}
	}
	ri.Printf(PRINT_ALL, "------------------\n");
}
