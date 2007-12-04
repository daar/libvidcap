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

#include "double_buffer.h"

#include "logging.h"

/* The write thread inserts (and frees) objects.
 * The read thread reads (copies) objects
 *
 * It is the responsibility of the read thread's 'application' to
 * free the copied objects.
 *
 * If the read thread 'application' blocks for too long (doesn't 
 * read often enough), the write thread will free a buffered object
 * to make room for an incoming object
 */

/* NOTE: This function is passed function pointers to call when
 *       necessary to free or copy an object
 */
struct double_buffer *
double_buffer_create( void (*free_me)(void *), void * (*copy_me)(void *) )
{

	struct double_buffer * db_buff;
		
	if ( !free_me || !copy_me )
		return 0;

	db_buff = calloc(1, sizeof(*db_buff));

	if ( !db_buff )
	{
		log_oom(__FILE__, __LINE__);
		return 0;
	}

	db_buff->read_count = 0;
	db_buff->write_count = 0;
	db_buff->count[0] = -1;
	db_buff->count[1] = -1;
	db_buff->objects[0] = 0;
	db_buff->objects[1] = 0;

	db_buff->free_object = free_me;
	db_buff->copy_object = copy_me;

	vc_mutex_init(&db_buff->locks[0]);
	vc_mutex_init(&db_buff->locks[1]);

	/* TODO: remove this (debug) counter */
	db_buff->num_insert_too_far_failures       = 0;

	return db_buff;
}

void
double_buffer_destroy(struct double_buffer * db_buff)
{
	vc_mutex_destroy(&db_buff->locks[1]);
	vc_mutex_destroy(&db_buff->locks[0]);

	if ( db_buff->write_count > 0 )
			db_buff->free_object(db_buff->objects[0]);

	if ( db_buff->write_count > 1 )
		db_buff->free_object(db_buff->objects[1]);

	free(db_buff);
}

void
double_buffer_insert(struct double_buffer * db_buff, void * new_object)
{
	const int insertion_index = db_buff->write_count % 2;

	/* TODO: we could eliminate a copy by having the reader free
	 *       all buffered objects. This would require that a failed
	 *       check below result in the object being freed. This
	 *       would ensure that the reader sees all BUFFERED objects.
	 *
	 *       The reader would then need to take care to free objects
	 *       as they become outdated.
	 *
	 *       The tradeoff is the occasional dropped object when
	 *       the writer gets ahead a little. The counter can help
	 *       to evaluate the cost of this tradeoff
	 */
	/* don't get far ahead of the reader*/
	if ( db_buff->write_count > ( db_buff->read_count + 2 ) )
	{
		db_buff->num_insert_too_far_failures++;
		/* return; */
	}

	/* get exclusive access to the correct buffer */
	if ( vc_mutex_trylock(&db_buff->locks[insertion_index] ))
	{
		/* failed to obtain lock */
		/* drop incoming object  */
		log_info("vidcap callback failed to write a frame\n");
		db_buff->free_object(new_object);
		return;
	}

	/* free the slot object if something is already there */
	if ( db_buff->write_count > 1 )
		db_buff->free_object(db_buff->objects[insertion_index]);

	/* insert object */
	db_buff->objects[insertion_index] = new_object;

	/* stamp the buffer with the write count */
	db_buff->count[insertion_index] = db_buff->write_count;

	/* advance the write count */
	++db_buff->write_count;

	/* unlock the correct lock */
	vc_mutex_unlock(&db_buff->locks[insertion_index]);
}

void *
double_buffer_read(struct double_buffer * db_buff)
{
	int copy_index = db_buff->read_count % 2;
	void * buff;

	if ( db_buff->write_count < 1 )
		return 0;

	/* try the next buffer */
	if ( db_buff->count[copy_index] < db_buff->read_count )
	{
		/* Next buffer isn't ready. Use the previous buffer */
		copy_index = 1 - copy_index;
	}


	if ( vc_mutex_trylock(&db_buff->locks[copy_index] ) )
	{
		/* Failed to obtain lock.
		 * Try the other slot?
		 */
		if ( db_buff->write_count < 2 )
			return 0;

		copy_index = 1 - copy_index;

		if ( db_buff->count[copy_index] < db_buff->read_count )
		{
			/* Too old. Don't read this buffer */
			vc_mutex_unlock(&db_buff->locks[copy_index]);
			return 0;
		}

		/* Try other lock. Failure should be rare */
		if ( vc_mutex_trylock(&db_buff->locks[copy_index] ) )
		{
			log_info("Capture timer thread failed to obtain 2nd lock\n");
			return 0;
		}
	}

	/* Is this buffer older than the last-read? */
	if ( db_buff->count[copy_index] < (db_buff->read_count - 1))
	{
		/* vidcap buffer is too old. Don't read it.
		 * This needs to be rare.
		 */
		vc_mutex_unlock(&db_buff->locks[copy_index]);
		return 0;
	}

	buff = db_buff->copy_object(db_buff->objects[copy_index]);

	db_buff->read_count = db_buff->count[copy_index] + 1;

	vc_mutex_unlock(&db_buff->locks[copy_index]);

	return buff;
}

int
double_buffer_count(struct double_buffer * db_buff)
{
	return db_buff->write_count;
}
