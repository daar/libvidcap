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

#ifndef _SG_MANAGER_H
#define _SG_MANAGER_H

/** \file sg_manager.h
 *  \ingroup QuickTime
 *  \brief QuickTime support files.
 *  \author Peter Grayson <jpgrayson@gmail.com>
 *  \author Bill Cholewka <bcholew@gmail.com>
 *  \since 2007
 */
 
#include <QuickTime/QuickTime.h>

#include "sg_source.h"
#include "vidcap/vidcap.h"

struct sg_manager
{
	int source_list_len;
	struct sg_source * source_list;
};

/**
 *  \brief sg_manager_init
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
int sg_manager_init(struct sg_manager *);

/**
 *  \brief sg_manager_destroy
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
int sg_manager_destroy(struct sg_manager *);

/**
 *  \brief sg_manager_update
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
int sg_manager_update(struct sg_manager *);

/**
 *  \brief sg_manager_source_count
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
int sg_manager_source_count(struct sg_manager *);

/**
 *  \brief sg_manager_source_get
 *  
 *  \param [in] index Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
struct sg_source *
sg_manager_source_get(struct sg_manager *, int index);

/**
 *  \brief sg_manager_source_find
 *  
 *  \param [in] device_id Parameter_Description
 *  \param [in] input_id  Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 */
struct sg_source *
sg_manager_source_find(struct sg_manager *, int device_id, int input_id);

/**
 *  \brief sg_manager_source_default_find
 *  
 *  \return Return_Description
 *  
 *  \details Details
 */
struct sg_source *
sg_manager_source_default_find(struct sg_manager *);

#endif
