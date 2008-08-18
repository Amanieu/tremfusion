/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>

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

/*
 * Include file for users of JPEG library.
 * You will need to have included system headers that define at least
 * the typedefs FILE and size_t before you can include jpeglib.h.
 * (stdio.h is sufficient on ANSI-conforming systems.)
 * You may also wish to include "jerror.h".
 */

#define JPEG_INTERNALS
#include "../jpeg-6/jpeglib.h"
#include "../png/png.h"

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

	if(i == 6)
	{
		ri.Printf(PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// bound texture anisotropy
	if(glConfig.textureAnisotropyAvailable)
	{
		if(r_ext_texture_filter_anisotropic->value > glConfig.maxTextureAnisotropy)
		{
			ri.Cvar_Set("r_ext_texture_filter_anisotropic", va("%f", glConfig.maxTextureAnisotropy));
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
			if(glConfig.textureAnisotropyAvailable)
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
	int             dataSize;
	const char     *yesno[] = {
		"no ", "yes"
	};

	ri.Printf(PRINT_ALL, "\n      -w-- -h-- -mm- -type- -if-- wrap --name-------\n");

	texels = 0;
	dataSize = 0;

	for(i = 0; i < tr.numImages; i++)
	{
		image = tr.images[i];

		ri.Printf(PRINT_ALL, "%4i: %4i %4i  %s   ",
				  i, image->uploadWidth, image->uploadHeight, yesno[image->filterType == FT_DEFAULT]);

		switch (image->type)
		{
			case GL_TEXTURE_2D:
				texels += image->uploadWidth * image->uploadHeight;
				dataSize += image->uploadWidth * image->uploadHeight * 4;

				ri.Printf(PRINT_ALL, "2D   ");
				break;

			case GL_TEXTURE_CUBE_MAP_ARB:
				texels += image->uploadWidth * image->uploadHeight * 6;
				dataSize += image->uploadWidth * image->uploadHeight * 6 * 4;

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

			case GL_COMPRESSED_RGBA_ARB:
				ri.Printf(PRINT_ALL, "ARB ");
				break;

			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
				ri.Printf(PRINT_ALL, "S3TC ");
				break;

			case GL_RGBA4:
				ri.Printf(PRINT_ALL, "RGBA4");
				break;

			case GL_RGB5:
				ri.Printf(PRINT_ALL, "RGB5 ");
				break;

			case GL_DEPTH_COMPONENT16_ARB:
			case GL_DEPTH_COMPONENT24_ARB:
			case GL_DEPTH_COMPONENT32_ARB:
				ri.Printf(PRINT_ALL, "D    ");
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
	ri.Printf(PRINT_ALL, " %d.%02d MB total image memory\n", dataSize / (1024 * 1024),
			  (dataSize % (1024 * 1024)) * 100 / (1024 * 1024));
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
// *INDENT-OFF*
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
// *INDENT-ON*

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

	if(glConfig.textureNPOTAvailable)
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
	if(image->type == GL_TEXTURE_CUBE_MAP_ARB)
	{
		while(scaledWidth > glConfig.maxCubeMapTextureSize || scaledHeight > glConfig.maxCubeMapTextureSize)
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


	if(image->bits & (IF_DEPTH16 | IF_DEPTH24 | IF_DEPTH32))
	{
		format = GL_DEPTH_COMPONENT;

		if(image->bits & IF_DEPTH16)
		{
			internalFormat = GL_DEPTH_COMPONENT16_ARB;
		}
		else if(image->bits & IF_DEPTH24)
		{
			internalFormat = GL_DEPTH_COMPONENT24_ARB;
		}
		else if(image->bits & IF_DEPTH32)
		{
			internalFormat = GL_DEPTH_COMPONENT32_ARB;
		}
	}
	else if(glConfig.textureFloatAvailable && (image->bits & (IF_RGBA16F | IF_RGBA32F | IF_RGBA16 | IF_LA16F | IF_LA32F | IF_ALPHA16F | IF_ALPHA32F)))
	{
		if(image->bits & IF_RGBA16F)
		{
			internalFormat = GL_RGBA16F_ARB;
		}
		else if(image->bits & IF_RGBA32F)
		{
			internalFormat = GL_RGBA32F_ARB;
		}
		else if(image->bits & IF_LA16F)
		{
			internalFormat = GL_LUMINANCE_ALPHA16F_ARB;
		}
		else if(image->bits & IF_LA32F)
		{
			internalFormat = GL_LUMINANCE_ALPHA32F_ARB;
		}
		else if(image->bits & IF_RGBA16)
		{
			internalFormat = GL_RGBA16;
		}
		else if(image->bits & IF_ALPHA16F)
		{
			internalFormat = GL_ALPHA16F_ARB;
		}
		else if(image->bits & IF_ALPHA32F)
		{
			internalFormat = GL_ALPHA32F_ARB;
		}
	}
	else if(!(image->bits & IF_LIGHTMAP))
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
			if(glConfig.textureCompression == TC_ARB)
			{
				internalFormat = GL_COMPRESSED_RGBA_ARB;
			}
			else if(glConfig.textureCompression == TC_S3TC)
			{
				internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
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

		if(!(image->bits & (IF_NORMALMAP | IF_RGBA16F | IF_RGBA32F | IF_LA16F | IF_LA32F)))
		{
			R_LightScaleTexture((unsigned *)scaledBuffer, scaledWidth, scaledHeight, image->filterType == FT_DEFAULT);
		}

		image->uploadWidth = scaledWidth;
		image->uploadHeight = scaledHeight;
		image->internalFormat = internalFormat;

		if(image->filterType == FT_DEFAULT && glConfig.generateMipmapAvailable)
		{
			// raynorpat: if hardware mipmap generation is available, use it
			qglHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);	// make sure its nice
			qglTexParameteri(image->type, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
			qglTexParameteri(image->type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);	// default to trilinear
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

		if(!glConfig.generateMipmapAvailable)
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

	GL_CheckErrors();

	// set filter type
	switch (image->filterType)
	{
		case FT_DEFAULT:
			// set texture anisotropy
			if(glConfig.textureAnisotropyAvailable)
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

	GL_CheckErrors();

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

/*
=========================================================

TARGA LOADING

=========================================================
*/

/*
=============
LoadTGA
=============
*/
static void LoadTGA(const char *name, byte ** pic, int *width, int *height, byte alphaByte)
{
	int             columns, rows, numPixels;
	byte           *pixbuf;
	int             row, column;
	byte           *buf_p;
	byte           *buffer;
	TargaHeader     targa_header;
	byte           *targa_rgba;

	*pic = NULL;

	//
	// load the file
	//
	ri.FS_ReadFile((char *)name, (void **)&buffer);
	if(!buffer)
	{
		return;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.colormap_length = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.y_origin = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.width = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.height = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if(targa_header.image_type != 2 && targa_header.image_type != 10 && targa_header.image_type != 3)
	{
		ri.Error(ERR_DROP, "LoadTGA: Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported (%s)\n", name);
	}

	if(targa_header.colormap_type != 0)
	{
		ri.Error(ERR_DROP, "LoadTGA: colormaps not supported (%s)\n", name);
	}

	if((targa_header.pixel_size != 32 && targa_header.pixel_size != 24) && targa_header.image_type != 3)
	{
		ri.Error(ERR_DROP, "LoadTGA: Only 32 or 24 bit images supported (no colormaps) (%s)\n", name);
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows * 4;

	if(width)
		*width = columns;
	if(height)
		*height = rows;

	if(!columns || !rows || numPixels > 0x7FFFFFFF || numPixels / columns / 4 != rows)
	{
		ri.Error(ERR_DROP, "LoadTGA: %s has an invalid image size\n", name);
	}

	targa_rgba = ri.Malloc(numPixels);

	*pic = targa_rgba;

	if(targa_header.id_length != 0)
		buf_p += targa_header.id_length;	// skip TARGA image comment

	if(targa_header.image_type == 2 || targa_header.image_type == 3)
	{
		// Uncompressed RGB or gray scale image
		for(row = rows - 1; row >= 0; row--)
		{
			pixbuf = targa_rgba + row * columns * 4;
			for(column = 0; column < columns; column++)
			{
				unsigned char   red, green, blue, alpha;

				switch (targa_header.pixel_size)
				{

					case 8:
						blue = *buf_p++;
						green = blue;
						red = blue;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphaByte;
						break;

					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphaByte;
						break;
					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alpha = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alpha;
						break;
					default:
						ri.Error(ERR_DROP, "LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name);
						break;
				}
			}
		}
	}
	else if(targa_header.image_type == 10)
	{							// Runlength encoded RGB images
		unsigned char   red, green, blue, alpha, packetHeader, packetSize, j;

		red = 0;
		green = 0;
		blue = 0;
		alpha = alphaByte;

		for(row = rows - 1; row >= 0; row--)
		{
			pixbuf = targa_rgba + row * columns * 4;
			for(column = 0; column < columns;)
			{
				packetHeader = *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if(packetHeader & 0x80)
				{				// run-length packet
					switch (targa_header.pixel_size)
					{
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alpha = alphaByte;
							break;
						case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alpha = *buf_p++;
							break;
						default:
							ri.Error(ERR_DROP, "LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name);
							break;
					}

					for(j = 0; j < packetSize; j++)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alpha;
						column++;
						if(column == columns)
						{		// run spans across rows
							column = 0;
							if(row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
				else
				{				// non run-length packet
					for(j = 0; j < packetSize; j++)
					{
						switch (targa_header.pixel_size)
						{
							case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alphaByte;
								break;
							case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alpha = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alpha;
								break;
							default:
								ri.Error(ERR_DROP,
										 "LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name);
								break;
						}
						column++;
						if(column == columns)
						{		// pixel packet run spans across rows
							column = 0;
							if(row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
			}
		  breakOut:;
		}
	}

#if 1
	// TTimo: this is the chunk of code to ensure a behavior that meets TGA specs 
	// bk0101024 - fix from Leonardo
	// bit 5 set => top-down
	if(targa_header.attributes & 0x20)
	{
		unsigned char  *flip;
		unsigned char  *src, *dst;

		//ri.Printf(PRINT_WARNING, "WARNING: '%s' TGA file header declares top-down image, flipping\n", name);

		flip = (unsigned char *)malloc(columns * 4);
		for(row = 0; row < rows / 2; row++)
		{
			src = targa_rgba + row * 4 * columns;
			dst = targa_rgba + (rows - row - 1) * 4 * columns;

			memcpy(flip, src, columns * 4);
			memcpy(src, dst, columns * 4);
			memcpy(dst, flip, columns * 4);
		}
		free(flip);
	}
#else
	// instead we just print a warning
	if(targa_header.attributes & 0x20)
	{
		ri.Printf(PRINT_WARNING, "WARNING: '%s' TGA file header declares top-down image, ignoring\n", name);
	}
#endif

	ri.FS_FreeFile(buffer);
}


/*
=========================================================

JPEG LOADING

=========================================================
*/

static void LoadJPG(const char *filename, unsigned char **pic, int *width, int *height, byte alphaByte)
{
	/* This struct contains the JPEG decompression parameters and pointers to
	 * working space (which is allocated as needed by the JPEG library).
	 */
	struct jpeg_decompress_struct cinfo = { NULL };

	/* We use our private extension JPEG error handler.
	 * Note that this struct must live as long as the main JPEG parameter
	 * struct, to avoid dangling-pointer problems.
	 */
	/* This struct represents a JPEG error handler.  It is declared separately
	 * because applications often want to supply a specialized error handler
	 * (see the second half of this file for an example).  But here we just
	 * take the easy way out and use the standard error handler, which will
	 * print a message on stderr and call exit() if compression fails.
	 * Note that this struct must live as long as the main JPEG parameter
	 * struct, to avoid dangling-pointer problems.
	 */
	struct jpeg_error_mgr jerr;

	/* More stuff */
	JSAMPARRAY      buffer;		/* Output row buffer */
	unsigned        row_stride;	/* physical row width in output buffer */
	unsigned        pixelcount;
	unsigned char  *out, *out_converted;
	byte           *fbuffer;
	byte           *bbuf;

	/* In this example we want to open the input file before doing anything else,
	 * so that the setjmp() error recovery below can assume the file is open.
	 * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
	 * requires it in order to read binary files.
	 */

	ri.FS_ReadFile((char *)filename, (void **)&fbuffer);
	if(!fbuffer)
	{
		return;
	}

	/* Step 1: allocate and initialize JPEG decompression object */

	/* We have to set up the error handler first, in case the initialization
	 * step fails.  (Unlikely, but it could happen if you are out of memory.)
	 * This routine fills in the contents of struct jerr, and returns jerr's
	 * address which we place into the link field in cinfo.
	 */
	cinfo.err = jpeg_std_error(&jerr);

	/* Now we can initialize the JPEG decompression object. */
	jpeg_create_decompress(&cinfo);

	/* Step 2: specify data source (eg, a file) */

	jpeg_stdio_src(&cinfo, fbuffer);

	/* Step 3: read file parameters with jpeg_read_header() */

	(void)jpeg_read_header(&cinfo, TRUE);
	/* We can ignore the return value from jpeg_read_header since
	 *   (a) suspension is not possible with the stdio data source, and
	 *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
	 * See libjpeg.doc for more info.
	 */

	/* Step 4: set parameters for decompression */

	/* In this example, we don't need to change any of the defaults set by
	 * jpeg_read_header(), so we do nothing here.
	 */

	/* Step 5: Start decompressor */

	(void)jpeg_start_decompress(&cinfo);
	/* We can ignore the return value since suspension is not possible
	 * with the stdio data source.
	 */

	/* We may need to do some setup of our own at this point before reading
	 * the data.  After jpeg_start_decompress() we have the correct scaled
	 * output image dimensions available, as well as the output colormap
	 * if we asked for color quantization.
	 * In this example, we need to make an output work buffer of the right size.
	 */
	/* JSAMPLEs per row in output buffer */
	pixelcount = cinfo.output_width * cinfo.output_height;
	row_stride = cinfo.output_width * cinfo.output_components;
	out = ri.Malloc(pixelcount * 4);

	if(!cinfo.output_width || !cinfo.output_height || ((pixelcount * 4) / cinfo.output_width) / 4 != cinfo.output_height || pixelcount > 0x1FFFFFFF || cinfo.output_components > 4)	// 4*1FFFFFFF == 0x7FFFFFFC < 0x7FFFFFFF
	{
		ri.Error(ERR_DROP, "LoadJPG: %s has an invalid image size: %dx%d*4=%d, components: %d\n", filename,
				 cinfo.output_width, cinfo.output_height, pixelcount * 4, cinfo.output_components);
	}

	*width = cinfo.output_width;
	*height = cinfo.output_height;

	/* Step 6: while (scan lines remain to be read) */
	/*           jpeg_read_scanlines(...); */

	/* Here we use the library's state variable cinfo.output_scanline as the
	 * loop counter, so that we don't have to keep track ourselves.
	 */
	while(cinfo.output_scanline < cinfo.output_height)
	{
		/* jpeg_read_scanlines expects an array of pointers to scanlines.
		 * Here the array is only one element long, but you could ask for
		 * more than one scanline at a time if that's more convenient.
		 */
		bbuf = ((out + (row_stride * cinfo.output_scanline)));
		buffer = &bbuf;
		(void)jpeg_read_scanlines(&cinfo, buffer, 1);
	}

	// If we are processing an 8-bit JPEG (greyscale), we'll have to convert
	// the greyscale values to RGBA.
	if(cinfo.output_components == 1)
	{
		int             sindex, dindex = 0;
		unsigned char   greyshade;

		// allocate a new buffer for the transformed image
		out_converted = ri.Malloc(pixelcount * 4);

		for(sindex = 0; sindex < pixelcount; sindex++)
		{
			greyshade = out[sindex];
			out_converted[dindex++] = greyshade;
			out_converted[dindex++] = greyshade;
			out_converted[dindex++] = greyshade;
			out_converted[dindex++] = alphaByte;
		}

		ri.Free(out);
		out = out_converted;
	}
	else
	{
		// clear all the alphas to 255
		int             i, j;
		byte           *buf;

		buf = out;

		j = cinfo.output_width * cinfo.output_height * 4;
		for(i = 3; i < j; i += 4)
		{
			buf[i] = alphaByte;
		}
	}

	*pic = out;

	/* Step 7: Finish decompression */

	(void)jpeg_finish_decompress(&cinfo);
	/* We can ignore the return value since suspension is not possible
	 * with the stdio data source.
	 */

	/* Step 8: Release JPEG decompression object */

	/* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_decompress(&cinfo);

	/* After finish_decompress, we can close the input file.
	 * Here we postpone it until after no more JPEG errors are possible,
	 * so as to simplify the setjmp error logic above.  (Actually, I don't
	 * think that jpeg_destroy can do an error exit, but why assume anything...)
	 */
	ri.FS_FreeFile(fbuffer);

	/* At this point you may want to check to see whether any corrupt-data
	 * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
	 */

	/* And we're done! */
}


/*
=========================================================

JPEG SAVING

=========================================================
*/


/* Expanded data destination object for stdio output */

typedef struct
{
	struct jpeg_destination_mgr pub;	/* public fields */

	byte           *outfile;	/* target stream */
	int             size;
} my_destination_mgr;

typedef my_destination_mgr *my_dest_ptr;


/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

void init_destination(j_compress_ptr cinfo)
{
	my_dest_ptr     dest = (my_dest_ptr) cinfo->dest;

	dest->pub.next_output_byte = dest->outfile;
	dest->pub.free_in_buffer = dest->size;
}


/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

boolean empty_output_buffer(j_compress_ptr cinfo)
{
	return TRUE;
}


/*
 * Compression initialization.
 * Before calling this, all parameters and a data destination must be set up.
 *
 * We require a write_all_tables parameter as a failsafe check when writing
 * multiple datastreams from the same compression object.  Since prior runs
 * will have left all the tables marked sent_table=TRUE, a subsequent run
 * would emit an abbreviated stream (no tables) by default.  This may be what
 * is wanted, but for safety's sake it should not be the default behavior:
 * programmers should have to make a deliberate choice to emit abbreviated
 * images.  Therefore the documentation and examples should encourage people
 * to pass write_all_tables=TRUE; then it will take active thought to do the
 * wrong thing.
 */

GLOBAL void jpeg_start_compress(j_compress_ptr cinfo, boolean write_all_tables)
{
	if(cinfo->global_state != CSTATE_START)
		ERREXIT1(cinfo, JERR_BAD_STATE, cinfo->global_state);

	if(write_all_tables)
		jpeg_suppress_tables(cinfo, FALSE);	/* mark all tables to be written */

	/* (Re)initialize error mgr and destination modules */
	(*cinfo->err->reset_error_mgr) ((j_common_ptr) cinfo);
	(*cinfo->dest->init_destination) (cinfo);
	/* Perform master selection of active modules */
	jinit_compress_master(cinfo);
	/* Set up for the first pass */
	(*cinfo->master->prepare_for_pass) (cinfo);
	/* Ready for application to drive first pass through jpeg_write_scanlines
	 * or jpeg_write_raw_data.
	 */
	cinfo->next_scanline = 0;
	cinfo->global_state = (cinfo->raw_data_in ? CSTATE_RAW_OK : CSTATE_SCANNING);
}


/*
 * Write some scanlines of data to the JPEG compressor.
 *
 * The return value will be the number of lines actually written.
 * This should be less than the supplied num_lines only in case that
 * the data destination module has requested suspension of the compressor,
 * or if more than image_height scanlines are passed in.
 *
 * Note: we warn about excess calls to jpeg_write_scanlines() since
 * this likely signals an application programmer error.  However,
 * excess scanlines passed in the last valid call are *silently* ignored,
 * so that the application need not adjust num_lines for end-of-image
 * when using a multiple-scanline buffer.
 */

GLOBAL JDIMENSION jpeg_write_scanlines(j_compress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION num_lines)
{
	JDIMENSION      row_ctr, rows_left;

	if(cinfo->global_state != CSTATE_SCANNING)
		ERREXIT1(cinfo, JERR_BAD_STATE, cinfo->global_state);
	if(cinfo->next_scanline >= cinfo->image_height)
		WARNMS(cinfo, JWRN_TOO_MUCH_DATA);

	/* Call progress monitor hook if present */
	if(cinfo->progress != NULL)
	{
		cinfo->progress->pass_counter = (long)cinfo->next_scanline;
		cinfo->progress->pass_limit = (long)cinfo->image_height;
		(*cinfo->progress->progress_monitor) ((j_common_ptr) cinfo);
	}

	/* Give master control module another chance if this is first call to
	 * jpeg_write_scanlines.  This lets output of the frame/scan headers be
	 * delayed so that application can write COM, etc, markers between
	 * jpeg_start_compress and jpeg_write_scanlines.
	 */
	if(cinfo->master->call_pass_startup)
		(*cinfo->master->pass_startup) (cinfo);

	/* Ignore any extra scanlines at bottom of image. */
	rows_left = cinfo->image_height - cinfo->next_scanline;
	if(num_lines > rows_left)
		num_lines = rows_left;

	row_ctr = 0;
	(*cinfo->main->process_data) (cinfo, scanlines, &row_ctr, num_lines);
	cinfo->next_scanline += row_ctr;
	return row_ctr;
}

/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

static int      hackSize;

void term_destination(j_compress_ptr cinfo)
{
	my_dest_ptr     dest = (my_dest_ptr) cinfo->dest;
	size_t          datacount = dest->size - dest->pub.free_in_buffer;

	hackSize = datacount;
}


/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

void jpegDest(j_compress_ptr cinfo, byte * outfile, int size)
{
	my_dest_ptr     dest;

	/* The destination object is made permanent so that multiple JPEG images
	 * can be written to the same file without re-executing jpeg_stdio_dest.
	 * This makes it dangerous to use this manager and a different destination
	 * manager serially with the same JPEG object, because their private object
	 * sizes may be different.  Caveat programmer.
	 */
	if(cinfo->dest == NULL)
	{							/* first time for this JPEG object? */
		cinfo->dest = (struct jpeg_destination_mgr *)
			(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(my_destination_mgr));
	}

	dest = (my_dest_ptr) cinfo->dest;
	dest->pub.init_destination = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination = term_destination;
	dest->outfile = outfile;
	dest->size = size;
}

void SaveJPG(char *filename, int quality, int image_width, int image_height, unsigned char *image_buffer)
{
	/* This struct contains the JPEG compression parameters and pointers to
	 * working space (which is allocated as needed by the JPEG library).
	 * It is possible to have several such structures, representing multiple
	 * compression/decompression processes, in existence at once.  We refer
	 * to any one struct (and its associated working data) as a "JPEG object".
	 */
	struct jpeg_compress_struct cinfo;

	/* This struct represents a JPEG error handler.  It is declared separately
	 * because applications often want to supply a specialized error handler
	 * (see the second half of this file for an example).  But here we just
	 * take the easy way out and use the standard error handler, which will
	 * print a message on stderr and call exit() if compression fails.
	 * Note that this struct must live as long as the main JPEG parameter
	 * struct, to avoid dangling-pointer problems.
	 */
	struct jpeg_error_mgr jerr;

	/* More stuff */
	JSAMPROW        row_pointer[1];	/* pointer to JSAMPLE row[s] */
	int             row_stride;	/* physical row width in image buffer */
	unsigned char  *out;

	/* Step 1: allocate and initialize JPEG compression object */

	/* We have to set up the error handler first, in case the initialization
	 * step fails.  (Unlikely, but it could happen if you are out of memory.)
	 * This routine fills in the contents of struct jerr, and returns jerr's
	 * address which we place into the link field in cinfo.
	 */
	cinfo.err = jpeg_std_error(&jerr);
	/* Now we can initialize the JPEG compression object. */
	jpeg_create_compress(&cinfo);

	/* Step 2: specify data destination (eg, a file) */
	/* Note: steps 2 and 3 can be done in either order. */

	/* Here we use the library-supplied code to send compressed data to a
	 * stdio stream.  You can also write your own code to do something else.
	 * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
	 * requires it in order to write binary files.
	 */
	out = ri.Hunk_AllocateTempMemory(image_width * image_height * 4);
	jpegDest(&cinfo, out, image_width * image_height * 4);

	/* Step 3: set parameters for compression */

	/* First we supply a description of the input image.
	 * Four fields of the cinfo struct must be filled in:
	 */
	cinfo.image_width = image_width;	/* image width and height, in pixels */
	cinfo.image_height = image_height;
	cinfo.input_components = 4;	/* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB;	/* colorspace of input image */
	/* Now use the library's routine to set default compression parameters.
	 * (You must set at least cinfo.in_color_space before calling this,
	 * since the defaults depend on the source color space.)
	 */
	jpeg_set_defaults(&cinfo);
	/* Now you can set any non-default parameters you wish to.
	 * Here we just illustrate the use of quality (quantization table) scaling:
	 */
	jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */ );
	/* If quality is set high, disable chroma subsampling */
	if(quality >= 85)
	{
		cinfo.comp_info[0].h_samp_factor = 1;
		cinfo.comp_info[0].v_samp_factor = 1;
	}

	/* Step 4: Start compressor */

	/* TRUE ensures that we will write a complete interchange-JPEG file.
	 * Pass TRUE unless you are very sure of what you're doing.
	 */
	jpeg_start_compress(&cinfo, TRUE);

	/* Step 5: while (scan lines remain to be written) */
	/*           jpeg_write_scanlines(...); */

	/* Here we use the library's state variable cinfo.next_scanline as the
	 * loop counter, so that we don't have to keep track ourselves.
	 * To keep things simple, we pass one scanline per call; you can pass
	 * more if you wish, though.
	 */
	row_stride = image_width * 4;	/* JSAMPLEs per row in image_buffer */

	while(cinfo.next_scanline < cinfo.image_height)
	{
		/* jpeg_write_scanlines expects an array of pointers to scanlines.
		 * Here the array is only one element long, but you could pass
		 * more than one scanline at a time if that's more convenient.
		 */
		row_pointer[0] = &image_buffer[((cinfo.image_height - 1) * row_stride) - cinfo.next_scanline * row_stride];
		(void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	/* Step 6: Finish compression */

	jpeg_finish_compress(&cinfo);
	/* After finish_compress, we can close the output file. */
	ri.FS_WriteFile(filename, out, hackSize);

	ri.Hunk_FreeTempMemory(out);

	/* Step 7: release JPEG compression object */

	/* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_compress(&cinfo);

	/* And we're done! */
}

/*
=================
SaveJPGToBuffer
=================
*/
int SaveJPGToBuffer(byte * buffer, int quality, int image_width, int image_height, byte * image_buffer)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW        row_pointer[1];	/* pointer to JSAMPLE row[s] */
	int             row_stride;	/* physical row width in image buffer */

	/* Step 1: allocate and initialize JPEG compression object */
	cinfo.err = jpeg_std_error(&jerr);
	/* Now we can initialize the JPEG compression object. */
	jpeg_create_compress(&cinfo);

	/* Step 2: specify data destination (eg, a file) */
	/* Note: steps 2 and 3 can be done in either order. */
	jpegDest(&cinfo, buffer, image_width * image_height * 4);

	/* Step 3: set parameters for compression */
	cinfo.image_width = image_width;	/* image width and height, in pixels */
	cinfo.image_height = image_height;
	cinfo.input_components = 4;	/* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB;	/* colorspace of input image */

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */ );
	/* If quality is set high, disable chroma subsampling */
	if(quality >= 85)
	{
		cinfo.comp_info[0].h_samp_factor = 1;
		cinfo.comp_info[0].v_samp_factor = 1;
	}

	/* Step 4: Start compressor */
	jpeg_start_compress(&cinfo, TRUE);

	/* Step 5: while (scan lines remain to be written) */
	/*           jpeg_write_scanlines(...); */
	row_stride = image_width * 4;	/* JSAMPLEs per row in image_buffer */

	while(cinfo.next_scanline < cinfo.image_height)
	{
		/* jpeg_write_scanlines expects an array of pointers to scanlines.
		 * Here the array is only one element long, but you could pass
		 * more than one scanline at a time if that's more convenient.
		 */
		row_pointer[0] = &image_buffer[((cinfo.image_height - 1) * row_stride) - cinfo.next_scanline * row_stride];
		(void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	/* Step 6: Finish compression */
	jpeg_finish_compress(&cinfo);

	/* Step 7: release JPEG compression object */
	jpeg_destroy_compress(&cinfo);

	/* And we're done! */
	return hackSize;
}

/*
=========================================================

PNG LOADING

=========================================================
*/
static void png_read_data(png_structp png, png_bytep data, png_size_t length)
{
	Com_Memcpy(data, png->io_ptr, length);

	// raynorpat: msvc is gay
#if _MSC_VER
	(byte *) png->io_ptr += length;
#else
	png->io_ptr += length;
#endif
}

static void png_user_warning_fn(png_structp png_ptr, png_const_charp warning_message)
{
	ri.Printf(PRINT_WARNING, "libpng warning: %s\n", warning_message);
}

static void png_user_error_fn(png_structp png_ptr, png_const_charp error_message)
{
	ri.Printf(PRINT_ERROR, "libpng error: %s\n", error_message);
	longjmp(png_ptr->jmpbuf, 0);
}

void LoadPNG(const char *name, byte ** pic, int *width, int *height, byte alphaByte)
{
	int             bit_depth;
	int             color_type;
	png_uint_32     w;
	png_uint_32     h;
	unsigned int    row;
	size_t          rowbytes;
	png_infop       info;
	png_structp     png;
	png_bytep      *row_pointers;
	byte           *data;
	byte           *out;
	int             size;

	// load png
	size = ri.FS_ReadFile(name, (void **)&data);

	if(!data)
		return;

	//png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp) NULL, png_user_error_fn, png_user_warning_fn);

	if(!png)
	{
		ri.Printf(PRINT_WARNING, "LoadPNG: png_create_write_struct() failed for (%s)\n", name);
		ri.FS_FreeFile(data);
		return;
	}

	// allocate/initialize the memory for image information.  REQUIRED
	info = png_create_info_struct(png);
	if(!info)
	{
		ri.Printf(PRINT_WARNING, "LoadPNG: png_create_info_struct() failed for (%s)\n", name);
		ri.FS_FreeFile(data);
		png_destroy_read_struct(&png, (png_infopp) NULL, (png_infopp) NULL);
		return;
	}

	/*
	 * Set error handling if you are using the setjmp/longjmp method (this is 
	 * the normal method of doing things with libpng).  REQUIRED unless you
	 * set up your own error handlers in the png_create_read_struct() earlier.
	 */
	if(setjmp(png_jmpbuf(png)))
	{
		// if we get here, we had a problem reading the file
		ri.Printf(PRINT_WARNING, "LoadPNG: first exception handler called for (%s)\n", name);
		ri.FS_FreeFile(data);
		png_destroy_read_struct(&png, (png_infopp) & info, (png_infopp) NULL);
		return;
	}

	//png_set_write_fn(png, buffer, png_write_data, png_flush_data);
	png_set_read_fn(png, data, png_read_data);

	png_set_sig_bytes(png, 0);

	// The call to png_read_info() gives us all of the information from the
	// PNG file before the first IDAT (image data chunk).  REQUIRED
	png_read_info(png, info);

	// get picture info
	png_get_IHDR(png, info, (png_uint_32 *) & w, (png_uint_32 *) & h, &bit_depth, &color_type, NULL, NULL, NULL);

	// tell libpng to strip 16 bit/color files down to 8 bits/color
	png_set_strip_16(png);

	// expand paletted images to RGB triplets
	if(color_type & PNG_COLOR_MASK_PALETTE)
		png_set_expand(png);

	// expand gray-scaled images to RGB triplets
	if(!(color_type & PNG_COLOR_MASK_COLOR))
		png_set_gray_to_rgb(png);

	// expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel
	//if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
	//  png_set_gray_1_2_4_to_8(png);

	// expand paletted or RGB images with transparency to full alpha channels
	// so the data will be available as RGBA quartets
	if(png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	// if there is no alpha information, fill with alphaByte
	if(!(color_type & PNG_COLOR_MASK_ALPHA))
		png_set_filler(png, alphaByte, PNG_FILLER_AFTER);

	// expand pictures with less than 8bpp to 8bpp
	if(bit_depth < 8)
		png_set_packing(png);

	// update structure with the above settings
	png_read_update_info(png, info);

	// allocate the memory to hold the image
	*width = w;
	*height = h;
	*pic = out = (byte *) ri.Malloc(w * h * 4);

	row_pointers = (png_bytep *) ri.Hunk_AllocateTempMemory(sizeof(png_bytep) * h);

	// set a new exception handler
	if(setjmp(png_jmpbuf(png)))
	{
		ri.Printf(PRINT_WARNING, "LoadPNG: second exception handler called for (%s)\n", name);
		ri.Hunk_FreeTempMemory(row_pointers);
		ri.FS_FreeFile(data);
		png_destroy_read_struct(&png, (png_infopp) & info, (png_infopp) NULL);
		return;
	}

	rowbytes = png_get_rowbytes(png, info);

	for(row = 0; row < h; row++)
		row_pointers[row] = (png_bytep) (out + (row * 4 * w));

	// read image data
	png_read_image(png, row_pointers);

	// read rest of file, and get additional chunks in info
	png_read_end(png, info);

	// clean up after the read, and free any memory allocated
	png_destroy_read_struct(&png, &info, (png_infopp) NULL);

	ri.Hunk_FreeTempMemory(row_pointers);
	ri.FS_FreeFile(data);
}

/*
=========================================================

PNG SAVING

=========================================================
*/
static int      png_compressed_size;

static void png_write_data(png_structp png, png_bytep data, png_size_t length)
{
	memcpy(png->io_ptr, data, length);

	// raynorpat: msvc is gay
#if _MSC_VER
	(byte *) png->io_ptr += length;
#else
	png->io_ptr += length;
#endif

	png_compressed_size += length;
}

static void png_flush_data(png_structp png)
{
}

void SavePNG(const char *name, const byte * pic, int width, int height)
{
	png_structp     png;
	png_infop       info;
	int             i;
	int             row_stride;
	byte           *buffer;
	const byte     *row;
	png_bytep      *row_pointers;

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if(!png)
		return;

	// Allocate/initialize the image information data
	info = png_create_info_struct(png);
	if(!info)
	{
		png_destroy_write_struct(&png, (png_infopp) NULL);
		return;
	}

	png_compressed_size = 0;
	buffer = ri.Hunk_AllocateTempMemory(width * height * 3);

	// set error handling
	if(setjmp(png_jmpbuf(png)))
	{
		ri.Hunk_FreeTempMemory(buffer);
		png_destroy_write_struct(&png, &info);
		return;
	}

	png_set_write_fn(png, buffer, png_write_data, png_flush_data);
	png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
				 PNG_FILTER_TYPE_DEFAULT);

	// write the file header information
	png_write_info(png, info);

	row_pointers = ri.Hunk_AllocateTempMemory(height * sizeof(png_bytep));

	if(setjmp(png_jmpbuf(png)))
	{
		ri.Hunk_FreeTempMemory(row_pointers);
		ri.Hunk_FreeTempMemory(buffer);
		png_destroy_write_struct(&png, &info);
		return;
	}

	row_stride = width * 3;
	row = pic + (height - 1) * row_stride;
	for(i = 0; i < height; i++)
	{
		row_pointers[i] = row;
		row -= row_stride;
	}

	png_write_image(png, row_pointers);
	png_write_end(png, info);

	// clean up after the write, and free any memory allocated
	png_destroy_write_struct(&png, &info);

	ri.Hunk_FreeTempMemory(row_pointers);

	ri.FS_WriteFile(name, buffer, png_compressed_size);

	ri.Hunk_FreeTempMemory(buffer);
}

/*
=========================================================

DDS LOADING

=========================================================
*/

/* -----------------------------------------------------------------------------

DDS Library 

Based on code from Nvidia's DDS example:
http://www.nvidia.com/object/dxtc_decompression_code.html

Copyright (c) 2003 Randy Reddig
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

Neither the names of the copyright holders nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------------- */

/* dds definition */
typedef enum
{
	DDS_PF_ARGB8888,
	DDS_PF_DXT1,
	DDS_PF_DXT2,
	DDS_PF_DXT3,
	DDS_PF_DXT4,
	DDS_PF_DXT5,
	DDS_PF_UNKNOWN
}
ddsPF_t;


/* 16bpp stuff */
#define DDS_LOW_5		0x001F;
#define DDS_MID_6		0x07E0;
#define DDS_HIGH_5		0xF800;
#define DDS_MID_555		0x03E0;
#define DDS_HI_555		0x7C00;


/* structures */
typedef struct ddsColorKey_s
{
	unsigned int    colorSpaceLowValue;
	unsigned int    colorSpaceHighValue;
}
ddsColorKey_t;


typedef struct ddsCaps_s
{
	unsigned int    caps1;
	unsigned int    caps2;
	unsigned int    caps3;
	unsigned int    caps4;
}
ddsCaps_t;


typedef struct ddsMultiSampleCaps_s
{
	unsigned short  flipMSTypes;
	unsigned short  bltMSTypes;
}
ddsMultiSampleCaps_t;


typedef struct ddsPixelFormat_s
{
	unsigned int    size;
	unsigned int    flags;
	unsigned int    fourCC;
	union
	{
		unsigned int    rgbBitCount;
		unsigned int    yuvBitCount;
		unsigned int    zBufferBitDepth;
		unsigned int    alphaBitDepth;
		unsigned int    luminanceBitCount;
		unsigned int    bumpBitCount;
		unsigned int    privateFormatBitCount;
	};
	union
	{
		unsigned int    rBitMask;
		unsigned int    yBitMask;
		unsigned int    stencilBitDepth;
		unsigned int    luminanceBitMask;
		unsigned int    bumpDuBitMask;
		unsigned int    operations;
	};
	union
	{
		unsigned int    gBitMask;
		unsigned int    uBitMask;
		unsigned int    zBitMask;
		unsigned int    bumpDvBitMask;
		ddsMultiSampleCaps_t multiSampleCaps;
	};
	union
	{
		unsigned int    bBitMask;
		unsigned int    vBitMask;
		unsigned int    stencilBitMask;
		unsigned int    bumpLuminanceBitMask;
	};
	union
	{
		unsigned int    rgbAlphaBitMask;
		unsigned int    yuvAlphaBitMask;
		unsigned int    luminanceAlphaBitMask;
		unsigned int    rgbZBitMask;
		unsigned int    yuvZBitMask;
	};
}
ddsPixelFormat_t;


typedef struct ddsBuffer_s
{
	/* magic: 'dds ' */
	char            magic[4];

	/* directdraw surface */
	unsigned int    size;
	unsigned int    flags;
	unsigned int    height;
	unsigned int    width;
	union
	{
		int             pitch;
		unsigned int    linearSize;
	};
	unsigned int    backBufferCount;
	union
	{
		unsigned int    mipMapCount;
		unsigned int    refreshRate;
		unsigned int    srcVBHandle;
	};
	unsigned int    alphaBitDepth;
	unsigned int    reserved;
	void           *surface;
	union
	{
		ddsColorKey_t   ckDestOverlay;
		unsigned int    emptyFaceColor;
	};
	ddsColorKey_t   ckDestBlt;
	ddsColorKey_t   ckSrcOverlay;
	ddsColorKey_t   ckSrcBlt;
	union
	{
		ddsPixelFormat_t pixelFormat;
		unsigned int    fvf;
	};
	ddsCaps_t       ddsCaps;
	unsigned int    textureStage;

	/* data (Varying size) */
	unsigned char   data[4];
}
ddsBuffer_t;


typedef struct ddsColorBlock_s
{
	unsigned short  colors[2];
	unsigned char   row[4];
}
ddsColorBlock_t;


typedef struct ddsAlphaBlockExplicit_s
{
	unsigned short  row[4];
}
ddsAlphaBlockExplicit_t;


typedef struct ddsAlphaBlock3BitLinear_s
{
	unsigned char   alpha0;
	unsigned char   alpha1;
	unsigned char   stuff[6];
}
ddsAlphaBlock3BitLinear_t;


typedef struct ddsColor_s
{
	unsigned char   r, g, b, a;
}
ddsColor_t;

/*
DDSDecodePixelFormat()
determines which pixel format the dds texture is in
*/

static void DDSDecodePixelFormat(ddsBuffer_t * dds, ddsPF_t * pf)
{
	unsigned int    fourCC;


	/* dummy check */
	if(dds == NULL || pf == NULL)
		return;

	/* extract fourCC */
	fourCC = dds->pixelFormat.fourCC;

	/* test it */
	if(fourCC == 0)
		*pf = DDS_PF_ARGB8888;
	else if(fourCC == *((unsigned int *)"DXT1"))
		*pf = DDS_PF_DXT1;
	else if(fourCC == *((unsigned int *)"DXT2"))
		*pf = DDS_PF_DXT2;
	else if(fourCC == *((unsigned int *)"DXT3"))
		*pf = DDS_PF_DXT3;
	else if(fourCC == *((unsigned int *)"DXT4"))
		*pf = DDS_PF_DXT4;
	else if(fourCC == *((unsigned int *)"DXT5"))
		*pf = DDS_PF_DXT5;
	else
		*pf = DDS_PF_UNKNOWN;
}



/*
DDSGetInfo()
extracts relevant info from a dds texture, returns 0 on success
*/

int DDSGetInfo(ddsBuffer_t * dds, int *width, int *height, ddsPF_t * pf)
{
	/* dummy test */
	if(dds == NULL)
		return -1;

	/* test dds header */
	if(*((int *)dds->magic) != *((int *)"DDS "))
		return -1;
	if(LittleLong(dds->size) != 124)
		return -1;

	/* extract width and height */
	if(width != NULL)
		*width = LittleLong(dds->width);
	if(height != NULL)
		*height = LittleLong(dds->height);

	/* get pixel format */
	DDSDecodePixelFormat(dds, pf);

	/* return ok */
	return 0;
}



/*
DDSGetColorBlockColors()
extracts colors from a dds color block
*/

static void DDSGetColorBlockColors(ddsColorBlock_t * block, ddsColor_t colors[4])
{
	unsigned short  word;


	/* color 0 */
	word = LittleShort(block->colors[0]);
	colors[0].a = 0xff;

	/* extract rgb bits */
	colors[0].b = (unsigned char)word;
	colors[0].b <<= 3;
	colors[0].b |= (colors[0].b >> 5);
	word >>= 5;
	colors[0].g = (unsigned char)word;
	colors[0].g <<= 2;
	colors[0].g |= (colors[0].g >> 5);
	word >>= 6;
	colors[0].r = (unsigned char)word;
	colors[0].r <<= 3;
	colors[0].r |= (colors[0].r >> 5);

	/* same for color 1 */
	word = LittleShort(block->colors[1]);
	colors[1].a = 0xff;

	/* extract rgb bits */
	colors[1].b = (unsigned char)word;
	colors[1].b <<= 3;
	colors[1].b |= (colors[1].b >> 5);
	word >>= 5;
	colors[1].g = (unsigned char)word;
	colors[1].g <<= 2;
	colors[1].g |= (colors[1].g >> 5);
	word >>= 6;
	colors[1].r = (unsigned char)word;
	colors[1].r <<= 3;
	colors[1].r |= (colors[1].r >> 5);

	/* use this for all but the super-freak math method */
	if(block->colors[0] > block->colors[1])
	{
		/* four-color block: derive the other two colors.    
		   00 = color 0, 01 = color 1, 10 = color 2, 11 = color 3
		   these two bit codes correspond to the 2-bit fields 
		   stored in the 64-bit block. */

		word = ((unsigned short)colors[0].r * 2 + (unsigned short)colors[1].r) / 3;
		/* no +1 for rounding */
		/* as bits have been shifted to 888 */
		colors[2].r = (unsigned char)word;
		word = ((unsigned short)colors[0].g * 2 + (unsigned short)colors[1].g) / 3;
		colors[2].g = (unsigned char)word;
		word = ((unsigned short)colors[0].b * 2 + (unsigned short)colors[1].b) / 3;
		colors[2].b = (unsigned char)word;
		colors[2].a = 0xff;

		word = ((unsigned short)colors[0].r + (unsigned short)colors[1].r * 2) / 3;
		colors[3].r = (unsigned char)word;
		word = ((unsigned short)colors[0].g + (unsigned short)colors[1].g * 2) / 3;
		colors[3].g = (unsigned char)word;
		word = ((unsigned short)colors[0].b + (unsigned short)colors[1].b * 2) / 3;
		colors[3].b = (unsigned char)word;
		colors[3].a = 0xff;
	}
	else
	{
		/* three-color block: derive the other color.
		   00 = color 0, 01 = color 1, 10 = color 2,  
		   11 = transparent.
		   These two bit codes correspond to the 2-bit fields 
		   stored in the 64-bit block */

		word = ((unsigned short)colors[0].r + (unsigned short)colors[1].r) / 2;
		colors[2].r = (unsigned char)word;
		word = ((unsigned short)colors[0].g + (unsigned short)colors[1].g) / 2;
		colors[2].g = (unsigned char)word;
		word = ((unsigned short)colors[0].b + (unsigned short)colors[1].b) / 2;
		colors[2].b = (unsigned char)word;
		colors[2].a = 0xff;

		/* random color to indicate alpha */
		colors[3].r = 0x00;
		colors[3].g = 0xff;
		colors[3].b = 0xff;
		colors[3].a = 0x00;
	}
}



/*
DDSDecodeColorBlock()
decodes a dds color block
fixme: make endian-safe
*/

static void DDSDecodeColorBlock(unsigned int *pixel, ddsColorBlock_t * block, int width, unsigned int colors[4])
{
	int             r, n;
	unsigned int    bits;
	unsigned int    masks[] = { 3, 12, 3 << 4, 3 << 6 };	/* bit masks = 00000011, 00001100, 00110000, 11000000 */
	int             shift[] = { 0, 2, 4, 6 };


	/* r steps through lines in y */
	for(r = 0; r < 4; r++, pixel += (width - 4))	/* no width * 4 as unsigned int ptr inc will * 4 */
	{
		/* width * 4 bytes per pixel per line, each j dxtc row is 4 lines of pixels */

		/* n steps through pixels */
		for(n = 0; n < 4; n++)
		{
			bits = block->row[r] & masks[n];
			bits >>= shift[n];

			switch (bits)
			{
				case 0:
					*pixel = colors[0];
					pixel++;
					break;

				case 1:
					*pixel = colors[1];
					pixel++;
					break;

				case 2:
					*pixel = colors[2];
					pixel++;
					break;

				case 3:
					*pixel = colors[3];
					pixel++;
					break;

				default:
					/* invalid */
					pixel++;
					break;
			}
		}
	}
}



/*
DDSDecodeAlphaExplicit()
decodes a dds explicit alpha block
*/

static void DDSDecodeAlphaExplicit(unsigned int *pixel, ddsAlphaBlockExplicit_t * alphaBlock, int width, unsigned int alphaZero)
{
	int             row, pix;
	unsigned short  word;
	ddsColor_t      color;


	/* clear color */
	color.r = 0;
	color.g = 0;
	color.b = 0;

	/* walk rows */
	for(row = 0; row < 4; row++, pixel += (width - 4))
	{
		word = LittleShort(alphaBlock->row[row]);

		/* walk pixels */
		for(pix = 0; pix < 4; pix++)
		{
			/* zero the alpha bits of image pixel */
			*pixel &= alphaZero;
			color.a = word & 0x000F;
			color.a = color.a | (color.a << 4);
			*pixel |= *((unsigned int *)&color);
			word >>= 4;			/* move next bits to lowest 4 */
			pixel++;			/* move to next pixel in the row */

		}
	}
}



/*
DDSDecodeAlpha3BitLinear()
decodes interpolated alpha block
*/

static void DDSDecodeAlpha3BitLinear(unsigned int *pixel, ddsAlphaBlock3BitLinear_t * alphaBlock, int width,
									 unsigned int alphaZero)
{

	int             row, pix;
	unsigned int    stuff;
	unsigned char   bits[4][4];
	unsigned short  alphas[8];
	ddsColor_t      aColors[4][4];


	/* get initial alphas */
	alphas[0] = alphaBlock->alpha0;
	alphas[1] = alphaBlock->alpha1;

	/* 8-alpha block */
	if(alphas[0] > alphas[1])
	{
		/* 000 = alpha_0, 001 = alpha_1, others are interpolated */
		alphas[2] = (6 * alphas[0] + alphas[1]) / 7;	/* bit code 010 */
		alphas[3] = (5 * alphas[0] + 2 * alphas[1]) / 7;	/* bit code 011 */
		alphas[4] = (4 * alphas[0] + 3 * alphas[1]) / 7;	/* bit code 100 */
		alphas[5] = (3 * alphas[0] + 4 * alphas[1]) / 7;	/* bit code 101 */
		alphas[6] = (2 * alphas[0] + 5 * alphas[1]) / 7;	/* bit code 110 */
		alphas[7] = (alphas[0] + 6 * alphas[1]) / 7;	/* bit code 111 */
	}

	/* 6-alpha block */
	else
	{
		/* 000 = alpha_0, 001 = alpha_1, others are interpolated */
		alphas[2] = (4 * alphas[0] + alphas[1]) / 5;	/* bit code 010 */
		alphas[3] = (3 * alphas[0] + 2 * alphas[1]) / 5;	/* bit code 011 */
		alphas[4] = (2 * alphas[0] + 3 * alphas[1]) / 5;	/* bit code 100 */
		alphas[5] = (alphas[0] + 4 * alphas[1]) / 5;	/* bit code 101 */
		alphas[6] = 0;			/* bit code 110 */
		alphas[7] = 255;		/* bit code 111 */
	}

	/* decode 3-bit fields into array of 16 bytes with same value */

	/* first two rows of 4 pixels each */
	stuff = *((unsigned int *)&(alphaBlock->stuff[0]));

	bits[0][0] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[0][1] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[0][2] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[0][3] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[1][0] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[1][1] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[1][2] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[1][3] = (unsigned char)(stuff & 0x00000007);

	/* last two rows */
	stuff = *((unsigned int *)&(alphaBlock->stuff[3]));	/* last 3 bytes */

	bits[2][0] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[2][1] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[2][2] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[2][3] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[3][0] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[3][1] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[3][2] = (unsigned char)(stuff & 0x00000007);
	stuff >>= 3;
	bits[3][3] = (unsigned char)(stuff & 0x00000007);

	/* decode the codes into alpha values */
	for(row = 0; row < 4; row++)
	{
		for(pix = 0; pix < 4; pix++)
		{
			aColors[row][pix].r = 0;
			aColors[row][pix].g = 0;
			aColors[row][pix].b = 0;
			aColors[row][pix].a = (unsigned char)alphas[bits[row][pix]];
		}
	}

	/* write out alpha values to the image bits */
	for(row = 0; row < 4; row++, pixel += width - 4)
	{
		for(pix = 0; pix < 4; pix++)
		{
			/* zero the alpha bits of image pixel */
			*pixel &= alphaZero;

			/* or the bits into the prev. nulled alpha */
			*pixel |= *((unsigned int *)&(aColors[row][pix]));
			pixel++;
		}
	}
}



/*
DDSDecompressDXT1()
decompresses a dxt1 format texture
*/

static int DDSDecompressDXT1(ddsBuffer_t * dds, int width, int height, unsigned char *pixels)
{
	int             x, y, xBlocks, yBlocks;
	unsigned int   *pixel;
	ddsColorBlock_t *block;
	ddsColor_t      colors[4];


	/* setup */
	xBlocks = width / 4;
	yBlocks = height / 4;

	/* walk y */
	for(y = 0; y < yBlocks; y++)
	{
		/* 8 bytes per block */
		block = (ddsColorBlock_t *) ((unsigned int)dds->data + y * xBlocks * 8);

		/* walk x */
		for(x = 0; x < xBlocks; x++, block++)
		{
			DDSGetColorBlockColors(block, colors);
			pixel = (unsigned int *)(pixels + x * 16 + (y * 4) * width * 4);
			DDSDecodeColorBlock(pixel, block, width, (unsigned int *)colors);
		}
	}

	/* return ok */
	return 0;
}



/*
DDSDecompressDXT3()
decompresses a dxt3 format texture
*/

static int DDSDecompressDXT3(ddsBuffer_t * dds, int width, int height, unsigned char *pixels)
{
	int             x, y, xBlocks, yBlocks;
	unsigned int   *pixel, alphaZero;
	ddsColorBlock_t *block;
	ddsAlphaBlockExplicit_t *alphaBlock;
	ddsColor_t      colors[4];


	/* setup */
	xBlocks = width / 4;
	yBlocks = height / 4;

	/* create zero alpha */
	colors[0].a = 0;
	colors[0].r = 0xFF;
	colors[0].g = 0xFF;
	colors[0].b = 0xFF;
	alphaZero = *((unsigned int *)&colors[0]);

	/* walk y */
	for(y = 0; y < yBlocks; y++)
	{
		/* 8 bytes per block, 1 block for alpha, 1 block for color */
		block = (ddsColorBlock_t *) ((unsigned int)dds->data + y * xBlocks * 16);

		/* walk x */
		for(x = 0; x < xBlocks; x++, block++)
		{
			/* get alpha block */
			alphaBlock = (ddsAlphaBlockExplicit_t *) block;

			/* get color block */
			block++;
			DDSGetColorBlockColors(block, colors);

			/* decode color block */
			pixel = (unsigned int *)(pixels + x * 16 + (y * 4) * width * 4);
			DDSDecodeColorBlock(pixel, block, width, (unsigned int *)colors);

			/* overwrite alpha bits with alpha block */
			DDSDecodeAlphaExplicit(pixel, alphaBlock, width, alphaZero);
		}
	}

	/* return ok */
	return 0;
}



/*
DDSDecompressDXT5()
decompresses a dxt5 format texture
*/

static int DDSDecompressDXT5(ddsBuffer_t * dds, int width, int height, unsigned char *pixels)
{
	int             x, y, xBlocks, yBlocks;
	unsigned int   *pixel, alphaZero;
	ddsColorBlock_t *block;
	ddsAlphaBlock3BitLinear_t *alphaBlock;
	ddsColor_t      colors[4];


	/* setup */
	xBlocks = width / 4;
	yBlocks = height / 4;

	/* create zero alpha */
	colors[0].a = 0;
	colors[0].r = 0xFF;
	colors[0].g = 0xFF;
	colors[0].b = 0xFF;
	alphaZero = *((unsigned int *)&colors[0]);

	/* walk y */
	for(y = 0; y < yBlocks; y++)
	{
		/* 8 bytes per block, 1 block for alpha, 1 block for color */
		block = (ddsColorBlock_t *) ((unsigned int)dds->data + y * xBlocks * 16);

		/* walk x */
		for(x = 0; x < xBlocks; x++, block++)
		{
			/* get alpha block */
			alphaBlock = (ddsAlphaBlock3BitLinear_t *) block;

			/* get color block */
			block++;
			DDSGetColorBlockColors(block, colors);

			/* decode color block */
			pixel = (unsigned int *)(pixels + x * 16 + (y * 4) * width * 4);
			DDSDecodeColorBlock(pixel, block, width, (unsigned int *)colors);

			/* overwrite alpha bits with alpha block */
			DDSDecodeAlpha3BitLinear(pixel, alphaBlock, width, alphaZero);
		}
	}

	/* return ok */
	return 0;
}



/*
DDSDecompressDXT2()
decompresses a dxt2 format texture (fixme: un-premultiply alpha)
*/

static int DDSDecompressDXT2(ddsBuffer_t * dds, int width, int height, unsigned char *pixels)
{
	int             r;


	/* decompress dxt3 first */
	r = DDSDecompressDXT3(dds, width, height, pixels);

	/* return to sender */
	return r;
}



/*
DDSDecompressDXT4()
decompresses a dxt4 format texture (fixme: un-premultiply alpha)
*/

static int DDSDecompressDXT4(ddsBuffer_t * dds, int width, int height, unsigned char *pixels)
{
	int             r;


	/* decompress dxt5 first */
	r = DDSDecompressDXT5(dds, width, height, pixels);

	/* return to sender */
	return r;
}



/*
DDSDecompressARGB8888()
decompresses an argb 8888 format texture
*/

static int DDSDecompressARGB8888(ddsBuffer_t * dds, int width, int height, unsigned char *pixels)
{
	int             x, y;
	unsigned char  *in, *out;


	/* setup */
	in = dds->data;
	out = pixels;

	/* walk y */
	for(y = 0; y < height; y++)
	{
		/* walk x */
		for(x = 0; x < width; x++)
		{
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
			*out++ = *in++;
		}
	}

	/* return ok */
	return 0;
}



/*
DDSDecompress()
decompresses a dds texture into an rgba image buffer, returns 0 on success
*/

int DDSDecompress(ddsBuffer_t * dds, unsigned char *pixels)
{
	int             width, height, r;
	ddsPF_t         pf;


	/* get dds info */
	r = DDSGetInfo(dds, &width, &height, &pf);
	if(r)
		return r;

	/* decompress */
	switch (pf)
	{
		case DDS_PF_ARGB8888:
			/* fixme: support other [a]rgb formats */
			r = DDSDecompressARGB8888(dds, width, height, pixels);
			break;

		case DDS_PF_DXT1:
			r = DDSDecompressDXT1(dds, width, height, pixels);
			break;

		case DDS_PF_DXT2:
			r = DDSDecompressDXT2(dds, width, height, pixels);
			break;

		case DDS_PF_DXT3:
			r = DDSDecompressDXT3(dds, width, height, pixels);
			break;

		case DDS_PF_DXT4:
			r = DDSDecompressDXT4(dds, width, height, pixels);
			break;

		case DDS_PF_DXT5:
			r = DDSDecompressDXT5(dds, width, height, pixels);
			break;

		default:
		case DDS_PF_UNKNOWN:
			memset(pixels, 0xFF, width * height * 4);
			r = -1;
			break;
	}

	/* return to sender */
	return r;
}

/*
=============
LoadDDS
loads a dxtc (1, 3, 5) dds buffer into a valid rgba image
=============
*/
static void LoadDDS(const char *name, unsigned char **pic, int *width, int *height, byte alphabyte)
{
	int             w, h;
	ddsPF_t         pf;
	byte           *buffer;

	*pic = NULL;

	// load the file
	ri.FS_ReadFile((char *)name, (void **)&buffer);
	if(!buffer)
	{
		return;
	}

	// null out
	*pic = 0;
	*width = 0;
	*height = 0;

	// get dds info
	if(DDSGetInfo((ddsBuffer_t *) buffer, &w, &h, &pf))
	{
		ri.Error(ERR_DROP, "LoadDDS: Invalid DDS texture '%s'\n", name);
		return;
	}

	// only certain types of dds textures are supported
	if(pf != DDS_PF_ARGB8888 && pf != DDS_PF_DXT1 && pf != DDS_PF_DXT3 && pf != DDS_PF_DXT5)
	{
		ri.Error(ERR_DROP, "LoadDDS: Only DDS texture formats ARGB8888, DXT1, DXT3, and DXT5 are supported (%d) '%s'\n", pf,
				 name);
		return;
	}

	// create image pixel buffer
	*width = w;
	*height = h;
	*pic = ri.Malloc(w * h * 4);

	// decompress the dds texture
	DDSDecompress((ddsBuffer_t *) buffer, *pic);

	ri.FS_FreeFile(buffer);
}

//===================================================================


static void     R_LoadImage(char **buffer, byte ** pic, int *width, int *height, int *bits);

static void ParseHeightMap(char **text, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;
	float           scale;

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
	if(token[0] != ',')
	{
		ri.Printf(PRINT_WARNING, "WARNING: no matching ',' found\n");
		return;
	}

	token = Com_ParseExt(text, qfalse);
	scale = atof(token);

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

	token = Com_ParseExt(text, qfalse);
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

typedef struct
{
	char           *ext;
	void            (*ImageLoader) (const char *, unsigned char **, int *, int *, byte);
} imageExtToLoaderMap_t;

// Note that the ordering indicates the order of preference used
// when there are multiple images of different formats available
static imageExtToLoaderMap_t imageLoaders[] = {
	{"tga", LoadTGA},
	{"png", LoadPNG},
	{"jpg", LoadJPG},
	{"jpeg", LoadJPG},
	{"dds", LoadDDS}
};

static int      numImageLoaders = sizeof(imageLoaders) / sizeof(imageLoaders[0]);

/*
=================
R_LoadImage

Loads any of the supported image types into a cannonical
32 bit format.
=================
*/
static void R_LoadImage(char **buffer, byte ** pic, int *width, int *height, int *bits)
{
	char           *token;

	*pic = NULL;
	*width = 0;
	*height = 0;

	token = Com_ParseExt(buffer, qfalse);
	if(!token[0])
	{
		ri.Printf(PRINT_WARNING, "WARNING: NULL parameter for R_LoadImage\n");
		return;
	}

	//ri.Printf(PRINT_ALL, "R_LoadImage: token '%s'\n", token);

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
		qboolean        orgNameFailed = qfalse;
		int             i;
		const char     *ext;
		char            filename[MAX_QPATH];
		byte            alphaByte;

		// Tr3B: clear alpha of normalmaps for displacement mapping
		if(*bits & IF_NORMALMAP)
			alphaByte = 0x00;
		else
			alphaByte = 0xFF;

		Q_strncpyz(filename, token, sizeof(filename));

		ext = Com_GetExtension(filename);

		if(*ext)
		{
			// look for the correct loader and use it
			for(i = 0; i < numImageLoaders; i++)
			{
				if(!Q_stricmp(ext, imageLoaders[i].ext))
				{
					// load
					imageLoaders[i].ImageLoader(filename, pic, width, height, alphaByte);
					break;
				}
			}

			// a loader was found
			if(i < numImageLoaders)
			{
				if(*pic == NULL)
				{
					// loader failed, most likely because the file isn't there;
					// try again without the extension
					orgNameFailed = qtrue;
					Com_StripExtension(token, filename, MAX_QPATH);
				}
				else
				{
					// something loaded
					return;
				}
			}
		}

		// try and find a suitable match using all the image formats supported
		for(i = 0; i < numImageLoaders; i++)
		{
			char           *altName = va("%s.%s", filename, imageLoaders[i].ext);

			// load
			imageLoaders[i].ImageLoader(altName, pic, width, height, alphaByte);

			if(*pic)
			{
				if(orgNameFailed)
				{
					ri.Printf(PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n", token, altName);
				}

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
			return image;
		}
	}

	// load the pic from disk
	buffer_p = &buffer[0];
	R_LoadImage(&buffer_p, &pic, &width, &height, &bits);
	if(pic == NULL)
	{
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
	int             bitsIgnore;
	char            buffer[1024], filename[1024];
	char           *filename_p;

	if(!name)
	{
		return NULL;
	}

	Q_strncpyz(buffer, name, sizeof(buffer));
	hash = generateHashValue(buffer);

	// see if the image is already loaded
	for(image = hashTable[hash]; image; image = image->next)
	{
		if(!Q_stricmp(buffer, image->name))
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
		Com_sprintf(filename, sizeof(filename), "%s_%s", buffer, suf[i]);

		filename_p = &filename[0];
		R_LoadImage(&filename_p, &pic[i], &width, &height, &bitsIgnore);

		if(!pic[i] || width != height)
		{
			image = NULL;
			goto done;
		}
	}

	image = R_CreateCubeImage((char *)buffer, (const byte **)pic, width, height, bits, filterType, wrapType);

  done:
	for(i = 0; i < 6; i++)
	{
		if(pic[i])
			ri.Free(pic[i]);
	}
	return image;
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

static void R_CreateContrastRenderImage(void)
{
	int             width, height;
	byte           *data;

	if(glConfig.textureNPOTAvailable)
	{
		width = glConfig.vidWidth;
		height = glConfig.vidHeight;
	}
	else
	{
		width = NearestPowerOfTwo(glConfig.vidWidth);
		height = NearestPowerOfTwo(glConfig.vidHeight);
	}

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	tr.contrastRenderImage = R_CreateImage("_contrastRender", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_CLAMP);

	ri.Hunk_FreeTempMemory(data);
}

static void R_CreateCurrentRenderImage(void)
{
	int             width, height;
	byte           *data;

	if(glConfig.textureNPOTAvailable)
	{
		width = glConfig.vidWidth;
		height = glConfig.vidHeight;
	}
	else
	{
		width = NearestPowerOfTwo(glConfig.vidWidth);
		height = NearestPowerOfTwo(glConfig.vidHeight);
	}

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	tr.currentRenderImage = R_CreateImage("_currentRender", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_CLAMP);

	ri.Hunk_FreeTempMemory(data);
}

static void R_CreateDepthRenderImage(void)
{
	int             width, height;
	byte           *data;

	if(glConfig.textureNPOTAvailable)
	{
		width = glConfig.vidWidth;
		height = glConfig.vidHeight;
	}
	else
	{
		width = NearestPowerOfTwo(glConfig.vidWidth);
		height = NearestPowerOfTwo(glConfig.vidHeight);
	}

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	tr.depthRenderImage = R_CreateImage("_depthRender", data, width, height, IF_NOPICMIP | IF_DEPTH24, FT_NEAREST, WT_CLAMP);

	ri.Hunk_FreeTempMemory(data);
}

static void R_CreatePortalRenderImage(void)
{
	int             width, height;
	byte           *data;

	if(glConfig.textureNPOTAvailable)
	{
		width = glConfig.vidWidth;
		height = glConfig.vidHeight;
	}
	else
	{
		width = NearestPowerOfTwo(glConfig.vidWidth);
		height = NearestPowerOfTwo(glConfig.vidHeight);
	}

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	tr.portalRenderImage = R_CreateImage("_portalRender", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_CLAMP);

	ri.Hunk_FreeTempMemory(data);
}

static void R_CreateDeferredRenderFBOImages(void)
{
	int             width, height;
	byte           *data;

	if(!r_deferredShading->integer)
		return;

	if(glConfig.textureNPOTAvailable)
	{
		width = glConfig.vidWidth;
		height = glConfig.vidHeight;
	}
	else
	{
		width = NearestPowerOfTwo(glConfig.vidWidth);
		height = NearestPowerOfTwo(glConfig.vidHeight);
	}

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	if(glConfig.framebufferMixedFormatsAvailable)
	{
		tr.deferredDiffuseFBOImage =
			R_CreateImage("_deferredDiffuseFBO", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);
		tr.deferredNormalFBOImage = R_CreateImage("_deferredNormalFBO", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);
		tr.deferredSpecularFBOImage =
			R_CreateImage("_deferredSpecularFBO", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);
		tr.deferredPositionFBOImage =
			R_CreateImage("_deferredPositionFBO", data, width, height,
						  IF_NOPICMIP | (r_deferredShading->integer == 2 ? IF_RGBA32F : IF_RGBA16F), FT_NEAREST, WT_REPEAT);
		tr.deferredRenderFBOImage = R_CreateImage("_deferredRenderFBO", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);
	}
	else
	{
		tr.deferredDiffuseFBOImage =
			R_CreateImage("_deferredDiffuseFBO", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);
		tr.deferredNormalFBOImage = R_CreateImage("_deferredNormalFBO", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);
		tr.deferredSpecularFBOImage =
			R_CreateImage("_deferredSpecularFBO", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);
		tr.deferredPositionFBOImage =
			R_CreateImage("_deferredPositionFBO", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);
		tr.deferredRenderFBOImage = R_CreateImage("_deferredRenderFBO", data, width, height, IF_NOPICMIP, FT_NEAREST, WT_REPEAT);
	}

	ri.Hunk_FreeTempMemory(data);
}

// *INDENT-OFF*
static void R_CreateShadowMapFBOImage(void)
{
	int             i;
	int             width, height;
	byte           *data;

	if(!glConfig.textureFloatAvailable)
		return;

	for(i = 0; i < 5; i++)
	{
		width = height = shadowMapResolutions[i];

		data = ri.Hunk_AllocateTempMemory(width * height * 4);

		if(glConfig.hardwareType == GLHW_ATI)
		{
			tr.shadowMapFBOImage[i] = R_CreateImage(va("_shadowMapFBO%d", i), data, width, height, IF_NOPICMIP | IF_RGBA16, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_CLAMP);
		}
		else if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 4)
		{
			tr.shadowMapFBOImage[i] = R_CreateImage(va("_shadowMapFBO%d", i), data, width, height, IF_NOPICMIP | IF_LA16F, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_CLAMP);
		}
		else if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 5)
		{
			tr.shadowMapFBOImage[i] = R_CreateImage(va("_shadowMapFBO%d", i), data, width, height, IF_NOPICMIP | IF_LA32F, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_CLAMP);
		}
		else if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 6)
		{
			tr.shadowMapFBOImage[i] = R_CreateImage(va("_shadowMapFBO%d", i), data, width, height, IF_NOPICMIP | IF_ALPHA32F, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_CLAMP);
		}
		else
		{
			tr.shadowMapFBOImage[i] = R_CreateImage(va("_shadowMapFBO%d", i), data, width, height, IF_NOPICMIP | IF_RGBA16F, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_CLAMP);
		}

		ri.Hunk_FreeTempMemory(data);
	}
}
// *INDENT-ON*

// *INDENT-OFF*
static void R_CreateShadowCubeFBOImage(void)
{
	int             i, j;
	int             width, height;
	byte           *data[6];

	if(!glConfig.textureFloatAvailable)
		return;

	for(j = 0; j < 5; j++)
	{
		width = height = shadowMapResolutions[j];

		for(i = 0; i < 6; i++)
		{
			data[i] = ri.Hunk_AllocateTempMemory(width * height * 4);
		}

		if(glConfig.hardwareType == GLHW_ATI)
		{
			tr.shadowCubeFBOImage[j] = R_CreateCubeImage(va("_shadowCubeFBO%d", j), (const byte **)data, width, height, IF_NOPICMIP | IF_RGBA16, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_EDGE_CLAMP);
		}
		else if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 4)
		{
			tr.shadowCubeFBOImage[j] = R_CreateCubeImage(va("_shadowCubeFBO%d", j), (const byte **)data, width, height, IF_NOPICMIP | IF_LA16F, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_EDGE_CLAMP);
		}
		else if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 5)
		{
			tr.shadowCubeFBOImage[j] = R_CreateCubeImage(va("_shadowCubeFBO%d", j), (const byte **)data, width, height, IF_NOPICMIP | IF_LA32F, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_EDGE_CLAMP);
		}
		else if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 6)
		{
			tr.shadowCubeFBOImage[j] = R_CreateCubeImage(va("_shadowCubeFBO%d", j), (const byte **)data, width, height, IF_NOPICMIP | IF_ALPHA32F, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_EDGE_CLAMP);
		}
		else
		{
			tr.shadowCubeFBOImage[j] = R_CreateCubeImage(va("_shadowCubeFBO%d", j), (const byte **)data, width, height, IF_NOPICMIP | IF_RGBA16F, (r_shadowMapLinearFilter->integer ? FT_LINEAR : FT_NEAREST), WT_EDGE_CLAMP);
		}

		for(i = 5; i >= 0; i--)
		{
			ri.Hunk_FreeTempMemory(data[i]);
		}
	}
}
// *INDENT-ON*

/*
==================
R_CreateBuiltinImages
==================
*/
void R_CreateBuiltinImages(void)
{
	int             x, y;
	byte            data[DEFAULT_SIZE][DEFAULT_SIZE][4];
	byte           *out;
	float           s, t, value;
	byte            intensity;

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

	for(x = 0; x < 32; x++)
	{
		// scratchimage is usually used for cinematic drawing
		tr.scratchImage[x] = R_CreateImage("_scratch", (byte *) data, DEFAULT_SIZE, DEFAULT_SIZE, IF_NONE, FT_LINEAR, WT_CLAMP);
	}

	out = data;
	for(y = 0; y < 8; y++)
	{
		for(x = 0; x < 32; x++, out += 4)
		{
			s = (((float)x + 0.5f) * (2.0f / 32) - 1.0f);

			s = Q_fabs(s) - (1.0f / 32);

			value = 1.0f - (s * 2.0f) + (s * s);

			intensity = ClampByte(Q_ftol(value * 255.0f));

			out[0] = intensity;
			out[1] = intensity;
			out[2] = intensity;
			out[3] = intensity;
		}
	}

	tr.quadraticImage =
		R_CreateImage("_quadratic", (byte *) data, DEFAULT_SIZE, DEFAULT_SIZE, IF_NOPICMIP | IF_NOCOMPRESSION, FT_LINEAR,
					  WT_CLAMP);

	R_CreateNoFalloffImage();
	R_CreateAttenuationXYImage();
	R_CreateContrastRenderImage();
	R_CreateCurrentRenderImage();
	R_CreateDepthRenderImage();
	R_CreatePortalRenderImage();
	R_CreateDeferredRenderFBOImages();
	R_CreateShadowMapFBOImage();
	R_CreateShadowCubeFBOImage();
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
