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

#ifndef _CONV_H
#define _CONV_H

#include <vidcap/vidcap.h>
#include <vidcap/converters.h>

enum vidcap_fourccs_extra
{
	VIDCAP_FOURCC_RGB24  = ' r24',
	VIDCAP_FOURCC_RGB555 = 'r555',
	VIDCAP_FOURCC_YVU9   = 'yvu9',
	VIDCAP_FOURCC_2VUY   = '2vuy',
};

typedef int (*conv_func)(int width, int height,
		const char * src, char * dst, int dst_size);

conv_func
conv_conversion_func_get(int src_fourcc, int dst_fourcc);

int
conv_fmt_size_get(int width, int height, int fourcc);

#endif

