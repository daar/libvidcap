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

#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <atlbase.h>
#include <qedit.h>
#include <comutil.h>
#include <DShow.h>

#include <vidcap/converters.h>
#include "hotlist.h"
#include "logging.h"
#include "sapi.h"
#include "DirectShowSource.h"

DirectShowSource::DirectShowSource(struct sapi_src_context *src,
		DShowSrcManager *mgr)
	: sourceContext_(src),
	  dshowMgr_(mgr),
	  pSource_(0),
	  pCapGraphBuilder_(0),
	  pStreamConfig_(0),
	  pFilterGraph_(0),
	  pMediaEventIF_(0),
	  pSampleGrabber_(0),
	  pSampleGrabberIF_(0),
	  pNullRenderer_(0),
	  pMediaControlIF_(0),
	  nativeMediaType_(0),
	  captureIsSetup_(false),
	  eventInitDone_(0),
	  eventStart_(0),
	  eventStop_(0),
	  eventTerminate_(0),
	  sourceThread_(0),
	  sourceThreadID_(0)
{
	if ( !dshowMgr_ )
	{
		log_error("NULL manager passed to DirectShowSource constructor.");
		throw std::runtime_error("NULL manager passed to DirectShowSource");
	}

	if ( !sourceContext_ )
	{
		log_error("NULL source context passed to DirectShowSource ctor.");
		throw std::runtime_error("NULL source context passed to constructor");
	}

	if ( !dshowMgr_->okayToBuildSource( getID() ) )
	{
		log_warn("Mgr won't permit construction of DirectShowSource w/id '%s'."
				" It's already been acquired.\n", getID());
		throw std::runtime_error("source already acquired");
	}

	IBindCtx * pBindCtx = 0;
	IMoniker * pMoniker = 0;

	// Get the capture device - identified by it's long display name
	if ( !dshowMgr_->getJustCapDevice(getID(), &pBindCtx, &pMoniker) )
	{
		log_warn("Failed to get device '%s'.\n", getID());
		throw std::runtime_error("failed to get device");
	}

	HRESULT hr = pMoniker->BindToObject(pBindCtx, 0, IID_IBaseFilter,
				(void **)&pSource_);

	if ( FAILED(hr) )
	{
		log_error("Failed BindToObject (%d)\n", hr);
		goto constructionFailure;
	}

	hr = CoCreateInstance(CLSID_CaptureGraphBuilder2,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_ICaptureGraphBuilder2,
			(void **)&pCapGraphBuilder_);

	if ( FAILED(hr) )
	{
		log_error("failed creating capture graph builder (%d)\n", hr);
		goto constructionFailure;
	}

	hr = pCapGraphBuilder_->FindInterface(&PIN_CATEGORY_CAPTURE,
			&MEDIATYPE_Video,
			pSource_,
			IID_IAMStreamConfig,
			(void **)&pStreamConfig_);
	if( FAILED(hr) )
	{
		log_error("failed getting stream config "
				"while building format list (%d)\n", hr);
		pCapGraphBuilder_->Release();
		goto constructionFailure;
	}

	hr = CoCreateInstance(CLSID_FilterGraph,
			0,
			CLSCTX_INPROC_SERVER,
			IID_IGraphBuilder,
			(void **)&pFilterGraph_);
	if ( FAILED(hr) )
	{
		log_error("failed creating the filter graph (%d)\n", hr);
		goto constructionFailure;
	}

	hr = pCapGraphBuilder_->SetFiltergraph(pFilterGraph_);
	if ( FAILED(hr) )
	{
		log_error("failed setting the filter graph (%d)\n", hr);
		goto constructionFailure;
	}

	hr = pFilterGraph_->QueryInterface(IID_IMediaEventEx,
			(void **)&pMediaEventIF_);
	if ( FAILED(hr) )
	{
		log_error("failed getting IMediaEventEx interface (%d)\n", hr);
		goto constructionFailure;
	}

	hr = pFilterGraph_->QueryInterface(IID_IMediaControl,
			(void **)&pMediaControlIF_);
	if ( FAILED(hr) )
	{
		log_error("failed getting Media Control interface (%d)\n", hr);
		goto constructionFailure;
	}

	// register filter graph - to monitor for errors during capture
	dshowMgr_->registerSrcGraph(src->src_info.identifier, this, pMediaEventIF_);

	if ( createEvents() )
	{
		log_error("failed creating events for source thread");
		goto constructionFailure;
	}

	// pass instance to thread
	sourceThread_ = CreateThread(
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)(&DirectShowSource::waitForCmd),
			this,
			0,
			&sourceThreadID_);

	if ( sourceThread_ != NULL )
	{
		// wait for signal from thread that it is ready
		WaitForSingleObject(eventInitDone_, INFINITE);

		return;
	}

	log_error("failed spinning source thread (%d)\n",
			GetLastError());

