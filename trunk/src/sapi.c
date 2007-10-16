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

#include <stdlib.h>
#include <string.h>

#include "hotlist.h"
#include "logging.h"
#include "sapi.h"

#ifndef HAVE_GETTIMEOFDAY
static __inline int gettimeofday(struct timeval * tv, struct timezone * tz)
#ifdef WIN32
{
	FILETIME ft;
	LARGE_INTEGER li;
	__int64 t;
	static int tzflag;
	const __int64 EPOCHFILETIME = 116444736000000000i64;
	if ( tv )
	{
		GetSystemTimeAsFileTime(&ft);
		li.LowPart  = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		t  = li.QuadPart;       /* In 100-nanosecond intervals */
		t -= EPOCHFILETIME;     /* Offset to the Epoch time */
		t /= 10;                /* In microseconds */
		tv->tv_sec  = (long)(t / 1000000);
		tv->tv_usec = (long)(t % 1000000);
	}

	return 0;

#else /* !defined(_WINDOWS) */
	errno = ENOSYS;
	return -1;
#endif
}
#endif

static __inline int
tv_greater_or_equal(struct timeval * t1, struct timeval * t0)
{
	if ( t1->tv_sec > t0->tv_sec)
		return 1;

	if ( t1->tv_sec < t0->tv_sec)
		return 0;

	if ( t1->tv_usec >= t0->tv_usec )
		return 1;

	return 0;
}

static __inline void
tv_add_usecs(struct timeval *t1, struct timeval *t0, int usecs)
{
	int secs_carried = 0;

	t1->tv_usec = t0->tv_usec + usecs;
	secs_carried = t1->tv_usec / 1000000;
	t1->tv_usec %= 1000000;

	t1->tv_sec = t0->tv_sec + secs_carried;
}

static int
enforce_framerate(struct sapi_src_context * src_ctx)
{
	struct timeval  tv_earliest_next;
	struct timeval  tv_now;
	struct timeval *tv_oldest = 0;
	int first_time = 0;

	if ( !src_ctx->frame_times )
		return 0;

	first_time = !sliding_window_count(src_ctx->frame_times);

	if ( gettimeofday(&tv_now, 0) )
		return -1;

	if ( !first_time && !tv_greater_or_equal(
				&tv_now, &src_ctx->frame_time_next) )
		return 0;

	/* Allow frame through */

	tv_oldest = sliding_window_peek_front(src_ctx->frame_times);

	if ( !tv_oldest )
		tv_oldest = &tv_now;

	/* calculate next frame time based on sliding window */
	tv_add_usecs(&src_ctx->frame_time_next, tv_oldest,
			sliding_window_count(src_ctx->frame_times) *
			1000000 *
			src_ctx->fmt_nominal.fps_denominator /
			src_ctx->fmt_nominal.fps_numerator);

	/* calculate the earliest allowable next frame time
	 * based on: now + arbitrary minimum spacing
	 */
	tv_add_usecs(&tv_earliest_next, &tv_now, 900000 *
			src_ctx->fmt_nominal.fps_denominator /
			src_ctx->fmt_nominal.fps_numerator);

	/* if earliest allowable > next, next = earliest */
	/* if there's been a stall, pace ourselves       */
	if ( tv_greater_or_equal(&tv_earliest_next,
				&src_ctx->frame_time_next) )
		src_ctx->frame_time_next = tv_earliest_next;

	sliding_window_slide(src_ctx->frame_times, &tv_now);

	return 1;
}

int
sapi_acquire(struct sapi_context * sapi_ctx)
{
	if ( sapi_ctx->ref_count )
		return -1;

	sapi_ctx->ref_count++;
	return 0;
}

int
sapi_release(struct sapi_context * sapi_ctx)
{
	if ( !sapi_ctx->ref_count )
		return -1;

	sapi_ctx->ref_count--;
	return 0;
}

