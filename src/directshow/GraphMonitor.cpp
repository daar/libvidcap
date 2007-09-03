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

// This thread will monitor a DirectShow filter graph for a capture device.
// If the device is removed or encounters an error, the capture callback will
// be called with a special status code, indicating it's the last callback
#include <sstream>
#include "GraphMonitor.h"
#include "logging.h"

GraphMonitor::GraphMonitor(cancelCallbackFunc cb, void * parentCtx)
		: cancelCBFunc_(cb),
		  parentContext_(parentCtx),
		  graphMonitorThread_(0),
		  graphMonitorThreadID_(0)
{
	// create an event used to wake up thread
	// when graphs must be added or removed
	// FIXME: how does one free these events?
	listEvent_ = CreateEvent( 
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		); 

	if ( listEvent_ == NULL) 
	{ 
		log_error("CreateEvent failed (%d)\n", GetLastError());
		throw std::runtime_error("GraphMonitor: failed creating list event");
	}

	// create an event used to wake up thread
	// when thread must terminate
	terminateEvent_ = CreateEvent( 
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		); 

	if ( terminateEvent_ == NULL)
	{ 
		log_error("CreateEvent failed (%d)\n", GetLastError());
		throw std::runtime_error("GraphMonitor: failed creating event");
	}

	// create an event used to signal that the thread has been created
	initDoneEvent_ = CreateEvent( 
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		); 

	if ( initDoneEvent_ == NULL) 
	{ 
		log_error("CreateEvent failed (%d)\n", GetLastError());
		throw std::runtime_error("GraphMonitor: failed creating init event");
	}

	// create thread, pass instance
	graphMonitorThread_ = CreateThread( 
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)(&GraphMonitor::monitorGraphs),
			this,
			0,
			&graphMonitorThreadID_);
 
	if ( graphMonitorThread_ == NULL )
	{
		log_error("GraphMonitor: failed spinning GraphMonitor thread (%d)\n",
				GetLastError());

		throw std::runtime_error("GraphMonitor: failed spinning thread");
	}

	DWORD rc = WaitForSingleObject(initDoneEvent_, INFINITE);
	//FIXME: consider a timeout

	return;

}

GraphMonitor::~GraphMonitor()
{
	// signal thread to shutdown
	if ( !SetEvent(terminateEvent_) )
	{
		log_error("failed to signal graph monitor thread to terminate (%d)\n",
				GetLastError());
		return;
	}

	// wait for thread to shutdown
	DWORD rc = WaitForSingleObject(graphMonitorThread_, INFINITE);

	if ( rc == WAIT_FAILED )
	{
		log_error("GraphMonitor: failed waiting for thread to return (%d)\n",
				GetLastError());
	}

	for ( unsigned int i = 0; i < graphContextList_.size(); i++ )
	{
		graphContextList_.erase( graphContextList_.begin() + i );
	}

	const graphListEvent * pListEvent;
	while ( graphListEventQueue_.dequeue(&pListEvent) )
	{
		delete pListEvent;
	}

	log_info("graph monitor thread destroyed\n");
}

// graph removals and additions dequeued by graph monitor thread
void
GraphMonitor::processListEvent()
{
	const graphListEvent * pListEvent;

	// take event off queue
	// either add or remove graph
	if ( !graphListEventQueue_.dequeue(&pListEvent) )
	{
		log_info("failed to dequeue a graph list event\n");
		return;
	}

	// get the Media Event interface
	IMediaEventEx *pME = pListEvent->graph;

	switch ( pListEvent->eventType )
	{
	case graphListEvent::add:
		{
			// get event handle, on which we can wait
			HANDLE *hEvent = new HANDLE;
			HRESULT hr = pME->GetEventHandle((OAEVENT *)&hEvent);
			if ( FAILED(hr) )
			{
				log_warn("failed to get handle for filter graph\n");
				break;
			}

			// create a context with all relevant info
			graphContext *pGraphContext = new graphContext();
			pGraphContext->pME = pME;
			pGraphContext->waitHandle = hEvent;
				
			// add graph context to list
			log_info("adding device #%d to graphMonitor list\n",
					graphContextList_.size());

			graphContextList_.push_back(pGraphContext);
		}
		break;

	case graphListEvent::remove:
		{
			// remove from vector
			int i = findContext(pME);
			if ( i >= 0 )
			{
					graphContext *pGraphContext = graphContextList_[i];

					graphContextList_.erase( graphContextList_.begin() + i );

					std::ostringstream strm;
					strm << "removed device #" << i <<
							" from graphMonitor list";

					if ( (unsigned int)i < graphContextList_.size() )
						strm << " (device numbers " << i + 1 <<
								"+ will slide down 1 slot)";

					log_info("%s\n", strm.str().c_str());

					delete pGraphContext;

					//FIXME: how does one free the waitHandle?
					return;
			}
			log_warn("failed to find MediaEvent interface to remove\n");
		}
		break;

	default:
		log_error("got a graph list UNKNOWN event\n");
		break;
	}

	delete pListEvent;
}

