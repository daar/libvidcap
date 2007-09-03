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

int
vidcap_rgb32_to_i420(int width, int height,
		const char * src,
		char * dst, int dest_size)
{
	log_error("vidcap_rgb32_to_i420() not implemented\n");
	return -1;
}

int
vidcap_yuy2_to_i420(int width, int height,
		const char * src,
		char * dst, int dest_size)
{
	/* convert from a packed structure to a planar structure */
	char * dst_y_even = dst;
	char * dst_y_odd = dst + width;
	char * dst_u = dst + width * height;
	char * dst_v = dst_u + width * height / 4;
	const char * src_even = src;
	const char * src_odd = src + width * 2;

	int i, j;

	if ( dest_size < width * height * 4 / 3 )
		return -1;

	/* yuy2 has a vertical sampling period (for u and v)
	 * half that for i420. Will toss half of the
	 * U and V data during repackaging.
	 */
	for ( i = 0; i < height / 2; ++i )
	{
		for ( j = 0; j < width / 2; ++j )
		{
			*dst_y_even++ = *src_even++;
			*dst_u++      = *src_even++;
			*dst_y_even++ = *src_even++;
			*dst_v++      = *src_even++;

			*dst_y_odd++  = *src_odd++;
			src_odd++;
			*dst_y_odd++  = *src_odd++;
			src_odd++;
		}

		dst_y_even += width;
		dst_y_odd += width;
		src_even += width * 2;
		src_odd += width * 2;
	}

	return 0;
}

/* 2vuy is a byte reversal of yuy2, so the conversion is the
 * same except where we find the yuv components.
 */
int
conv_2vuy_to_i420(int width, int height,
		const char * src,
		char * dst, int dest_size)
{
	char * dst_y_even = dst;
	char * dst_y_odd = dst + width;
	char * dst_u = dst + width * height;
	char * dst_v = dst_u + width * height / 4;
	const char * src_even = src;
	const char * src_odd = src + width * 2;

	int i, j;

	if ( dest_size < width * height * 4 / 3 )
		return -1;

	for ( i = 0; i < height / 2; ++i )
	{
		for ( j = 0; j < width / 2; ++j )
		{
			*dst_u++      = *src_even++;
			*dst_y_even++ = *src_even++;
			*dst_v++      = *src_even++;
			*dst_y_even++ = *src_even++;

			src_odd++;
			*dst_y_odd++  = *src_odd++;
			src_odd++;
			*dst_y_odd++  = *src_odd++;
		}

		dst_y_even += width;
		dst_y_odd += width;
		src_even += width * 2;
		src_odd += width * 2;
	}

	return 0;
}

