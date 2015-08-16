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

#ifndef _CONVERTERS_H
#define _CONVERTERS_H

/** \file converters.h
 *  \ingroup API
 *  \brief Public vidcap converters API functions.
 *  \author Peter Grayson <jpgrayson@gmail.com>
 *  \author Bill Cholewka <bcholew@gmail.com>
 *  \since 2007
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \brief Converts i420 to rgb32
 *  
 *  \param [in] width  Width of the source image
 *  \param [in] height Height of the source image
 *  \param [in] src    Source image
 *  \param [in] dest   Destination image
 *  \return Returns 0 on success
 *  
 *  \details Details
 */
int
vidcap_i420_to_rgb32(int width, int height, const char * src, char * dest);

/**
 *  \brief Converts i420 to yuy2
 *  
 *  \param [in] width  Width of the source image
 *  \param [in] height Height of the source image
 *  \param [in] src    Source image
 *  \param [in] dest   Destination image
 *  \return Returns 0 on success
 *  
 *  \details Details
 */
int
vidcap_i420_to_yuy2(int width, int height, const char * src, char * dest);

/**
 *  \brief Converts yuy2 to i420
 *  
 *  \param [in] width  Width of the source image
 *  \param [in] height Height of the source image
 *  \param [in] src    Source image
 *  \param [in] dest   Destination image
 *  \return Returns 0 on success
 *  
 *  \details Details
 */
int
vidcap_yuy2_to_i420(int width, int height, const char * src, char * dest);

/**
 *  \brief Converts yuy2 to rgb32
 *  
 *  \param [in] width  Width of the source image
 *  \param [in] height Height of the source image
 *  \param [in] src    Source image
 *  \param [in] dest   Destination image
 *  \return Returns 0 on success
 *  
 *  \details Details
 */
int
vidcap_yuy2_to_rgb32(int width, int height, const char * src, char * dest);

/**
 *  \brief Converts rgb32 to i420
 *  
 *  \param [in] width  Width of the source image
 *  \param [in] height Height of the source image
 *  \param [in] src    Source image
 *  \param [in] dest   Destination image
 *  \return Returns 0 on success
 *  
 *  \details Details
 */
int
vidcap_rgb32_to_i420(int width, int height, const char * src, char * dest);

/**
 *  \brief Converts rgb32 to yuy2
 *  
 *  \param [in] width  Width of the source image
 *  \param [in] height Height of the source image
 *  \param [in] src    Source image
 *  \param [in] dest   Destination image
 *  \return Returns 0 on success
 *  
 *  \details Details
 */
int
vidcap_rgb32_to_yuy2(int width, int height, const char * src, char * dest);

#ifdef __cplusplus
}
#endif

#endif
