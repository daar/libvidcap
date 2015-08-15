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

#ifndef _LOGGING_H
#define _LOGGING_H

/** \file logging.h
 *  \ingroup Core
 *  \brief Message logging functions.
 *  \author Peter Grayson <jpgrayson@gmail.com>
 *  \author Bill Cholewka <bcholew@gmail.com>
 *  \since 2007
 */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum log_level
{
	log_level_none  = 0,
	log_level_error = 1,
	log_level_warn  = 2,
	log_level_info  = 3,
	log_level_debug = 4
};

/**
 *  \brief log_file_set
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_file_set(FILE *);

/**
 *  \brief log_level_set
 *  
 *  \param [in] log_level Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_level_set(enum log_level);

#ifdef __GNUC__
/**
 *  \brief log_error
 *  
 *  \param [in] fmt Parameter_Description
 *  \param [in] ... Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_error(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));

/**
 *  \brief log_warn
 *  
 *  \param [in] fmt Parameter_Description
 *  \param [in] ... Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_warn(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));

/**
 *  \brief log_info
 *  
 *  \param [in] fmt Parameter_Description
 *  \param [in] ... Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_info(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));

/**
 *  \brief log_debug
 *  
 *  \param [in] fmt Parameter_Description
 *  \param [in] ... Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_debug(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));
#else

/**
 *  \brief log_error
 *  
 *  \param [in] fmt Parameter_Description
 *  \param [in] ... Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_error(const char * fmt, ...);

/**
 *  \brief log_warn
 *  
 *  \param [in] fmt Parameter_Description
 *  \param [in] ... Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_warn(const char * fmt, ...);

/**
 *  \brief log_info
 *  
 *  \param [in] fmt Parameter_Description
 *  \param [in] ... Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_info(const char * fmt, ...);

/**
 *  \brief log_debug
 *  
 *  \param [in] fmt Parameter_Description
 *  \param [in] ... Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_debug(const char * fmt, ...);
#endif

/**
 *  \brief log_oom
 *  
 *  \param [in] file Parameter_Description
 *  \param [in] line Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
void log_oom(const char * file, int line);

#ifdef __cplusplus
}
#endif

#endif
