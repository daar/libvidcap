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

#ifndef _SLIDING_WINDOW_H
#define _SLIDING_WINDOW_H

/** \file sliding_window.h
 *  \ingroup Core
 *  \brief Core library functions.
 *  \author Peter Grayson <jpgrayson@gmail.com>
 *  \author Bill Cholewka <bcholew@gmail.com>
 *  \since 2007
 */

struct sliding_window
{
	int window_len;
	int object_size;
	char * objects;
	int count;
	int head;
	int tail;
};

/**
 *  \brief sliding_window_create
 *  
 *  \param [in] window_len  Parameter_Description
 *  \param [in] object_size Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
struct sliding_window *
sliding_window_create(int window_len, int object_size);

/**
 *  \brief sliding_window_count
 *  
 *  \param [in] swin Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
int
sliding_window_count(struct sliding_window * swin);

/**
 *  \brief sliding_window_destroy
 *  
 *  \param [in] swin Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
void
sliding_window_destroy(struct sliding_window * swin);

/**
 *  \brief sliding_window_slide
 *  
 *  \param [in] swin   Parameter_Description
 *  \param [in] object Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
void
sliding_window_slide(struct sliding_window * swin, void * object);

/**
 *  \brief sliding_window_peek_front
 *  
 *  \param [in] swin Parameter_Description
 *  \return Return_Description
 *  
 *  \details Details
 *  
 */
void *
sliding_window_peek_front(struct sliding_window * swin);

#endif