int
sapi_src_format_list_build(struct sapi_src_context * src_ctx)
{
	struct vidcap_fmt_info fmt_info;
	struct vidcap_fmt_info * list = 0;
	int list_len = 0;
	int i, j, k;

	if ( src_ctx->fmt_list )
	{
		log_error("source alread has format list\n");
		return -1;
	}

	for ( i = 0; i < hot_fourcc_list_len; ++i )
	{
		fmt_info.fourcc = hot_fourcc_list[i];

		for ( j = 0; j < hot_fps_list_len; ++j )
		{
			fmt_info.fps_numerator =
				hot_fps_list[j].fps_numerator;
			fmt_info.fps_denominator =
				hot_fps_list[j].fps_denominator;

			for ( k = 0; k < hot_resolution_list_len; ++k )
			{
				struct vidcap_fmt_info fmt_native;

				fmt_info.width = hot_resolution_list[k].width;
				fmt_info.height = hot_resolution_list[k].height;

				if ( !src_ctx->format_validate(src_ctx,
							&fmt_info,
							&fmt_native) )
					continue;

				list = realloc(list,
						++list_len * sizeof(fmt_info));

				if ( !list )
				{
					log_oom(__FILE__, __LINE__);
					return -1;
				}

				list[list_len-1] = fmt_info;
			}
		}
	}

	src_ctx->fmt_list = list;
	src_ctx->fmt_list_len = list_len;

	return 0;
}

/* NOTE: stride-ignorant sapis should pass a stride of zero */
int
sapi_src_capture_notify(struct sapi_src_context * src_ctx,
		const char * video_data, int video_data_size,
		int stride,
		int error_status)
{
	struct vidcap_capture_info cap_info;

	/* NOTE: We may be called here by the capture thread while the
	 * main thread is clearing capture_data and capture_callback
	 * from within vidcap_src_capture_stop().
	 */
	vidcap_src_capture_callback cap_callback = src_ctx->capture_callback;
	void * cap_data = src_ctx->capture_data;
	void * buf = 0;
	int buf_data_size = 0;

	int send_frame = 0;

	if ( !error_status )
		send_frame = enforce_framerate(src_ctx);

	if ( send_frame < 0 )
		error_status = -1000;

	cap_info.error_status = error_status;

	if ( !cap_info.error_status && stride &&
			!destridify(src_ctx->fmt_native.width,
				src_ctx->fmt_native.height,
				src_ctx->fmt_native.fourcc,
				stride,
				video_data,
				src_ctx->stride_free_buf) )
	{
		buf = src_ctx->stride_free_buf;
		buf_data_size = src_ctx->stride_free_buf_size;
	}
	else
	{
		buf = (void *)video_data;
		buf_data_size = video_data_size;
	}

	if ( cap_info.error_status )
	{
		cap_info.video_data = 0;
		cap_info.video_data_size = 0;
	}
	else if ( src_ctx->fmt_conv_func )
	{
		if ( src_ctx->fmt_conv_func(
					src_ctx->fmt_native.width,
					src_ctx->fmt_native.height,
					buf,
					src_ctx->fmt_conv_buf) )
		{
			log_error("failed format conversion\n");
			cap_info.error_status = -1;
		}

		cap_info.video_data = src_ctx->fmt_conv_buf;
		cap_info.video_data_size = src_ctx->fmt_conv_buf_size;
	}
	else
	{
		cap_info.video_data = buf;
		cap_info.video_data_size = buf_data_size;
	}

	if ( ( send_frame || error_status ) && cap_callback && cap_data != VIDCAP_INVALID_USER_DATA )
	{
		cap_callback(src_ctx, cap_data, &cap_info);

		if ( cap_info.error_status )
		{
			src_ctx->src_state = src_bound;
			src_ctx->capture_callback = 0;
			src_ctx->capture_data = VIDCAP_INVALID_USER_DATA;
		}
	}

	return 0;
}

int
sapi_can_convert_native_to_nominal(const struct vidcap_fmt_info * fmt_native,
		const struct vidcap_fmt_info * fmt_nominal)
{
	const float native_fps =
		(float)fmt_native->fps_numerator /
		(float)fmt_native->fps_denominator;

	const float nominal_fps =
		(float)fmt_nominal->fps_numerator /
		(float)fmt_nominal->fps_denominator;

	if ( native_fps < nominal_fps )
		return 0;

	if ( fmt_native->width != fmt_nominal->width ||
			fmt_native->height != fmt_nominal->height )
		return 0;

	if ( fmt_native->fourcc == fmt_nominal->fourcc )
		return 1;

	if ( conv_conversion_func_get(fmt_native->fourcc, fmt_nominal->fourcc) )
		return 1;

	return 0;
}
