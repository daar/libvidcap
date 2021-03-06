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

#ifndef _VIDCAP_H
#define _VIDCAP_H

/** \addtogroup API */

/** \file vidcap.h
 *  \ingroup API
 *  \brief Public vidcap API functions.
 *  \author Peter Grayson <jpgrayson@gmail.com>
 *  \author Bill Cholewka <bcholew@gmail.com>
 *  \since 2007
 */

#ifdef __cplusplus
extern "C" {
#endif

#define VIDCAP_NAME_LENGTH 256

/** Fourcc values available from vidcap */
enum vidcap_fourccs {
	VIDCAP_FOURCC_I420   = 100,
	VIDCAP_FOURCC_YUY2   = 101,
	VIDCAP_FOURCC_RGB32  = 102,
};

/** The different log levels that vidcap supports */
enum vidcap_log_level {
	VIDCAP_LOG_NONE  = 0,
	VIDCAP_LOG_ERROR = 10,
	VIDCAP_LOG_WARN  = 20,
	VIDCAP_LOG_INFO  = 30,
	VIDCAP_LOG_DEBUG = 40
};

typedef void vidcap_state;
typedef void vidcap_sapi;
typedef void vidcap_src;

struct vidcap_sapi_info
{
	char identifier[VIDCAP_NAME_LENGTH];
	char description[VIDCAP_NAME_LENGTH];
};

struct vidcap_src_info
{
	char identifier[VIDCAP_NAME_LENGTH];
	char description[VIDCAP_NAME_LENGTH];
};

struct vidcap_fmt_info
{
	int width;
	int height;
	int fourcc;
	int fps_numerator;
	int fps_denominator;
};

struct vidcap_capture_info
{
	const char * video_data;
	int video_data_size;
	int error_status;
	long capture_time_sec;
	long capture_time_usec;
	struct vidcap_fmt_info format;
};

typedef int (*vidcap_src_capture_callback) (vidcap_src *,
				void * user_data,
				struct vidcap_capture_info * cap_info);

typedef int (*vidcap_sapi_notify_callback) (vidcap_sapi *, void * user_data);

/**
 *  \brief vidcap_initialize
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
vidcap_state *
vidcap_initialize(void);

/**
 *  \brief vidcap_destroy
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
void
vidcap_destroy(vidcap_state *);

/**
 *  \brief vidcap_log_level_set
 *  
 *  \param [in] level Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_log_level_set(enum vidcap_log_level level);

/**
 *  \brief vidcap_sapi_enumerate
 *  
 *  \param [in] index Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_sapi_enumerate(vidcap_state *,
		int index,
		struct vidcap_sapi_info *);

/**
 *  \brief vidcap_sapi_acquire
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
vidcap_sapi *
vidcap_sapi_acquire(vidcap_state *,
		const struct vidcap_sapi_info *);

/**
 *  \brief vidcap_sapi_release
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_sapi_release(vidcap_sapi *);

/**
 *  \brief vidcap_sapi_info_get
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_sapi_info_get(vidcap_sapi *,
		struct vidcap_sapi_info *);

/**
 *  \brief vidcap_srcs_notify
 *  
 *  \param [in] callback  Parameter_Description
 *  \param [in] user_data Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_srcs_notify(vidcap_sapi *,
		vidcap_sapi_notify_callback callback,
		void * user_data);

/**
 *  \brief vidcap_src_list_update
 *  
 *  \param [in] sapi Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_src_list_update(vidcap_sapi * sapi);

/**
 *  \brief vidcap_src_list_get
 *  
 *  \param [in] sapi     Parameter_Description
 *  \param [in] list_len Parameter_Description
 *  \param [in] src_list Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_src_list_get(vidcap_sapi * sapi,
		int list_len,
		struct vidcap_src_info * src_list);

/**
 *  \brief vidcap_src_acquire
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
vidcap_src *
vidcap_src_acquire(vidcap_sapi *,
		const struct vidcap_src_info *);

/**
 *  \brief vidcap_src_release
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_src_release(vidcap_src *);

/**
 *  \brief vidcap_src_info_get
 *  
 *  \param [in] src      Parameter_Description
 *  \param [in] src_info Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_src_info_get(vidcap_src * src,
		struct vidcap_src_info * src_info);

/**
 *  \brief vidcap_format_enumerate
 *  
 *  \param [in] index Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_format_enumerate(vidcap_src *,
		int index,
		struct vidcap_fmt_info *);

/**
 *  \brief vidcap_format_bind
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_format_bind(vidcap_src *,
		const struct vidcap_fmt_info *);

/**
 *  \brief vidcap_format_info_get
 *  
 *  \param [in] src      Parameter_Description
 *  \param [in] fmt_info Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_format_info_get(vidcap_src * src,
		struct vidcap_fmt_info * fmt_info);

/**
 *  \brief vidcap_src_capture_start
 *  
 *  \param [in] callback  Parameter_Description
 *  \param [in] user_data Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_src_capture_start(vidcap_src *,
		vidcap_src_capture_callback callback,
		void * user_data);

/**
 *  \brief vidcap_src_capture_stop
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
int
vidcap_src_capture_stop(vidcap_src *);

/**
 *  \brief vidcap_fourcc_string_get
 *  
 *  \param [in] fourcc Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
const char *
vidcap_fourcc_string_get(int fourcc);

#ifdef __cplusplus
}
#endif

#endif
