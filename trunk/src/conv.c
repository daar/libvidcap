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

#include "conv.h"

int conv_2vuy_to_i420(int w, int h, const char * s, char * d);
int conv_2vuy_to_yuy2(int w, int h, const char * s, char * d);
int conv_rgb24_to_rgb32(int w, int h, const char * s, char * d);
int conv_yvu9_to_i420(int w, int h, const char * s, char * d);
int conv_bottom_up_rgb24_to_rgb32(int w, int h, const char * s, char * d);

struct conv_info
{
	int src_fourcc;
	int dst_fourcc;
	conv_func func;
	const char *name;
};

static const struct conv_info conv_list[] =
{
	{ VIDCAP_FOURCC_I420,  VIDCAP_FOURCC_RGB32, vidcap_i420_to_rgb32,
		"i420->rgb32" },
	{ VIDCAP_FOURCC_YUY2,  VIDCAP_FOURCC_RGB32, vidcap_yuy2_to_rgb32,
		"yuy2->rgb32" },
	{ VIDCAP_FOURCC_RGB32, VIDCAP_FOURCC_I420,  vidcap_rgb32_to_i420,
		"rgb32->i420" },
	{ VIDCAP_FOURCC_YUY2,  VIDCAP_FOURCC_I420,  vidcap_yuy2_to_i420,
		"yuy2->i420" },
	{ VIDCAP_FOURCC_I420,  VIDCAP_FOURCC_YUY2,  vidcap_i420_to_yuy2,
		"i420->yuy2" },
	{ VIDCAP_FOURCC_RGB32, VIDCAP_FOURCC_YUY2,  vidcap_rgb32_to_yuy2,
		"rgb32->yuy2 (not implemented!)" },

	{ VIDCAP_FOURCC_2VUY,  VIDCAP_FOURCC_YUY2,  conv_2vuy_to_yuy2,
		"2vuy->yuy2" },
	{ VIDCAP_FOURCC_2VUY,  VIDCAP_FOURCC_I420,  conv_2vuy_to_i420,
		"2vuy->i420" },
	{ VIDCAP_FOURCC_RGB24, VIDCAP_FOURCC_RGB32, conv_rgb24_to_rgb32,
		"rgb24->rgb32" },
	{ VIDCAP_FOURCC_BOTTOM_UP_RGB24, VIDCAP_FOURCC_RGB32,
		conv_bottom_up_rgb24_to_rgb32, "bottom-up rgb24->rgb32" },

	{ VIDCAP_FOURCC_YVU9,  VIDCAP_FOURCC_I420,  conv_yvu9_to_i420,
		"yvu9->i420" },
};

static const int conv_list_len = sizeof(conv_list) / sizeof(struct conv_info);

conv_func
conv_conversion_func_get(int src_fourcc, int dst_fourcc)
{
	int i;

	for ( i = 0; i < conv_list_len; ++i )
	{
		const struct conv_info * ci = &conv_list[i];

		if ( src_fourcc == ci->src_fourcc &&
				dst_fourcc == ci->dst_fourcc )
			return ci->func;
	}

	return 0;
}

int
conv_fmt_size_get(int width, int height, int fourcc)
{
	const int pixels = width * height;

	switch ( fourcc )
	{
	case VIDCAP_FOURCC_I420:
		return pixels * 3 / 2;

	case VIDCAP_FOURCC_RGB24:
		return pixels * 3;

	case VIDCAP_FOURCC_RGB32:
		return pixels * 4;

	case VIDCAP_FOURCC_RGB555:
	case VIDCAP_FOURCC_YUY2:
	case VIDCAP_FOURCC_2VUY:
		return pixels * 2;
	default:
		return 0;
	}
}

const char *
conv_conversion_name_get(conv_func function)
{
	int i;

	for ( i = 0; i < conv_list_len; ++i )
	{
		const struct conv_info * ci = &conv_list[i];

		if ( ci->func == function )
			return ci->name;
	}

	return "(ERROR: Conversion name not found)";
}
