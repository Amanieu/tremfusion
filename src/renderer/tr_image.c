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
	for(i = 0; i < tr.images.currentElements; i++)
	{
		image = Com_GrowListElement(&tr.images, i);

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
	image_t			*image;

	total = 0;
	for(i = 0; i < tr.images.currentElements; i++)
	{
		image = Com_GrowListElement(&tr.images, i);

		if(image->frameUsed == tr.frameCount)
		{
			total += image->uploadWidth * image->uploadHeight;
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
	int				imageDataSize;
	const char     *yesno[] = {
		"no ", "yes"
	};

	ri.Printf(PRINT_ALL, "\n      -w-- -h-- -mm- -type-   -if-- wrap --name-------\n");

	texels = 0;
	dataSize = 0;

	for(i = 0; i < tr.images.currentElements; i++)
	{
		image = Com_GrowListElement(&tr.images, i);

		ri.Printf(PRINT_ALL, "%4i: %4i %4i  %s   ",
				  i, image->uploadWidth, image->uploadHeight, yesno[image->filterType == FT_DEFAULT]);

		switch (image->type)
		{
			case GL_TEXTURE_2D:
				texels += image->uploadWidth * image->uploadHeight;
				imageDataSize = image->uploadWidth * image->uploadHeight;

				ri.Printf(PRINT_ALL, "2D   ");
				break;

			case GL_TEXTURE_CUBE_MAP_ARB:
				texels += image->uploadWidth * image->uploadHeight * 6;
				imageDataSize = image->uploadWidth * image->uploadHeight * 6;

				ri.Printf(PRINT_ALL, "CUBE ");
				break;

			default:
				ri.Printf(PRINT_ALL, "???? ");
				imageDataSize = image->uploadWidth * image->uploadHeight;
				break;
		}

		switch (image->internalFormat)
		{
			case GL_RGB8:
				ri.Printf(PRINT_ALL, "RGB8     ");
				imageDataSize *= 3;
				break;

			case GL_RGBA8:
				ri.Printf(PRINT_ALL, "RGBA8    ");
				imageDataSize *= 4;
				break;

			case GL_RGB16:
				ri.Printf(PRINT_ALL, "RGB      ");
				imageDataSize *= 6;
				break;

			case GL_RGB16F_ARB:
				ri.Printf(PRINT_ALL, "RGB16F   ");
				imageDataSize *= 6;
				break;

			case GL_RGB32F_ARB:
				ri.Printf(PRINT_ALL, "RGB32F   ");
				imageDataSize *= 12;
				break;

			case GL_RGBA16F_ARB:
				ri.Printf(PRINT_ALL, "RGBA16F  ");
				imageDataSize *= 8;
				break;

			case GL_RGBA32F_ARB:
				ri.Printf(PRINT_ALL, "RGBA32F  ");
				imageDataSize *= 16;
				break;

			case GL_ALPHA16F_ARB:
				ri.Printf(PRINT_ALL, "A16F     ");
				imageDataSize *= 2;
				break;

			case GL_ALPHA32F_ARB:
				ri.Printf(PRINT_ALL, "A32F     ");
				imageDataSize *= 4;
				break;

			case GL_LUMINANCE_ALPHA16F_ARB:
				ri.Printf(PRINT_ALL, "LA16F    ");
				imageDataSize *= 4;
				break;

			case GL_LUMINANCE_ALPHA32F_ARB:
				ri.Printf(PRINT_ALL, "LA32F    ");
				imageDataSize *= 8;
				break;

			case GL_COMPRESSED_RGBA_ARB:
				ri.Printf(PRINT_ALL, "ARB      ");
				imageDataSize *= 4;	// FIXME
				break;

			case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
				ri.Printf(PRINT_ALL, "DXT1     ");
				imageDataSize *= 4 / 8;
				break;

			case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
				ri.Printf(PRINT_ALL, "DXT1a    ");
				imageDataSize *= 4 / 8;
				break;

			case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
				ri.Printf(PRINT_ALL, "DXT3     ");
				imageDataSize *= 4 / 4;
				break;

			case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
				ri.Printf(PRINT_ALL, "DXT5     ");
				imageDataSize *= 4 / 4;
				break;

			case GL_DEPTH_COMPONENT16_ARB:
				ri.Printf(PRINT_ALL, "D16      ");
				imageDataSize *= 2;
				break;

			case GL_DEPTH_COMPONENT24_ARB:
				ri.Printf(PRINT_ALL, "D24      ");
				imageDataSize *= 3;
				break;

			case GL_DEPTH_COMPONENT32_ARB:
				ri.Printf(PRINT_ALL, "D32      ");
				imageDataSize *= 4;
				break;

			default:
				ri.Printf(PRINT_ALL, "????     ");
				imageDataSize *= 4;
				break;
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

		dataSize += imageDataSize;

		ri.Printf(PRINT_ALL, " %s\n", image->name);
	}
	ri.Printf(PRINT_ALL, " ---------\n");
	ri.Printf(PRINT_ALL, " %i total texels (not including mipmaps)\n", texels);
	ri.Printf(PRINT_ALL, " %d.%02d MB total image memory\n", dataSize / (1024 * 1024),
			  (dataSize % (1024 * 1024)) * 100 / (1024 * 1024));
	ri.Printf(PRINT_ALL, " %i total images\n\n", tr.images.currentElements);
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
void R_UploadImage(const byte ** dataArray, int numData, image_t * image)
{
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
	else if(image->bits & (IF_PACKED_DEPTH24_STENCIL8))
	{
		format = GL_DEPTH_STENCIL_EXT;
		internalFormat = GL_DEPTH24_STENCIL8_EXT;
	}
	else if(glConfig.textureFloatAvailable &&
			(image->bits & (IF_RGBA16F | IF_RGBA32F | IF_RGBA16 | IF_LA16F | IF_LA32F | IF_ALPHA16F | IF_ALPHA32F)))
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
	else if(image->bits & IF_RGBE)
	{
		internalFormat = GL_RGBA8;
	}
	else
	{
		int             samples;

		samples = 3;

		// Tr3B: normalmaps have the displacement maps in the alpha channel
		// samples 3 would cause an opaque alpha channel and odd displacements!
		if(image->bits & IF_NORMALMAP)
		{
			if(image->bits & (IF_DISPLACEMAP | IF_ALPHATEST))
				samples = 4;
			else
				samples = 3;
		}
		else if(image->bits & IF_LIGHTMAP)
		{
			samples = 3;
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
			if(glConfig.textureCompression == TC_S3TC && !(image->bits & IF_NOCOMPRESSION))
			{
				internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
			}
			else
			{
				internalFormat = GL_RGB8;
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
			else
			{
				if(glConfig.textureCompression == TC_S3TC && !(image->bits & IF_NOCOMPRESSION))
				{
					if(image->bits & IF_DISPLACEMAP)
					{
						internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
					}
					else if(image->bits & IF_ALPHATEST)
					{
						internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
					}
					else
					{
						internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
					}
				}
				else
				{
					internalFormat = GL_RGBA8;
				}
			}
		}
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
			//qglHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);	// make sure its nice
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
				if(image->bits & IF_PACKED_DEPTH24_STENCIL8)
				{
					qglTexImage2D(target, 0, internalFormat, scaledWidth, scaledHeight, 0, format, GL_UNSIGNED_INT_24_8_EXT, NULL);
				}
				else
				{
					qglTexImage2D(target, 0, internalFormat, scaledWidth, scaledHeight, 0, format, GL_UNSIGNED_BYTE, scaledBuffer);
				}
				break;
		}

		if(!glConfig.generateMipmapAvailable)
		{
			if(image->filterType == FT_DEFAULT && !(image->bits & (IF_DEPTH16 | IF_DEPTH24 | IF_DEPTH32 | IF_PACKED_DEPTH24_STENCIL8)))
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
R_AllocImage
================
*/
image_t        *R_AllocImage(const char *name, qboolean linkIntoHashTable)
{
	image_t        *image;
	long            hash;
	char            buffer[1024];

//  if(strlen(name) >= MAX_QPATH)
	if(strlen(name) >= 1024)
	{
		ri.Error(ERR_DROP, "R_AllocImage: \"%s\" image name is too long\n", name);
		return NULL;
	}

	/*
	if(tr.numImages == MAX_DRAWIMAGES)
	{
		ri.Error(ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit\n");
		return NULL;
	}
	*/

	image = ri.Hunk_Alloc(sizeof(image_t), h_low);
	Com_Memset(image, 0, sizeof(image_t));
	qglGenTextures(1, &image->texnum);

	Com_AddToGrowList(&tr.images, image);

	Q_strncpyz(image->name, name, sizeof(image->name));

	if(linkIntoHashTable)
	{
		Q_strncpyz(buffer, name, sizeof(buffer));
		hash = generateHashValue(buffer);
		image->next = hashTable[hash];
		hashTable[hash] = image;
	}

	return image;
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

	image = R_AllocImage(name, qtrue);
	if(!image)
		return NULL;

	image->type = GL_TEXTURE_2D;

	image->width = width;
	image->height = height;

	image->bits = bits;
	image->filterType = filterType;
	image->wrapType = wrapType;

	GL_Bind(image);

	R_UploadImage(&pic, 1, image);

	//GL_Unbind();
	qglBindTexture(image->type, 0);

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
	image_t        *image;

	image = R_AllocImage(name, qtrue);
	if(!image)
		return NULL;

	image->type = GL_TEXTURE_CUBE_MAP_ARB;

	image->width = width;
	image->height = height;

	image->bits = bits;
	image->filterType = filterType;
	image->wrapType = wrapType;

	GL_Bind(image);

	R_UploadImage(pic, 6, image);

	qglBindTexture(image->type, 0);

	return image;
}




static void     R_LoadImage(char **buffer, byte ** pic, int *width, int *height, int *bits);
image_t        *R_LoadDDSImage(const char *name, int bits, filterType_t filterType, wrapType_t wrapType);

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
	*bits |= IF_DISPLACEMAP;
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

//	*bits |= IF_INTENSITY;
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
//	*bits |= IF_ALPHA;
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
	{"bmp",  R_LoadBMP},
//	{"dds", LoadDDS},	// need to write some direct uploader routines first
//	{"hdr", LoadRGBE}	// RGBE just sucks
};

static int      numImageLoaders = sizeof(imageLoaders) / sizeof(imageLoaders[0]);

/*
=================
R_LoadImage

Loads any of the supported image types into a canonical
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
	char            ddsName[1024];
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

	if(glConfig.textureCompression == TC_S3TC && !(bits & IF_NOCOMPRESSION) && Q_stricmpn(name, "fonts", 5))
	{
		Q_strncpyz(ddsName, name, sizeof(ddsName));
		Com_StripExtension(ddsName, ddsName, sizeof(ddsName));
		Q_strcat(ddsName, sizeof(ddsName), ".dds");

		// try to load a customized .dds texture
		image = R_LoadDDSImage(ddsName, bits, filterType, wrapType);
		if(image != NULL)
		{
			ri.Printf(PRINT_ALL, "found custom .dds '%s'\n", ddsName);
			return image;
		}
	}
#if 0
	else if(r_tryCachedDDSImages->integer && !(bits & IF_NOCOMPRESSION) && Q_strncasecmp(name, "fonts", 5))
	{
		Q_strncpyz(ddsName, "dds/", sizeof(ddsName));
		Q_strcat(ddsName, sizeof(ddsName), name);
		Com_StripExtension(ddsName, ddsName, sizeof(ddsName));
		Q_strcat(ddsName, sizeof(ddsName), ".dds");

		// try to load a cached .dds texture from the XreaL/<mod>/dds/ folder
		image = R_LoadDDSImage(ddsName, bits, filterType, wrapType);
		if(image != NULL)
		{
			ri.Printf(PRINT_ALL, "found cached .dds '%s'\n", ddsName);
			return image;
		}
	}
#endif

	// load the pic from disk
	buffer_p = &buffer[0];
	R_LoadImage(&buffer_p, &pic, &width, &height, &bits);
	if(pic == NULL)
	{
		return NULL;
	}

#if 0
	//if(r_tryCachedDDSImages->integer && !(bits & IF_NOCOMPRESSION) && Q_strncasecmp(name, "fonts", 5))
	{
		// try to cache a .dds texture to the XreaL/<mod>/dds/ folder
		SavePNG(ddsName, pic, width, height, 4, qtrue);
	}
#endif

	image = R_CreateImage((char *)buffer, pic, width, height, bits, filterType, wrapType);
	ri.Free(pic);
	return image;
}


static ID_INLINE void SwapPixel(byte * inout, int x, int y, int x2, int y2, int width, int height)
{
	byte			color[4];
	byte			color2[4];

	color[0] =  inout[4 * (y * width + x) + 0];
	color[1] =  inout[4 * (y * width + x) + 1];
	color[2] =  inout[4 * (y * width + x) + 2];
	color[3] =  inout[4 * (y * width + x) + 3];

	color2[0] =  inout[4 * (y2 * width + x2) + 0];
	color2[1] =  inout[4 * (y2 * width + x2) + 1];
	color2[2] =  inout[4 * (y2 * width + x2) + 2];
	color2[3] =  inout[4 * (y2 * width + x2) + 3];

	inout[4 * (y * width + x) + 0] = color2[0];
	inout[4 * (y * width + x) + 1] = color2[1];
	inout[4 * (y * width + x) + 2] = color2[2];
	inout[4 * (y * width + x) + 3] = color2[3];

	inout[4 * (y2 * width + x2) + 0] = color[0];
	inout[4 * (y2 * width + x2) + 1] = color[1];
	inout[4 * (y2 * width + x2) + 2] = color[2];
	inout[4 * (y2 * width + x2) + 3] = color[3];
}

static void R_Flip(byte * in, int width, int height)
{
	int             x, y;
	byte           *out;

	out = in;

	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width / 2; x++)
		{
			SwapPixel(in, x, y, (width -1 -x), y, width, height);
		}
	}
}

static void R_Flop(byte * in, int width, int height)
{
	int             x, y;
	byte           *out;

	out = in;

	for(y = 0; y < height / 2; y++)
	{
		for(x = 0; x < width; x++)
		{
			SwapPixel(in, x, y, x, (height -1 -y), width, height);
		}
	}
}

static void R_Rotate(byte * in, int width, int height, int degrees)
{
	byte			color[4];
	int             x, y, x2, y2;
	byte           *out, *tmp;

	tmp = Com_Allocate(width * height * 4);

	// rotate into tmp buffer
	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			color[0] =  in[4 * (y * width + x) + 0];
			color[1] =  in[4 * (y * width + x) + 1];
			color[2] =  in[4 * (y * width + x) + 2];
			color[3] =  in[4 * (y * width + x) + 3];

			if(degrees == 90)
			{
				x2 = y;
				y2 = (height -(1 + x));

				tmp[4 * (y2 * width + x2) + 0] = color[0];
				tmp[4 * (y2 * width + x2) + 1] = color[1];
				tmp[4 * (y2 * width + x2) + 2] = color[2];
				tmp[4 * (y2 * width + x2) + 3] = color[3];
			}
			else if(degrees == -90)
			{
				x2 = (width -(1 + y));
				y2 = x;

				tmp[4 * (y2 * width + x2) + 0] = color[0];
				tmp[4 * (y2 * width + x2) + 1] = color[1];
				tmp[4 * (y2 * width + x2) + 2] = color[2];
				tmp[4 * (y2 * width + x2) + 3] = color[3];
			}
			else
			{
				tmp[4 * (y * width + x) + 0] = color[0];
				tmp[4 * (y * width + x) + 1] = color[1];
				tmp[4 * (y * width + x) + 2] = color[2];
				tmp[4 * (y * width + x) + 3] = color[3];
			}

		}
	}

	// copy back to input
	out = in;
	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			out[4 * (y * width + x) + 0] = tmp[4 * (y * width + x) + 0];
			out[4 * (y * width + x) + 1] = tmp[4 * (y * width + x) + 1];
			out[4 * (y * width + x) + 2] = tmp[4 * (y * width + x) + 2];
			out[4 * (y * width + x) + 3] = tmp[4 * (y * width + x) + 3];
		}
	}

	Com_Dealloc(tmp);
}

/*
===============
R_FindCubeImage

Finds or loads the given image.
Returns NULL if it fails, not a default image.

Tr3B: fear the use of goto
==============
*/
image_t        *R_FindCubeImage(const char *name, int bits, filterType_t filterType, wrapType_t wrapType)
{
	int             i;
	image_t        *image = NULL;
	int             width = 0, height = 0;
	byte           *pic[6];
	long            hash;

	static char    *openglSuffices[6] = { "px", "nx", "py", "ny", "pz", "nz" };

	/*
		convert $1_forward.tga -flip -rotate 90 $1_px.png
		convert $1_back.tga -flip -rotate -90 $1_nx.png

		convert $1_left.tga -flip $1_py.png
		convert $1_right.tga -flop $1_ny.png

		convert $1_up.tga -flip -rotate 90 $1_pz.png
		convert $1_down.tga -flop -rotate -90 $1_nz.png
	 */

	static char    *doom3Suffices[6] = { "forward", "back", "left", "right", "up", "down" };
	static qboolean doom3FlipX[6] = { qtrue,	qtrue, 	qfalse,	qtrue,	qtrue,	qfalse };
	static qboolean doom3FlipY[6] = { qfalse,	qfalse,	qtrue,	qfalse,	qfalse,	qtrue };
	static int		doom3Rot[6] =	{ 90, 		-90,	0,		0,		90,		-90 };

	/*
		convert $1_rt.tga -flip -rotate 90 $1_px.tga
		convert $1_lf.tga -flip -rotate -90 $1_nx.tga

		convert $1_bk.tga -flip $1_py.tga
		convert $1_ft.tga -flop $1_ny.tga

		convert $1_up.tga -flip -rotate 90 $1_pz.tga
		convert $1_dn.tga -flop -rotate -90 $1_nz.tga
	 */
	static char    *quakeSuffices[6] = { "rt", "lf", "bk", "ft", "up", "dn" };
	static qboolean quakeFlipX[6] = { qtrue,	qtrue, 	qfalse,	qtrue,	qtrue,	qfalse };
	static qboolean quakeFlipY[6] = { qfalse,	qfalse,	qtrue,	qfalse,	qfalse,	qtrue };
	static int		quakeRot[6] =	{ 90, 		-90,	0,		0,		90,		-90 };


	int             bitsIgnore;
	char            buffer[1024], filename[1024];
	char            ddsName[1024];
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


	if(glConfig.textureCompression == TC_S3TC && !(bits & IF_NOCOMPRESSION) && Q_stricmpn(name, "fonts", 5))
	{
		Q_strncpyz(ddsName, name, sizeof(ddsName));
		Com_StripExtension(ddsName, ddsName, sizeof(ddsName));
		Q_strcat(ddsName, sizeof(ddsName), ".dds");

		// try to load a customized .dds texture
		image = R_LoadDDSImage(ddsName, bits, filterType, wrapType);
		if(image != NULL)
		{
			ri.Printf(PRINT_ALL, "found custom .dds '%s'\n", ddsName);
			return image;
		}
	}
#if 0
	else if(r_tryCachedDDSImages->integer && !(bits & IF_NOCOMPRESSION) && Q_strncasecmp(name, "fonts", 5))
	{
		Q_strncpyz(ddsName, "dds/", sizeof(ddsName));
		Q_strcat(ddsName, sizeof(ddsName), name);
		Com_StripExtension(ddsName, ddsName, sizeof(ddsName));
		Q_strcat(ddsName, sizeof(ddsName), ".dds");

		// try to load a cached .dds texture from the XreaL/<mod>/dds/ folder
		image = R_LoadDDSImage(ddsName, bits, filterType, wrapType);
		if(image != NULL)
		{
			ri.Printf(PRINT_ALL, "found cached .dds '%s'\n", ddsName);
			return image;
		}
	}
#endif

	for(i = 0; i < 6; i++)
	{
		pic[i] = NULL;
	}

	for(i = 0; i < 6; i++)
	{
		Com_sprintf(filename, sizeof(filename), "%s_%s", buffer, openglSuffices[i]);

		filename_p = &filename[0];
		R_LoadImage(&filename_p, &pic[i], &width, &height, &bitsIgnore);

		if(!pic[i] || width != height)
		{
			image = NULL;
			goto tryDoom3Suffices;
		}
	}
	goto createCubeImage;

  tryDoom3Suffices:
	for(i = 0; i < 6; i++)
  	{
  		Com_sprintf(filename, sizeof(filename), "%s_%s", buffer, doom3Suffices[i]);

  		filename_p = &filename[0];
  		R_LoadImage(&filename_p, &pic[i], &width, &height, &bitsIgnore);

  		if(!pic[i] || width != height)
		{
			image = NULL;
			goto tryQuakeSuffices;
		}

  		if(doom3FlipX[i])
  		{
  			R_Flip(pic[i], width, height);
  		}

  		if(doom3FlipY[i])
		{
			R_Flop(pic[i], width, height);
		}

  		R_Rotate(pic[i], width, height, doom3Rot[i]);
  	}
	goto createCubeImage;

  tryQuakeSuffices:
	for(i = 0; i < 6; i++)
	{
		Com_sprintf(filename, sizeof(filename), "%s_%s", buffer, quakeSuffices[i]);

		filename_p = &filename[0];
		R_LoadImage(&filename_p, &pic[i], &width, &height, &bitsIgnore);

		if(!pic[i] || width != height)
		{
			image = NULL;
			goto skipCubeImage;
		}

		if(quakeFlipX[i])
		{
			R_Flip(pic[i], width, height);
		}

		if(quakeFlipY[i])
		{
			R_Flop(pic[i], width, height);
		}

		R_Rotate(pic[i], width, height, quakeRot[i]);
	}

  createCubeImage:
	image = R_CreateCubeImage((char *)buffer, (const byte **)pic, width, height, bits, filterType, wrapType);

  skipCubeImage:
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

static void R_CreateContrastRenderFBOImage(void)
{
	int             width, height;
	byte           *data;

	if(glConfig.textureNPOTAvailable)
	{
		width = glConfig.vidWidth * 0.25f;
		height = glConfig.vidHeight * 0.25f;
	}
	else
	{
		width = NearestPowerOfTwo(glConfig.vidWidth) * 0.25f;
		height = NearestPowerOfTwo(glConfig.vidHeight) * 0.25f;
	}

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
	{
		tr.contrastRenderFBOImage = R_CreateImage("_contrastRenderFBO", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION | IF_RGBA16F, FT_LINEAR, WT_CLAMP);
	}
	else
	{
		tr.contrastRenderFBOImage = R_CreateImage("_contrastRenderFBO", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_LINEAR, WT_CLAMP);
	}

	ri.Hunk_FreeTempMemory(data);
}

static void R_CreateBloomRenderFBOImage(void)
{
	int				i;
	int             width, height;
	byte           *data;

	if(glConfig.textureNPOTAvailable)
	{
		width = glConfig.vidWidth * 0.25f;
		height = glConfig.vidHeight * 0.25f;
	}
	else
	{
		width = NearestPowerOfTwo(glConfig.vidWidth) * 0.25f;
		height = NearestPowerOfTwo(glConfig.vidHeight) * 0.25f;
	}

	data = ri.Hunk_AllocateTempMemory(width * height * 4);

	for(i = 0; i < 2; i++)
	{
		if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
		{
			tr.bloomRenderFBOImage[i] = R_CreateImage(va("_bloomRenderFBO%d", i), data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION | IF_RGBA16F, FT_LINEAR, WT_CLAMP);
		}
		else
		{
			tr.bloomRenderFBOImage[i] = R_CreateImage(va("_bloomRenderFBO%d", i), data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_LINEAR, WT_CLAMP);
		}
	}

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

	tr.currentRenderImage = R_CreateImage("_currentRender", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);

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

	if(glConfig.hardwareType == GLHW_ATI || glConfig.hardwareType == GLHW_ATI_DX10)// || glConfig.hardwareType == GLHW_NV_DX10)
	{
		tr.depthRenderImage = R_CreateImage("_depthRender", data, width, height, IF_NOPICMIP | IF_DEPTH16, FT_NEAREST, WT_CLAMP);
	}
	else if(glConfig.framebufferPackedDepthStencilAvailable)
	{
		tr.depthRenderImage = R_CreateImage("_depthRender", data, width, height, IF_NOPICMIP | IF_PACKED_DEPTH24_STENCIL8, FT_NEAREST, WT_CLAMP);
	}
	else
	{
		tr.depthRenderImage = R_CreateImage("_depthRender", data, width, height, IF_NOPICMIP | IF_DEPTH24, FT_NEAREST, WT_CLAMP);
	}
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

	if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
	{
		tr.portalRenderImage = R_CreateImage("_portalRender", data, width, height, IF_NOPICMIP | IF_RGBA16F, FT_NEAREST, WT_CLAMP);
	}
	else
	{
		tr.portalRenderImage = R_CreateImage("_portalRender", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}

	ri.Hunk_FreeTempMemory(data);
}

static void R_CreateOcclusionRenderFBOImage(void)
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

	//
#if 0
	if(glConfig.hardwareType == GLHW_ATI_DX10 || glConfig.hardwareType == GLHW_NV_DX10)
	{
		tr.occlusionRenderFBOImage = R_CreateImage("_occlusionFBORender", data, width, height, IF_NOPICMIP | IF_ALPHA16F, FT_NEAREST, WT_CLAMP);
	}
	else if(glConfig.framebufferPackedDepthStencilAvailable)
	{
		tr.occlusionRenderFBOImage = R_CreateImage("_occlusionFBORender", data, width, height, IF_NOPICMIP | IF_ALPHA32F, FT_NEAREST, WT_CLAMP);
	}
	else
#endif
	{
		tr.occlusionRenderFBOImage = R_CreateImage("_occlusionFBORender", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}

	ri.Hunk_FreeTempMemory(data);
}

static void R_CreateDepthToColorFBOImages(void)
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

#if 0
	if(glConfig.hardwareType == GLHW_ATI_DX10)
	{
		tr.depthToColorBackFacesFBOImage = R_CreateImage("_depthToColorBackFacesFBORender", data, width, height, IF_NOPICMIP | IF_ALPHA16F, FT_NEAREST, WT_CLAMP);
		tr.depthToColorFrontFacesFBOImage = R_CreateImage("_depthToColorFrontFacesFBORender", data, width, height, IF_NOPICMIP | IF_ALPHA16F, FT_NEAREST, WT_CLAMP);
	}
	else if(glConfig.hardwareType == GLHW_NV_DX10)
	{
		tr.depthToColorBackFacesFBOImage = R_CreateImage("_depthToColorBackFacesFBORender", data, width, height, IF_NOPICMIP | IF_ALPHA32F, FT_NEAREST, WT_CLAMP);
		tr.depthToColorFrontFacesFBOImage = R_CreateImage("_depthToColorFrontFacesFBORender", data, width, height, IF_NOPICMIP | IF_ALPHA32F, FT_NEAREST, WT_CLAMP);
	}
	else if(glConfig.framebufferPackedDepthStencilAvailable)
	{
		tr.depthToColorBackFacesFBOImage = R_CreateImage("_depthToColorBackFacesFBORender", data, width, height, IF_NOPICMIP | IF_ALPHA32F, FT_NEAREST, WT_CLAMP);
		tr.depthToColorFrontFacesFBOImage = R_CreateImage("_depthToColorFrontFacesFBORender", data, width, height, IF_NOPICMIP | IF_ALPHA32F, FT_NEAREST, WT_CLAMP);
	}
	else
#endif
	{
		tr.depthToColorBackFacesFBOImage = R_CreateImage("_depthToColorBackFacesFBORender", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
		tr.depthToColorFrontFacesFBOImage = R_CreateImage("_depthToColorFrontFacesFBORender", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}

	ri.Hunk_FreeTempMemory(data);
}

// Tr3B: clean up this mess some day ...
static void R_CreateDownScaleFBOImages(void)
{
	byte           *data;
	int				width, height;


	if(glConfig.textureNPOTAvailable)
	{
		width = glConfig.vidWidth * 0.25f;
		height = glConfig.vidHeight * 0.25f;
	}
	else
	{
		width = NearestPowerOfTwo(glConfig.vidWidth * 0.25f);
		height = NearestPowerOfTwo(glConfig.vidHeight * 0.25f);
	}

	data = ri.Hunk_AllocateTempMemory(width * height * 4);
	if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
	{
		tr.downScaleFBOImage_quarter = R_CreateImage("_downScaleFBOImage_quarter", data, width, height, IF_NOPICMIP | IF_RGBA16F, FT_NEAREST, WT_CLAMP);
	}
	else
	{
		tr.downScaleFBOImage_quarter = R_CreateImage("_downScaleFBOImage_quarter", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}
	ri.Hunk_FreeTempMemory(data);


	width = height = 64;
	data = ri.Hunk_AllocateTempMemory(width * height * 4);
	if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
	{
		tr.downScaleFBOImage_64x64 = R_CreateImage("_downScaleFBOImage_64x64", data, width, height, IF_NOPICMIP | IF_RGBA16F, FT_NEAREST, WT_CLAMP);
	}
	else
	{
		tr.downScaleFBOImage_64x64 = R_CreateImage("_downScaleFBOImage_64x64", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}
	ri.Hunk_FreeTempMemory(data);

#if 0
	width = height = 16;
	data = ri.Hunk_AllocateTempMemory(width * height * 4);
	if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
	{
		tr.downScaleFBOImage_16x16 = R_CreateImage("_downScaleFBOImage_16x16", data, width, height, IF_NOPICMIP | IF_RGBA16F, FT_NEAREST, WT_CLAMP);
	}
	else
	{
		tr.downScaleFBOImage_16x16 = R_CreateImage("_downScaleFBOImage_16x16", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}
	ri.Hunk_FreeTempMemory(data);


	width = height = 4;
	data = ri.Hunk_AllocateTempMemory(width * height * 4);
	if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
	{
		tr.downScaleFBOImage_4x4 = R_CreateImage("_downScaleFBOImage_4x4", data, width, height, IF_NOPICMIP | IF_RGBA16F, FT_NEAREST, WT_CLAMP);
	}
	else
	{
		tr.downScaleFBOImage_4x4 = R_CreateImage("_downScaleFBOImage_4x4", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}
	ri.Hunk_FreeTempMemory(data);


	width = height = 1;
	data = ri.Hunk_AllocateTempMemory(width * height * 4);
	if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
	{
		tr.downScaleFBOImage_1x1 = R_CreateImage("_downScaleFBOImage_1x1", data, width, height, IF_NOPICMIP | IF_RGBA16F, FT_NEAREST, WT_CLAMP);
	}
	else
	{
		tr.downScaleFBOImage_1x1 = R_CreateImage("_downScaleFBOImage_1x1", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}
	ri.Hunk_FreeTempMemory(data);
#endif
}

static void R_CreateDeferredRenderFBOImages(void)
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

	if(r_deferredShading->integer && glConfig.maxColorAttachments >= 4 && glConfig.textureFloatAvailable &&
		   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		tr.deferredDiffuseFBOImage = R_CreateImage("_deferredDiffuseFBO", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
		tr.deferredNormalFBOImage = R_CreateImage("_deferredNormalFBO", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
		tr.deferredSpecularFBOImage = R_CreateImage("_deferredSpecularFBO", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}

	if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
	{
		tr.deferredRenderFBOImage = R_CreateImage("_deferredRenderFBO", data, width, height, IF_NOPICMIP | IF_RGBA16F, FT_NEAREST, WT_CLAMP);
	}
	else
	{
		tr.deferredRenderFBOImage = R_CreateImage("_deferredRenderFBO", data, width, height, IF_NOPICMIP | IF_NOCOMPRESSION, FT_NEAREST, WT_CLAMP);
	}

	ri.Hunk_FreeTempMemory(data);
}

// *INDENT-OFF*
static void R_CreateShadowMapFBOImage(void)
{
	int             i;
	int             width, height;
	byte           *data;

	if(!glConfig.textureFloatAvailable || r_shadows->integer <= 3)
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

	if(!glConfig.textureFloatAvailable || r_shadows->integer <= 3)
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

// *INDENT-OFF*
static void R_CreateBlackCubeImage(void)
{
	int             i;
	int             width, height;
	byte           *data[6];

	width = REF_CUBEMAP_SIZE;
	height = REF_CUBEMAP_SIZE;

	for(i = 0; i < 6; i++)
	{
		data[i] = ri.Hunk_AllocateTempMemory(width * height * 4);
		Com_Memset(data[i], 0, width * height * 4);
	}

	tr.blackCubeImage = R_CreateCubeImage("_blackCube", (const byte **)data, width, height, IF_NOPICMIP, FT_LINEAR, WT_EDGE_CLAMP);
	tr.autoCubeImage = R_CreateCubeImage("_autoCube", (const byte **)data, width, height, IF_NOPICMIP, FT_LINEAR, WT_EDGE_CLAMP);

	for(i = 5; i >= 0; i--)
	{
		ri.Hunk_FreeTempMemory(data[i]);
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
	float           s, value;
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
	R_CreateContrastRenderFBOImage();
	R_CreateBloomRenderFBOImage();
	R_CreateCurrentRenderImage();
	R_CreateDepthRenderImage();
	R_CreatePortalRenderImage();
	R_CreateOcclusionRenderFBOImage();
	R_CreateDepthToColorFBOImages();
	R_CreateDownScaleFBOImages();
	R_CreateDeferredRenderFBOImages();
	R_CreateShadowMapFBOImage();
	R_CreateShadowCubeFBOImage();
	R_CreateBlackCubeImage();
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
	const char *charsetImage = "gfx/2d/charset-bezerk-plain-rc2.png";

	ri.Printf(PRINT_ALL, "------- R_InitImages -------\n");

	Com_Memset(hashTable, 0, sizeof(hashTable));
	Com_InitGrowList(&tr.images, 4096);
	Com_InitGrowList(&tr.lightmaps, 128);
	Com_InitGrowList(&tr.deluxemaps, 128);

	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();

	/*tr.charsetImage = R_FindImageFile(charsetImage, IF_NOCOMPRESSION | IF_NOPICMIP, FT_DEFAULT, WT_CLAMP);
	if(!tr.charsetImage)
	{
		ri.Error(ERR_FATAL, "R_InitImages: could not load '%s'", charsetImage);
	}*/
}


/*
===============
R_ShutdownImages
===============
*/
void R_ShutdownImages(void)
{
	int             i;
	image_t			*image;

	ri.Printf(PRINT_ALL, "------- R_ShutdownImages -------\n");

	for(i = 0; i < tr.images.currentElements; i++)
	{
		image = Com_GrowListElement(&tr.images, i);
		qglDeleteTextures(1, &image->texnum);
	}

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

	Com_DestroyGrowList(&tr.images);
	Com_DestroyGrowList(&tr.lightmaps);
	Com_DestroyGrowList(&tr.deluxemaps);
	Com_DestroyGrowList(&tr.cubeProbes);
	FreeVertexHashTable(tr.cubeHashTable);
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
