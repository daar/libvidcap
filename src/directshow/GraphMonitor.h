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

#ifndef _GRAPHMONITOR_H_
#define _GRAPHMONITOR_H_

#include <windows.h>
#include <DShow.h>
#include <vector>
#include "LocklessQueue.h"

class GraphMonitor
{

public:
	typedef bool (*cancelCallbackFunc)(IMediaEventEx *, void *);

	GraphMonitor(cancelCallbackFunc, void *);
	~GraphMonitor();
	void addGraph(IMediaEventEx *);
	void removeGraph(IMediaEventEx *);

private:
	static DWORD WINAPI monitorGraphs(LPVOID lpParam);
	void processListEvent();
	void processGraphEvent(IMediaEventEx *);
	int findContext(IMediaEventEx *);

private:
	void * parentContext_;
	cancelCallbackFunc cancelCBFunc_;
	HANDLE initDoneEvent_;
	HANDLE listEvent_;
	HANDLE terminateEvent_;

	struct graphContext
	{
		IMediaEventEx      * pME;
		HANDLE             * waitHandle;
	};

	// used to generate array of handles on which to wait
	std::vector<graphContext *> graphContextList_;

	struct graphListEvent
	{
		enum graphListEventType { add = 0, remove } eventType;
		IMediaEventEx * graph;
	};

	// enqueued by main thread
	// dequeued by graph monitor thread
	LocklessQueue<graphListEvent> graphListEventQueue_;

	void * graphMonitorThread_;
	DWORD graphMonitorThreadID_;
};

#endif
