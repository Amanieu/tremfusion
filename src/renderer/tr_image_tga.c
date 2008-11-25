/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"

void R_LoadTGA ( const char *name, byte **pic, int *width, int *height, byte alphaByte)
{
	int             columns, rows, numPixels, file_read;
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
	file_read = ri.FS_ReadFile((char *)name, (void **)&buffer);
	if(!buffer)
	{
		ri.Printf(PRINT_DEVELOPER, "Got back %d from FS_ReadFile when reading tga image '%s'\n", file_read, (char*)name);
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
	numPixels = columns * rows;

	if(width)
		*width = columns;
	if(height)
		*height = rows;

	targa_rgba = ri.Malloc(numPixels * 4);
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