// graph addition enqueued by main thread
void
GraphMonitor::addGraph(IMediaEventEx *pME)
{
	graphListEvent * pListEvent = new graphListEvent;

	pListEvent->eventType = graphListEvent::add;
	pListEvent->graph = pME;
	
	// queue-up graph to be added,
	if ( !graphListEventQueue_.enqueue(pListEvent) )
	{
		log_warn("failed to add graph to event queue\n");
	}

	// signal thread to wake up and add it
	if ( !SetEvent(listEvent_) )
	{
		log_error("failed to signal graph monitor thread of new graph (%d)\n",
				GetLastError());
	}
}

// graph removal enqueued by main thread
void
GraphMonitor::removeGraph(IMediaEventEx *pME)
{
	graphListEvent * pListEvent = new graphListEvent;

	pListEvent->eventType = graphListEvent::remove;
	pListEvent->graph = pME;
	
	// queue-up graph to be removed,
	if ( !graphListEventQueue_.enqueue(pListEvent) )
	{
		log_warn("failed to add graph to the event queue\n");
	}

	// signal thread to wake up and remove it
	if ( !SetEvent(listEvent_) )
	{
		log_error("failed to signal graph monitor thread to "
				"remove graph (%d)\n", GetLastError());
	}
}

DWORD WINAPI 
GraphMonitor::monitorGraphs(LPVOID lpParam)
{
	// extract instance
	GraphMonitor * pGraphMon = (GraphMonitor *)lpParam;

	// signal that thread is ready
	if ( !SetEvent(pGraphMon->initDoneEvent_) )
	{
		log_error("failed to signal that graph monitor thread is ready (%d)\n",
				GetLastError());
		return -1;
	}

	while ( true )
	{
		// Setup array of handles to wait on...
		enum { terminateIndex = 0, listEventIndex, graphEventIndex_0 };
		size_t numHandles = pGraphMon->graphContextList_.size() +
				graphEventIndex_0;
		HANDLE * waitHandles = new HANDLE [numHandles];

		// ...plus something to break-out when a handle must be
		// added or removed, or when thread must die
		waitHandles[listEventIndex] = pGraphMon->listEvent_;
		waitHandles[terminateIndex] = pGraphMon->terminateEvent_;

		// complete the array of handles
		for ( unsigned int i = 0; i < pGraphMon->graphContextList_.size(); i++ )
		{
			waitHandles[i + graphEventIndex_0] =
				pGraphMon->graphContextList_[i]->waitHandle;
		}

		// wait until a graph signals an event OR
		// a graph is added or removed OR
		// the thread must die
		DWORD rc = WaitForMultipleObjects(static_cast<DWORD>(numHandles),
				waitHandles, false, INFINITE);

		delete [] waitHandles;

		// get index of object that signaled
		unsigned int index = rc - WAIT_OBJECT_0;

		// get index of graph that MAY have had an event
		unsigned int handleIndex = index - graphEventIndex_0;

		if ( rc == WAIT_FAILED )
		{
			log_warn("graph monitor wait failed. (0x%x)\n", GetLastError());
		}
		else if ( index == listEventIndex )
		{	
			// process addition or removal of a graph
			pGraphMon->processListEvent();
			if ( !ResetEvent(pGraphMon->listEvent_) )
			{
				log_error("failed to reset graph monitor eventFlag."
						"Terminating.\n");

				// terminate
				break;
			}
		}
		else if ( index == terminateIndex )
		{
			// terminate
			break;
		}
		// check if it was a filter graph handle that signaled
		else if ( 0 <= handleIndex && 
				handleIndex < pGraphMon->graphContextList_.size() )
		{
			// handle event of appropriate graph
			pGraphMon->processGraphEvent(
					pGraphMon->graphContextList_[handleIndex]->pME);
		}
		else
		{
			log_warn("graph monitor: unknown wait rc = 0x%x\n", rc);
		}
	}

	return 0;
}

