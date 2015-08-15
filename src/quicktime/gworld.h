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

#ifndef _GWORLD_H
#define _GWORLD_H

/** \addtogroup QuickTime */

/** \file gworld.h
 *  \ingroup QuickTime
 *  \brief QuickTime support files.
 *  \author Peter Grayson <jpgrayson@gmail.com>
 *  \author Bill Cholewka <bcholew@gmail.com>
 *  \since 2007
 */
 
#include <QuickTime/QuickTime.h>

struct gworld_info
{
	GWorldPtr gworld;
};

/**
 *  \brief gworld_init
 *  
 *  \param [in] gi           Parameter_Description
 *  \param [in] dim          Parameter_Description
 *  \param [in] pixel_format Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int gworld_init(struct gworld_info * gi, const Rect * dim, OSType pixel_format);

/**
 *  \brief gworld_destroy
 *  
 *  \param [in] gi Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
int gworld_destroy(struct gworld_info * gi);

/**
 *  \brief gworld_get
 *  
 *  \param [in] gi Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
GWorldPtr gworld_get(struct gworld_info * gi);

#endif