constructionFailure:

	if ( pMediaControlIF_ )
		pMediaControlIF_->Release();

	if ( pFilterGraph_ )
		pFilterGraph_->Release();

	if ( pSource_ )
		pSource_->Release();

	pMoniker->Release();
	pBindCtx->Release();

	dshowMgr_->sourceReleased( getID() );

	throw std::runtime_error("failed source construction");
}

DirectShowSource::~DirectShowSource()
{
	log_info("Signaling source '%s' to terminate...\n", 
			sourceContext_->src_info.description);

	// signal thread to shutdown
	if ( !SetEvent(eventTerminate_) )
	{
		log_error("failed to signal graph monitor thread to terminate (%d)\n",
				GetLastError());
		return;
	}

	// wait for thread to shutdown
	DWORD rc = WaitForSingleObject(sourceThread_, INFINITE);

	if ( rc == WAIT_FAILED )
	{
		log_error("DirectShowSource: failed waiting for thread to return (%d)\n",
				GetLastError());
	}

	log_info("source '%s' has terminated\n", 
			sourceContext_->src_info.description);

	CloseHandle(eventInitDone_);
	CloseHandle(eventStart_);
	CloseHandle(eventStop_);
	CloseHandle(eventTerminate_);
}

void
DirectShowSource::terminate()
{
	doStop();

	cleanupCaptureGraphFoo();

	if ( nativeMediaType_ )
		freeMediaType(*nativeMediaType_);

	// These below were initialized in constructor
	if ( pMediaControlIF_ )
		pMediaControlIF_->Release();
	pMediaControlIF_ = 0;

	pMediaEventIF_->Release();

	pFilterGraph_->Release();

	pSource_->Release();

	pStreamConfig_->Release();

	pCapGraphBuilder_->Release();

	dshowMgr_->sourceReleased( getID() );
}

int
DirectShowSource::createEvents()
{
	// create an event used to signal that the thread has been created
	eventInitDone_ = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		);

	if ( eventInitDone_ == NULL)
	{
		log_error("DirectShowSource: failed creating initDone event (%d)\n",
				GetLastError());
		return -1;
	}

	// create an event used to signal thread to start capture
	eventStart_ = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		);

	if ( eventStart_ == NULL)
	{
		log_error("DirectShowSource: failed creating start event (%d)\n",
				GetLastError());
		CloseHandle(eventInitDone_);
		return -1;
	}

	// create an event used to signal thread to stop capture
	eventStop_ = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		);

	if ( eventStop_ == NULL)
	{
		log_error("DirectShowSource: failed creating stop event (%d)\n",
				GetLastError());
		CloseHandle(eventInitDone_);
		CloseHandle(eventStart_);
		return -1;
	}

	// create an event used to signal thread to terminate
	eventTerminate_ = CreateEvent(
		NULL,               // default security attributes
		TRUE,               // manual-reset event
		FALSE,              // initial state is clear
		NULL                // no name
		);

	if ( eventTerminate_ == NULL)
	{
		log_error("DirectShowSource: failed creating terminate event (%d)\n",
				GetLastError());
		CloseHandle(eventInitDone_);
		CloseHandle(eventStart_);
		CloseHandle(eventStop_);
		return -1;
	}

	return 0;
}

