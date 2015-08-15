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

#ifndef _SAPI_H
#define _SAPI_H

/** \file sapi.h
 *  \ingroup Core
 *  \brief Core library functions.
 *  \author Peter Grayson <jpgrayson@gmail.com>
 *  \author Bill Cholewka <bcholew@gmail.com>
 *  \since 2007
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define VIDCAP_INVALID_USER_DATA	((void *)-1)

#include "sapi_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \brief sapi_acquire
 *  
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
int
sapi_acquire(struct sapi_context *);

/**
 *  \brief sapi_release
 *  
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
int
sapi_release(struct sapi_context *);

/**
 *  \brief sapi_src_capture_notify
 *  
 *  \param [in] src_ctx         Parameter_Description
 *  \param [in] video_data      Parameter_Description
 *  \param [in] video_data_size Parameter_Description
 *  \param [in] stride          Parameter_Description
 *  \param [in] error_status    Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
int
sapi_src_capture_notify(struct sapi_src_context * src_ctx,
		char * video_data, int video_data_size,
		int stride,
		int error_status);

/**
 *  \brief sapi_src_format_list_build
 *  
 *  \param [in] src_ctx Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
int
sapi_src_format_list_build(struct sapi_src_context * src_ctx);

/**
 *  \brief sapi_can_convert_native_to_nominal
 *  
 *  \param [in] fmt_native  Parameter_Description
 *  \param [in] fmt_nominal Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
int
sapi_can_convert_native_to_nominal(const struct vidcap_fmt_info * fmt_native,
		const struct vidcap_fmt_info * fmt_nominal);

#ifdef __cplusplus
}
#endif

#endif
