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

#ifndef _SAPI_CONTEXT_H
#define _SAPI_CONTEXT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <windows.h>
#endif

#if defined(HAVE_NANOSLEEP) || defined(HAVE_GETTIMEOFDAY)
#include <sys/time.h>
#include <time.h>
#endif

#include <vidcap/vidcap.h>
#include "sliding_window.h"

#include "conv.h"

struct sapi_context;

struct sapi_src_context
{
	int (*release)(struct sapi_src_context *);

	int (*format_validate)(struct sapi_src_context *,
			const struct vidcap_fmt_info * fmt_nominal,
			struct vidcap_fmt_info * fmt_native);

	int (*format_bind)(struct sapi_src_context *,
			const struct vidcap_fmt_info *);

	int (*start_capture)(struct sapi_src_context *);

	int (*stop_capture)(struct sapi_src_context *);

	struct vidcap_src_info src_info;
	struct sliding_window * frame_times;
	struct timeval frame_time_next;

	struct vidcap_fmt_info fmt_nominal;
	struct vidcap_fmt_info fmt_native;
	conv_func fmt_conv_func;
	char * fmt_conv_buf;
	int fmt_conv_buf_size;

	struct vidcap_fmt_info * fmt_list;
	int fmt_list_len;

	enum state_enum { src_acquired, src_bound, src_capturing } src_state;

	vidcap_src_capture_callback capture_callback;
	void * capture_data;

	void * priv;
};

struct sapi_src_list
{
	struct vidcap_src_info * list;
	int len;
};

struct sapi_context
{
	int (*acquire)(struct sapi_context *);

	int (*release)(struct sapi_context *);

	void (*destroy)(struct sapi_context *);

	int (*monitor_sources)(struct sapi_context *);

	int (*scan_sources)(struct sapi_context *,
			struct sapi_src_list *);

	int (*acquire_source)(struct sapi_context *,
			struct sapi_src_context * src_ctx,
			const struct vidcap_src_info * src_info);

	vidcap_sapi_notify_callback notify_callback;
	void * notify_data;

	const char * identifier;
	const char * description;
	void * priv;
	int ref_count;
	struct sapi_src_list user_src_list;
};

int
sapi_src_fps_info_init(struct sapi_src_context *);

void
sapi_src_fps_info_clean(struct sapi_src_context *);

#endif
