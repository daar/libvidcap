/*
 * libvidcap - a cross-platform video capture library
 *
 * Copyright 2007 Wimba, Inc.
 *
 * Contributors:
 * Peter Grayson <jpgrayson@gmail.com>
 * Bill Cholewka <bcholew@gmail.com>
 *
 * libvidcap is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * libvidcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include <vidcap/converters.h>
#include "logging.h"

/* NOTE: size of dest buffer must be >= width * height * 2 */

int
vidcap_rgb32_to_yuy2(int width, int height, const char * src, char * dest)
{
	log_error("vidcap_rgb32_to_yuy2() not implemented\n");
	return -1;
}

int
vidcap_i420_to_yuy2(int width, int height, const char * src, char * dest)
{
	log_error("vidcap_i420_to_yuy2() not implemented\n");
	return -1;
}

int
conv_2vuy_to_yuy2(int width, int height, const char * src, char * dest)
{
	int i;
	unsigned int * d = (unsigned int *)dest;
	const unsigned int * s = (const unsigned int *)src;

	for ( i = 0; i < width * height / 2; ++i )
	{
		*d++ = ((*s & 0xff000000) >> 8) |
			((*s & 0x00ff0000) << 8) |
			((*s & 0x0000ff00) >> 8) |
			((*s & 0x000000ff) << 8);
		++s;
	}

	return 0;
}