DWORD WINAPI
DirectShowSource::waitForCmd(LPVOID lpParam)
{
	// extract instance
	DirectShowSource * pSrc = (DirectShowSource *)lpParam;

	// signal to main thread that we are ready for commands
	if ( !SetEvent(pSrc->eventInitDone_) )
	{
		log_error("failed to signal that source thread is ready (%d)\n",
				GetLastError());
		return -1;
	}

	enum { startEventIndex = 0, stopEventIndex,
			terminateEventIndex };

	size_t numHandles = terminateEventIndex + 1;
	HANDLE * waitHandles = new HANDLE [numHandles];

	// ...plus something to break-out when a handle must be
	// added or removed, or when thread must die
	waitHandles[startEventIndex]     = pSrc->eventStart_;
	waitHandles[stopEventIndex]      = pSrc->eventStop_;
	waitHandles[terminateEventIndex] = pSrc->eventTerminate_;

	while ( true )
	{
		// wait until signaled to start or stop capture
		// OR to terminate
		DWORD rc = WaitForMultipleObjects(static_cast<DWORD>(numHandles),
				waitHandles, false, INFINITE);

		// get index of object that signaled
		unsigned int index = rc - WAIT_OBJECT_0;

		if ( rc == WAIT_FAILED )
		{
			log_warn("source wait failed. (0x%x)\n", GetLastError());
		}
		else if ( index == startEventIndex )
		{
			pSrc->doStart();

			if ( !ResetEvent(pSrc->eventStart_) )
			{
				log_error("failed to reset source start event flag."
						"Terminating.\n");
				// terminate
				break;
			}
		}
		else if ( index == stopEventIndex )
		{
			pSrc->doStop();

			if ( !ResetEvent(pSrc->eventStop_) )
			{
				log_error("failed to reset source stop event flag."
						"Terminating.\n");
				// terminate
				break;
			}
		}
		else if ( index == terminateEventIndex )
		{
			pSrc->terminate();
			break;
		}
	}

	delete [] waitHandles;

	return 0;
}

void
DirectShowSource::cleanupCaptureGraphFoo()
{
	if ( pNullRenderer_ )
		pNullRenderer_->Release();
	pNullRenderer_ = 0;

	if ( pSampleGrabberIF_ )
		pSampleGrabberIF_->Release();
	pSampleGrabberIF_ = 0;

	if ( pSampleGrabber_ )
		pSampleGrabber_->Release();
	pSampleGrabber_ = 0;

	captureIsSetup_ = false;
}

int
DirectShowSource::setupCaptureGraphFoo()
{
	if ( !captureIsSetup_ ) 
	{
		HRESULT hr = CoCreateInstance(CLSID_SampleGrabber,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IBaseFilter,
				(void **)&pSampleGrabber_);
		if ( FAILED(hr) )
		{
			log_error("failed creating Sample Grabber (%d)\n", hr);
			return -1;
		}

		hr = pSampleGrabber_->QueryInterface(IID_ISampleGrabber,
				(void**)&pSampleGrabberIF_);
		if ( FAILED(hr) )
		{
			log_error("failed getting ISampleGrabber interface (%d)\n", hr);
			goto bail_1;
		}

		// Capture more than once
		hr = pSampleGrabberIF_->SetOneShot(FALSE);
		if ( FAILED(hr) )
		{
			log_error("failed SetOneShot (%d)\n", hr);
			goto bail_2;
		}

		hr = pSampleGrabberIF_->SetBufferSamples(FALSE);
		if ( FAILED(hr) )
		{
			log_error("failed SetBufferSamples (%d)\n", hr);
			goto bail_2;
		}

		// set which callback type and function to use
		hr = pSampleGrabberIF_->SetCallback( this, 1 );
		if ( FAILED(hr) )
		{
			log_error("failed to set callback (%d)\n", hr);
			goto bail_2;
		}

		// Set sample grabber's media type to match that of the source
		hr = pSampleGrabberIF_->SetMediaType(nativeMediaType_);
		if ( FAILED(hr) )
		{
			log_error("failed to set grabber media type (%d)\n", hr);
			goto bail_2;
		}

		hr = CoCreateInstance(CLSID_NullRenderer,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IBaseFilter,
				(void **)&pNullRenderer_);

		if ( FAILED(hr) )
		{
			log_error("failed creating a NULL renderer (%d)\n", hr);
			goto bail_2;
		}

		//FIXME: use actual device name
		hr = pFilterGraph_->AddFilter(pSource_, L"Source Device");
		if ( FAILED(hr) )
		{
			log_error("failed to add source (%d)\n", hr);
			goto bail_3;
		}

		hr = pFilterGraph_->AddFilter(pSampleGrabber_, L"Sample Grabber");
		if ( FAILED(hr) )
		{
			log_error("failed to add Sample Grabber to filter graph (%d)\n",
					hr);
			goto bail_3;
		}

		hr = pFilterGraph_->AddFilter(pNullRenderer_, L"NullRenderer");
		if ( FAILED(hr) )
		{
			log_error("failed to add null renderer (%d)\n", hr);
			goto bail_3;
		}

		hr = pCapGraphBuilder_->RenderStream(&PIN_CATEGORY_CAPTURE,
				&MEDIATYPE_Video,
				pSource_,
				pSampleGrabber_,
				pNullRenderer_);
		if ( FAILED(hr) )
		{
			log_error("failed to connect source, grabber "
					"and null renderer (%d)\n", hr);
			goto bail_3;
		}

		captureIsSetup_ = true;
	}

	return 0;

bail_3:
	pNullRenderer_->Release();
	pNullRenderer_ = 0;
bail_2:
	pSampleGrabberIF_->Release();
	pSampleGrabberIF_ = 0;
bail_1:
	pSampleGrabber_->Release();
	pSampleGrabber_ = 0;

	return -1;
}