// Direct Show filter graph has signaled an event
// Device may have been removed
// Error may have occurred
void
GraphMonitor::processGraphEvent(IMediaEventEx *pME)
{
	HRESULT hr;
	long evCode, param1, param2;
	hr = pME->GetEvent(&evCode, &param1, &param2, 0);

	if ( SUCCEEDED(hr) )
	{
		// Event codes taken from:
		// http://msdn2.microsoft.com/en-us/library/ms783649.aspx
		// Embedded reference not used:
		// http://msdn2.microsoft.com/en-us/library/aa921722.aspx

		std::string str;

	    switch(evCode) 
	    { 
		case EC_DEVICE_LOST:
			if ( (int)param2 == 0 )
			{
				str.assign("EC_DEVICE_LOST\n");
				log_info("device removal detected\n");

				// shutdown capture
				if ( !cancelCBFunc_(pME, parentContext_) )
					log_warn("failed to find graph for final callback\n");
			}
			else
			{
				str.assign("EC_DEVICE_LOST - (device re-inserted)\n");
			}
			break;

		case EC_ACTIVATE:
			str.assign("EC_ACTIVATE\n");
			break;

/*		case EC_BANDWIDTHCHANGE:
			str.assign("EC_BANDWIDTHCHANGE\n");
			break;
*/
		case EC_BUFFERING_DATA:
			str.assign("EC_BUFFERING_DATA\n");
			break;

		case EC_BUILT:
			str.assign("EC_BUILT\n");
			break;

		case EC_CLOCK_CHANGED:
			str.assign("EC_CLOCK_CHANGED\n");
			break;

		case EC_CLOCK_UNSET:
			str.assign("EC_CLOCK_UNSET\n");
			break;

		case EC_CODECAPI_EVENT:
			str.assign("EC_CODECAPI_EVENT\n");
			break;

		case EC_COMPLETE:
			str.assign("EC_COMPLETE\n");
			break;

/*		case EC_CONTENTPROPERTY_CHANGED:
			str.assign("EC_CONTENTPROPERTY_CHANGED\n");
			break;
*/
		case EC_DISPLAY_CHANGED:
			str.assign("EC_DISPLAY_CHANGED\n");
			break;

		case EC_END_OF_SEGMENT:
			str.assign("EC_END_OF_SEGMENT\n");
			break;

/*		case EC_EOS_SOON:
			str.assign("EC_EOS_SOON\n");
			break;
*/
		case EC_ERROR_STILLPLAYING:
			str.assign("EC_ERROR_STILLPLAYING\n");
			break;

		case EC_ERRORABORT:
			str.assign("EC_ERRORABORT\n");

			log_info("graph monitor stopping capture...\n");

			// shutdown capture
			if ( !cancelCBFunc_(pME, parentContext_) )
				log_warn("failed to find graph for the final callback\n");

			break;

/*		case EC_ERRORABORTEX:
			str.assign("EC_ERRORABORTEX\n");
			break;
*/
		case EC_EXTDEVICE_MODE_CHANGE:
			str.assign("EC_EXTDEVICE_MODE_CHANGE\n");
			break;

/*		case EC_FILE_CLOSED:
			str.assign("EC_FILE_CLOSED\n");
			break;
*/
		case EC_FULLSCREEN_LOST:
			str.assign("EC_FULLSCREEN_LOST\n");
			break;

		case EC_GRAPH_CHANGED:
			str.assign("EC_GRAPH_CHANGED\n");
			break;

		case EC_LENGTH_CHANGED:
			str.assign("EC_LENGTH_CHANGED\n");
			break;

/*		case EC_LOADSTATUS:
			str.assign("EC_LOADSTATUS\n");
			break;
*/
/*		case EC_MARKER_HIT:
			str.assign("EC_MARKER_HIT\n");
			break;
*/
		case EC_NEED_RESTART:
			str.assign("EC_NEED_RESTART\n");
			break;

/*		case EC_NEW_PIN:
			str.assign("EC_NEW_PIN\n");
			break;
*/
		case EC_NOTIFY_WINDOW:
			str.assign("EC_NOTIFY_WINDOW\n");
			break;

		case EC_OLE_EVENT:
			str.assign("EC_OLE_EVENT\n");
			break;

		case EC_OPENING_FILE:
			str.assign("EC_OPENING_FILE\n");
			break;

		case EC_PALETTE_CHANGED:
			str.assign("EC_PALETTE_CHANGED\n");
			break;

		case EC_PAUSED:
			str.assign("EC_PAUSED\n");
			break;

/*		case EC_PLEASE_REOPEN:
			str.assign("EC_PLEASE_REOPEN\n");
			break;
*/
		case EC_PREPROCESS_COMPLETE:
			str.assign("EC_PREPROCESS_COMPLETE\n");
			break;

/*		case EC_PROCESSING_LATENCY:
			str.assign("EC_PROCESSING_LATENCY\n");
			break;
*/
		case EC_QUALITY_CHANGE:
			str.assign("EC_QUALITY_CHANGE\n");
			break;

/*		case EC_RENDER_FINISHED:
			str.assign("EC_RENDER_FINISHED\n");
			break;
*/
		case EC_REPAINT:
			str.assign("EC_REPAINT\n");
			break;

/*		case EC_SAMPLE_LATENCY:
			str.assign("EC_SAMPLE_LATENCY\n");
			break;

		case EC_SAMPLE_NEEDED:
			str.assign("EC_SAMPLE_NEEDED\n");
			break;

		case EC_SCRUB_TIME:
			str.assign("EC_SCRUB_TIME\n");
			break;
*/
		case EC_SEGMENT_STARTED:
			str.assign("EC_SEGMENT_STARTED\n");
			break;

		case EC_SHUTTING_DOWN:
			str.assign("EC_SHUTTING_DOWN\n");
			break;

		case EC_SNDDEV_IN_ERROR:
			str.assign("EC_SNDDEV_IN_ERROR\n");
			break;

		case EC_SNDDEV_OUT_ERROR:
			str.assign("EC_SNDDEV_OUT_ERROR\n");
			break;

		case EC_STARVATION:
			str.assign("EC_STARVATION\n");
			break;

		case EC_STATE_CHANGE:
			str.assign("EC_STATE_CHANGE\n");
			break;

/*		case EC_STATUS:
			str.assign("EC_STATUS\n");
			break;
*/
		case EC_STEP_COMPLETE:
			str.assign("EC_STEP_COMPLETE\n");
			break;

		case EC_STREAM_CONTROL_STARTED:
			str.assign("EC_STREAM_CONTROL_STARTED\n");
			break;

		case EC_STREAM_CONTROL_STOPPED:
			str.assign("EC_STREAM_CONTROL_STOPPED\n");
			break;

		case EC_STREAM_ERROR_STILLPLAYING:
			str.assign("EC_STREAM_ERROR_STILLPLAYING\n");
			break;

		case EC_STREAM_ERROR_STOPPED:
			str.assign("EC_STREAM_ERROR_STOPPED\n");
			break;

		case EC_TIMECODE_AVAILABLE:
			str.assign("EC_TIMECODE_AVAILABLE\n");
			break;

		case EC_UNBUILT:
			str.assign("EC_UNBUILT\n");
			break;

		case EC_USERABORT:
			str.assign("EC_USERABORT\n");
			break;

		case EC_VIDEO_SIZE_CHANGED:
			str.assign("EC_VIDEO_SIZE_CHANGED\n");
			break;

/*		case EC_VIDEOFRAMEREADY:
			str.assign("EC_VIDEOFRAMEREADY\n");
			break;
*/
		case EC_VMR_RECONNECTION_FAILED:
			str.assign("EC_VMR_RECONNECTION_FAILED\n");
			break;

		case EC_VMR_RENDERDEVICE_SET:
			str.assign("EC_VMR_RENDERDEVICE_SET\n");
			break;

		case EC_VMR_SURFACE_FLIPPED:
			str.assign("EC_VMR_SURFACE_FLIPPED\n");
			break;

		case EC_WINDOW_DESTROYED:
			str.assign("EC_WINDOW_DESTROYED\n");
			break;

		case EC_WMT_EVENT:
			str.assign("EC_WMT_EVENT\n");
			break;

		case EC_WMT_INDEX_EVENT:
			str.assign("EC_WMT_INDEX_EVENT\n");
			break;

		default:
			str.assign("unknown graph event code (%ld)\n", evCode);
			break;
	    } 

		log_info("graph #%d processed event: %s",
				findContext(pME), str.c_str());

	    hr = pME->FreeEventParams(evCode, param1, param2);
	}
	else
	{
		log_error("failed getting event for a graph\n");
	}
}

int
GraphMonitor::findContext(IMediaEventEx *pME)
{
	for ( unsigned int i = 0; i < graphContextList_.size(); i++ )
	{
		// found matching MediaEvent interface?
		if ( graphContextList_[i]->pME == pME )
		{
			return i;
		}
	}

	// failed to find MediaEvent interface
	return -1;
}
