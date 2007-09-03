//
// libvidcap - a cross-platform video capture library
//
// Copyright 2007 Wimba, Inc.
//
// Contributors:
// Peter Grayson <jpgrayson@gmail.com>
// Bill Cholewka <bcholew@gmail.com>
//
// libvidcap is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of
// the License, or (at your option) any later version.
//
// libvidcap is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this program.  If not, see
// <http://www.gnu.org/licenses/>.
//

#ifndef __LOCKLESSQUEUE_H__
#define __LOCKLESSQUEUE_H__

template<typename T>
class LocklessQueue
{
public:
	LocklessQueue(int size = 512)
		: max_(size),
		  head_(0),
		  tail_(0)
	{
		pData_ = new const T * [max_];
	};

	~LocklessQueue()
	{
		delete[] pData_;
	};

	bool enqueue(const T *buffer)
	{
		// take note of future head value
		unsigned int nextHead = (head_ + 1) % max_;

		// safe to push?
		if ( nextHead == tail_ )
			return false;

		// push the data
		pData_[head_] = buffer;

		// NOW advance the head
		head_ = nextHead;

		return true;
	};

	bool dequeue(const T **pBuffer)
	{
		if ( head_ == tail_ )
			return false;

		// take note of future tail value
		unsigned int next = (tail_ + 1) % max_;

		// get data
		*pBuffer = pData_[tail_];

		// NOW advance the tail
		tail_ = next;

		return true;
	};

private:
	const int max_;

	volatile unsigned int head_;
	volatile unsigned int tail_;

	const T ** pData_;
};

#endif