int
DirectShowSource::start()
{
	// signal to source thread to start capturing
	if ( !SetEvent(eventStart_) )
	{
		log_error("failed to signal source to start (%d)\n",
				GetLastError());
		return -1;
	}

	return 0;
}

void
DirectShowSource::doStart()
{
	if ( !setupCaptureGraphFoo() )
	{
		HRESULT hr = pMediaControlIF_->Run();

		if ( SUCCEEDED(hr) )
			return;
		else
			log_error("failed to run filter graph for source '%s' (%ul 0x%x)\n",
					sourceContext_->src_info.description, hr, hr);
	}

	// call capture callback - with error status
	// (vidcap will reset capture_callback)
	sapi_src_capture_notify(sourceContext_, 0, 0, -1);
}

int
DirectShowSource::stop()
{
	// signal to source thread to stop capturing
	if ( !SetEvent(eventStop_) )
	{
		log_error("failed to signal source to stop (%d)\n",
				GetLastError());
		return -1;
	}

	return 0;
}

void
DirectShowSource::doStop()
{
	if ( captureIsSetup_ )
	{
		HRESULT hr = pMediaControlIF_->Stop();
		if ( FAILED(hr) )
		{
			log_error("failed to STOP the filter graph (0x%0x)\n", hr);
		}
	}
}

int
DirectShowSource::validateFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative) const
{
	AM_MEDIA_TYPE *mediaFormat;

	if ( !findBestFormat(fmtNominal, fmtNative, &mediaFormat) )
		return 0;

	freeMediaType(*mediaFormat);
	return 1;
}

int
DirectShowSource::bindFormat(const vidcap_fmt_info * fmtNominal)
{
	vidcap_fmt_info fmtNative;

	// If we've already got one, free it
	if ( nativeMediaType_ )
	{
		freeMediaType(*nativeMediaType_);
		nativeMediaType_ = 0;
	}

	if ( !findBestFormat(fmtNominal, &fmtNative, &nativeMediaType_) )
		return 1;

	// set the framerate
	VIDEOINFOHEADER * vih = (VIDEOINFOHEADER *) nativeMediaType_->pbFormat;
	vih->AvgTimePerFrame = 10000000 *
		fmtNative.fps_denominator /
		fmtNative.fps_numerator;

	// set the dimensions
	vih->bmiHeader.biWidth = fmtNative.width;
	vih->bmiHeader.biHeight = fmtNative.height;

	cleanupCaptureGraphFoo();

	// set the stream's media type
	HRESULT hr = pStreamConfig_->SetFormat(nativeMediaType_);
	if ( FAILED(hr) )
	{
		log_error("failed setting stream format (%d)\n", hr);

		freeMediaType(*nativeMediaType_);
		nativeMediaType_ = 0;

		return 1;
	}

	return 0;
}

bool
DirectShowSource::findBestFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative, AM_MEDIA_TYPE **mediaFormat) const

{
	bool needsFpsEnforcement = false;
	bool needsFmtConv = false;

	int iCount = 0, iSize = 0;
	HRESULT hr = pStreamConfig_->GetNumberOfCapabilities(&iCount, &iSize);

	// Check the size to make sure we pass in the correct structure.
	if (iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS) )
	{
		log_error("capabilities struct is wrong size (%d not %d)\n",
				iSize, sizeof(VIDEO_STREAM_CONFIG_CAPS));
		return false;
	}

	struct formatProperties
	{
		bool isSufficient;
		bool needsFmtConversion;
		bool needsFpsEnforcement;
		AM_MEDIA_TYPE *mediaFormat;
	};

	// these will be filled-in by checkFormat()
	struct formatProperties * candidateFmtProps =
				new struct formatProperties [iCount];
	vidcap_fmt_info * fmtsNative = new vidcap_fmt_info [iCount];

	// enumerate each NATIVE source format
	bool itCanWork = false;
	for (int iFormat = 0; iFormat < iCount; ++iFormat )
	{
		candidateFmtProps[iFormat].isSufficient = false;
		candidateFmtProps[iFormat].needsFmtConversion = false;
		candidateFmtProps[iFormat].needsFpsEnforcement = false;
		candidateFmtProps[iFormat].mediaFormat = 0;

		// evaluate each native source format
		if ( checkFormat(fmtNominal, &fmtsNative[iFormat], iFormat,
					&(candidateFmtProps[iFormat].needsFmtConversion),
					&(candidateFmtProps[iFormat].needsFpsEnforcement),
					&(candidateFmtProps[iFormat].mediaFormat)) )
		{
			candidateFmtProps[iFormat].isSufficient = true;
			itCanWork = true;
		}
	}

	// Can ANY of this source's native formats
	// satisfy the requested format (to be bound)?
	if ( !itCanWork )
	{
		goto freeThenReturn;
	}

	// evaluate the possibilities

	int bestFmtNum = -1;

	// any that work without mods?
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient &&
			!candidateFmtProps[iFmt].needsFmtConversion &&
			!candidateFmtProps[iFmt].needsFpsEnforcement )
		{
			// found a perfect match
			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	// any that work without format conversion? but with framerate conversion?
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient &&
			!candidateFmtProps[iFmt].needsFmtConversion &&
			 candidateFmtProps[iFmt].needsFpsEnforcement )
		{
			// found an okay match
			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	// any that work with format conversion? but without framerate conversion?
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient &&
			 candidateFmtProps[iFmt].needsFmtConversion &&
			!candidateFmtProps[iFmt].needsFpsEnforcement )
		{
			// found a so-so match
			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	// any that work at all (with both conversions)?
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient )
		{
			// found a poor match
			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	log_error("findBestFormat: coding ERROR\n");
	itCanWork = false;

freeThenReturn:

	// Free all sufficient candidates (unless it's the the CHOSEN one)
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient )
		{
			// NOT the chosen one?
			if ( !itCanWork || iFmt != bestFmtNum )
				freeMediaType(*candidateFmtProps[iFmt].mediaFormat);
		}
	}

	// Can bind succeed?
	if ( itCanWork )
	{
		// take note of native media type, fps, dimensions, fourcc
		*mediaFormat = candidateFmtProps[bestFmtNum].mediaFormat;

		fmtNative->fps_numerator =
						fmtsNative[bestFmtNum].fps_numerator;
		fmtNative->fps_denominator =
						fmtsNative[bestFmtNum].fps_denominator;
		fmtNative->width = fmtsNative[bestFmtNum].width;
		fmtNative->height = fmtsNative[bestFmtNum].height;
		fmtNative->fourcc = fmtsNative[bestFmtNum].fourcc;
	}

	delete [] candidateFmtProps;
	delete [] fmtsNative;

	return itCanWork;
}

// Evaluate one of perhaps several native formats for
// suitability for providing the nominal format.
// Fill-in output parameter 'mediaFormat'.
bool
DirectShowSource::checkFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative,
		int formatNum,
		bool *needsFramerateEnforcing,
		bool *needsFormatConversion,
		AM_MEDIA_TYPE **mediaFormat) const
{
	// get video stream capabilities structure #(formatNum)
	VIDEO_STREAM_CONFIG_CAPS scc;
	AM_MEDIA_TYPE *pMediaType;
	HRESULT hr = pStreamConfig_->GetStreamCaps(formatNum, &pMediaType,
			(BYTE*)&scc);

	if (FAILED(hr))
	{
		log_warn("checkFormat: failed getting stream capabilities [%d] (%d)\n",
				formatNum + 1, hr);
		return false;
	}

	// check resolution
	if ( fmtNominal->width < scc.MinOutputSize.cx ||
	     fmtNominal->height < scc.MinOutputSize.cy ||
	     fmtNominal->width > scc.MaxOutputSize.cx ||
	     fmtNominal->height > scc.MaxOutputSize.cy )
	{
		freeMediaType(*pMediaType);
		return false;
	}

	bool matchesWidth = false;
	for (int width = scc.MinOutputSize.cx; width <= scc.MaxOutputSize.cx;
				width += scc.OutputGranularityX)
	{
		if ( width == fmtNominal->width )
		{
			matchesWidth = true;
			break;
		}
	}

	bool matchesHeight = false;
	for (int height = scc.MinOutputSize.cy; height <= scc.MaxOutputSize.cy;
				height += scc.OutputGranularityY)
	{
		if ( height == fmtNominal->height )
		{
			matchesHeight = true;
			break;
		}
	}

	if ( !matchesWidth || !matchesHeight )
	{
		freeMediaType(*pMediaType);
		return false;
	}

	// calculate range of supported frame rates
	double fpsMin = static_cast<double>( 1000000000 / scc.MaxFrameInterval)
			/ 100.0;
	double fpsMax = static_cast<double>( 1000000000 / scc.MinFrameInterval)
			/ 100.0;

	double fps = static_cast<double>(fmtNominal->fps_numerator) /
		static_cast<double>(fmtNominal->fps_denominator);

	// check framerate
	if ( fps > fpsMax )
	{
		freeMediaType(*pMediaType);
		return false;
	}

	if ( fps < fpsMin )
		*needsFramerateEnforcing = true;

	// check media type

	int nativeFourcc = 0;
	if ( mapDirectShowMediaTypeToVidcapFourcc(
				pMediaType->subtype.Data1, nativeFourcc) )
	{
		freeMediaType(*pMediaType);
		return false;
	}

	*needsFormatConversion = ( nativeFourcc != fmtNominal->fourcc );

	if ( *needsFormatConversion )
	{
		if ( conv_conversion_func_get(nativeFourcc, fmtNominal->fourcc) == 0 )
		{
			freeMediaType(*pMediaType);
			return false;
		}
	}

	fmtNative->fourcc = nativeFourcc;

	fmtNative->width = fmtNominal->width;
	fmtNative->height = fmtNominal->height;

	if ( *needsFramerateEnforcing )
	{
		//FIXME: Use float. Drop numerator/denominator business.
		fmtNative->fps_numerator = (int)fpsMax;
		fmtNative->fps_denominator = 1;
	}
	else
	{
		fmtNative->fps_numerator = fmtNominal->fps_numerator;
		fmtNative->fps_denominator = fmtNominal->fps_denominator;
	}

	// return this suitable media type
	*mediaFormat = pMediaType;

	return true;
}

void
DirectShowSource::freeMediaType(AM_MEDIA_TYPE &mediaType) const
{
	if ( mediaType.cbFormat != 0 )
		CoTaskMemFree((PVOID)mediaType.pbFormat);

	if ( mediaType.pUnk != NULL )
		mediaType.pUnk->Release();
}

void
DirectShowSource::cancelCallbacks()
{
	// have buffer callbacks already been cancelled?
	if ( !sourceContext_->capture_callback )
		return;

	// stop callbacks before thinking of 
	// touching sourceContext_->capture_callback
	stop();

	// call capture callback - but let vidcap and the
	// app know that this is the last time
	// (vidcap will reset capture_callback)
	sapi_src_capture_notify(sourceContext_, 0, 0, -1);
}

STDMETHODIMP
DirectShowSource::QueryInterface(REFIID riid, void ** ppv)
{
	if ( ppv == NULL )
		return E_POINTER;

	if( riid == IID_ISampleGrabberCB || riid == IID_IUnknown )
	{
		*ppv = (void *) static_cast<ISampleGrabberCB*> ( this );
		return NOERROR;
	}

	return E_NOINTERFACE;
}

// The sample grabber calls us back from its deliver thread
STDMETHODIMP
DirectShowSource::BufferCB( double dblSampleTime, BYTE * pBuff, long buffSize )
{
	if ( !sourceContext_->capture_callback )
		return 0;

	return sapi_src_capture_notify(sourceContext_,
			reinterpret_cast<const char *>(pBuff),
			static_cast<int>(buffSize), 0);
}

int
DirectShowSource::mapDirectShowMediaTypeToVidcapFourcc(DWORD data, int & fourcc)
{
	switch ( data )
	{
	case 0xe436eb7e: 
		fourcc = VIDCAP_FOURCC_RGB32;
		break;
	case 0x30323449: // I420
	case 0x56555949: // IYUV
		fourcc = VIDCAP_FOURCC_I420;
		break;
	case 0x32595559:
		fourcc = VIDCAP_FOURCC_YUY2;
		break;
	case 0xe436eb7d:
		fourcc = VIDCAP_FOURCC_BOTTOM_UP_RGB24;
		break;
	case 0xe436eb7c:
		fourcc = VIDCAP_FOURCC_RGB555;
		break;
	case 0x39555659:
		fourcc = VIDCAP_FOURCC_YVU9;
		break;
	default:
		log_warn("failed to map 0x%08x to vidcap fourcc\n", data);
		return -1;
	}

	return 0;
}
