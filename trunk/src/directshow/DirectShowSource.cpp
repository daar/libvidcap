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
	  stoppingCapture_(false)
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

	// register filter graph - to monitor for errors during capture
	// FIXME: Is this too early? Should we only do it during captures?
	// FIXME: Consider renaming to filterGraphMediaEventInterface
	dshowMgr_->registerSrcGraph(this, pMediaEventIF_);

	InitializeCriticalSection(&captureMutex_);

	return;

constructionFailure:

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
	stop();

	dshowMgr_->unregisterSrcGraph(pMediaEventIF_);

	if ( nativeMediaType_ )
		dshowMgr_->freeMediaType(*nativeMediaType_);

	// These below were initialized in constructor
	pMediaEventIF_->Release();

	pFilterGraph_->Release();

	pSource_->Release();

	pStreamConfig_->Release();

	pCapGraphBuilder_->Release();

	dshowMgr_->sourceReleased( getID() );

	DeleteCriticalSection(&captureMutex_);
}

int
DirectShowSource::start()
{
	HRESULT hr = pFilterGraph_->QueryInterface(IID_IMediaControl,
			(void **)&pMediaControlIF_);
	if ( FAILED(hr) )
	{
		log_error("failed getting Media Control interface (%d)\n", hr);
		return -1;
	}

	hr = CoCreateInstance(CLSID_SampleGrabber,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_IBaseFilter,
			(void **)&pSampleGrabber_);
	if ( FAILED(hr) )
	{
		log_error("failed creating Sample Grabber (%d)\n", hr);
		goto bail_1;
	}

	hr = pSampleGrabber_->QueryInterface(IID_ISampleGrabber,
			(void**)&pSampleGrabberIF_);
	if ( FAILED(hr) )
	{
		log_error("failed getting ISampleGrabber interface (%d)\n", hr);
		goto bail_2;
	}

	// Capture more than once
	hr = pSampleGrabberIF_->SetOneShot(FALSE);
	if ( FAILED(hr) )
	{
		log_error("failed SetOneShot (%d)\n", hr);
		goto bail_3;
	}

	hr = pSampleGrabberIF_->SetBufferSamples(FALSE);
	if ( FAILED(hr) )
	{
		log_error("failed SetBufferSamples (%d)\n", hr);
		goto bail_3;
	}

	// set which callback type and function to use
	hr = pSampleGrabberIF_->SetCallback( this, 1 );
	if ( FAILED(hr) )
	{
		log_error("failed to set callback (%d)\n", hr);
		goto bail_3;
	}

	// Get the appropriate source media type
	AM_MEDIA_TYPE * pAmMediaType = nativeMediaType_;
	if ( !pAmMediaType )
	{
		log_error("failed to match media type\n");
		goto bail_3;
	}

	// set the frame rate
	VIDEOINFOHEADER * vih = (VIDEOINFOHEADER *) pAmMediaType->pbFormat;
	vih->AvgTimePerFrame = 10000000 *
		sourceContext_->fmt_native.fps_denominator /
		sourceContext_->fmt_native.fps_numerator;

	// set the dimensions
	vih->bmiHeader.biWidth = sourceContext_->fmt_native.width;
	vih->bmiHeader.biHeight = sourceContext_->fmt_native.height;

	// set the stream's media type
	hr = pStreamConfig_->SetFormat(pAmMediaType);
	if ( FAILED(hr) )
	{
		log_error("failed setting stream format (%d)\n", hr);
		goto bail_4;
	}

	// Set sample grabber's media type to match that of the source
	//FIXME: do at bind time
	hr = pSampleGrabberIF_->SetMediaType(pAmMediaType);
	if ( FAILED(hr) )
	{
		log_error("failed to set grabber media type (%d)\n", hr);
		goto bail_4;
	}

	hr = CoCreateInstance(CLSID_NullRenderer,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_IBaseFilter,
			(void **)&pNullRenderer_);

	if ( FAILED(hr) )
	{
		log_error("failed creating a NULL renderer (%d)\n", hr);
		goto bail_4;
	}

	//FIXME: use actual device name
	hr = pFilterGraph_->AddFilter(pSource_, L"Source Device");
	if ( FAILED(hr) )
	{
		log_error("failed to add source (%d)\n", hr);
		goto bail_5;
	}

	hr = pFilterGraph_->AddFilter(pSampleGrabber_, L"Sample Grabber");
	if ( FAILED(hr) )
	{
		log_error("failed to add Sample Grabber to filter graph (%d)\n",
				hr);
		goto bail_5;
	}

	hr = pFilterGraph_->AddFilter(pNullRenderer_, L"NullRenderer");
	if ( FAILED(hr) )
	{
		log_error("failed to add null renderer (%d)\n", hr);
		goto bail_5;
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
		goto bail_5;
	}

	hr = pMediaControlIF_->Run();

	if ( SUCCEEDED(hr) )
	{
		return 0;
	}

	log_error("failed to run filter graph for source '%s' (%ul 0x%x)\n",
			sourceContext_->src_info.description, hr, hr);

	hr = pMediaControlIF_->Stop();
	if ( FAILED(hr) )
	{
		log_error("failed to STOP filter graph (%ul)\n", hr);
	}

bail_5:
	pNullRenderer_->Release();
	pNullRenderer_ = 0;
bail_4:
	// free this at next bind time, and in destructor 
	//dshowMgr_->freeMediaType(*pAmMediaType);
bail_3:
	pSampleGrabberIF_->Release();
	pSampleGrabberIF_ = 0;
bail_2:
	pSampleGrabber_->Release();
	pSampleGrabber_ = 0;
bail_1:
	EnterCriticalSection(&captureMutex_);
	pMediaControlIF_->Release();
	pMediaControlIF_ = 0;
	LeaveCriticalSection(&captureMutex_);

	return -1;
}

int
DirectShowSource::stop()
{
	stoppingCapture_ = true;

	EnterCriticalSection(&captureMutex_);

	if ( pMediaControlIF_ )
	{
		HRESULT hr = pMediaControlIF_->Stop();
		if ( FAILED(hr) )
		{
			log_error("failed to STOP the filter graph (%ul)\n", hr);
		}

		if ( pNullRenderer_ )
			pNullRenderer_->Release();
		pNullRenderer_ = 0;

		if ( pSampleGrabberIF_ )
			pSampleGrabberIF_->Release();
		pSampleGrabberIF_ = 0;

		if ( pSampleGrabber_ )
			pSampleGrabber_->Release();
		pSampleGrabber_ = 0;

		pMediaControlIF_->Release();
		pMediaControlIF_ = 0;
	}

	LeaveCriticalSection(&captureMutex_);

	stoppingCapture_ = false;

	return 0;
}

int
DirectShowSource::bindFormat(const vidcap_fmt_info * fmtNominal)
{
	// If we've already got one, free it
	if ( nativeMediaType_ )
		dshowMgr_->freeMediaType(*nativeMediaType_);

	bool needsFpsEnforcement = false;
	bool needsFmtConv = false;

	int iCount = 0, iSize = 0;
	HRESULT hr = pStreamConfig_->GetNumberOfCapabilities(&iCount, &iSize);

	// Check the size to make sure we pass in the correct structure.
	if (iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS) )
	{
		log_error("capabilities struct is wrong size (%d not %d)\n",
				iSize, sizeof(VIDEO_STREAM_CONFIG_CAPS));
		return 1;
	}

	struct formatProperties
	{
		bool isSufficient;
		bool needsFmtConversion;
		bool needsFpsEnforcement;
		AM_MEDIA_TYPE *mediaFormat;
	};

	struct formatProperties * candidateFmtProps =
				new struct formatProperties [iCount];

	// these will be filled-in by checkFormat()
	vidcap_fmt_info * fmtNative = new vidcap_fmt_info [iCount];

	// enumerate each NATIVE source format
	bool itCanWork = false;
	for (int iFormat = 0; iFormat < iCount; ++iFormat )
	{
		candidateFmtProps[iFormat].isSufficient = false;
		candidateFmtProps[iFormat].needsFmtConversion = false;
		candidateFmtProps[iFormat].needsFpsEnforcement = false;
		candidateFmtProps[iFormat].mediaFormat = 0;

		// evaluate each native source format
		if ( checkFormat(fmtNominal, &fmtNative[iFormat], iFormat,
					&(candidateFmtProps[iFormat].needsFmtConversion),
					&(candidateFmtProps[iFormat].needsFpsEnforcement),
					&(candidateFmtProps[iFormat].mediaFormat)) )
		{
			candidateFmtProps[iFormat].isSufficient = true;
			itCanWork = true;
		}
	}

	// NOTE: Will need to free unselected mediaFormats
	//       before returning,  but hold onto the best 
	//       (if any) candidate format

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
			// found a perfect match!
			log_info("bindFormat: found a 'perfect' match (fmt #%d)\n", iFmt);

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
			log_info("bindFormat: found an 'okay' match (fmt #%d)\n", iFmt);

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
			log_info("bindFormat: found a 'so-so' match (fmt #%d)\n", iFmt);

			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	// any that work with both caveats?
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient &&
			 candidateFmtProps[iFmt].needsFmtConversion &&
			 candidateFmtProps[iFmt].needsFpsEnforcement )
		{
			// found a poor match
			log_info("bindFormat: found a poor match (fmt #%d)\n", iFmt);

			bestFmtNum = iFmt;
			goto freeThenReturn;
		}
	}

	log_error("bindFormat: coding ERROR\n");
	itCanWork = false;

freeThenReturn:

	// Free all sufficient candidates (unless it's the the CHOSEN one)
	for ( int iFmt = 0; iFmt < iCount; ++iFmt )
	{
		if ( candidateFmtProps[iFmt].isSufficient )
		{
			// NOT the chosen one?
			if ( !itCanWork || iFmt != bestFmtNum )
				dshowMgr_->freeMediaType(*candidateFmtProps[iFmt].mediaFormat);
		}
	}

	// Can bind succeed?
	if ( itCanWork )
	{
			// take note of native media type, fps, dimensions
			nativeMediaType_ = candidateFmtProps[bestFmtNum].mediaFormat;
			sourceContext_->fmt_native.fps_numerator =
							fmtNative[bestFmtNum].fps_numerator;
			sourceContext_->fmt_native.fps_denominator =
							fmtNative[bestFmtNum].fps_denominator;
			sourceContext_->fmt_native.width = fmtNative[bestFmtNum].width;
			sourceContext_->fmt_native.height = fmtNative[bestFmtNum].height;
			sourceContext_->fmt_native.fourcc = fmtNative[bestFmtNum].fourcc;

			//FIXME: use these values NOW, instead of waiting for
			//       capture to start()
	}

	delete [] candidateFmtProps;
	delete [] fmtNative;

	if ( !itCanWork )
		return 1;

	return 0;
}

bool
DirectShowSource::validateFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative) const
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
		return 0;
	}

	// enumerate each NATIVE format for this source
	bool itWorks = false;
	for (int iFormat = 0; iFormat < iCount; iFormat++)
	{
		AM_MEDIA_TYPE *mediaFormat = 0;

		itWorks = checkFormat(fmtNominal, fmtNative, iFormat,
					&needsFpsEnforcement, &needsFmtConv,
					&mediaFormat);

		if ( itWorks )
		{
			dshowMgr_->freeMediaType(*mediaFormat);
			return 1;
		}
	}

	return 0;
}

// Evaluate one of perhaps several native formats for
// suitability for providing the nominal format.
// Fill-in output parameter 'mediaFormat'.
bool
DirectShowSource::checkFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative,
		int formatNum,
		bool *needsFramerateEnforcing, bool *needsFormatConversion,
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
		dshowMgr_->freeMediaType(*pMediaType);
		return false;
	}

	bool matchesWidth = false;
	for (int width = scc.MinOutputSize.cx; width <= scc.MaxOutputSize.cx;
				width += scc.OutputGranularityX)
	{
		if ( width == fmtNominal->width )
			matchesWidth = true;
	}

	bool matchesHeight = false;
	for (int height = scc.MinOutputSize.cy; height <= scc.MaxOutputSize.cy;
				height += scc.OutputGranularityY)
	{
		if ( height == fmtNominal->height )
			matchesHeight = true;
	}

	if ( !matchesWidth || !matchesHeight )
	{
		dshowMgr_->freeMediaType(*pMediaType);
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
		dshowMgr_->freeMediaType(*pMediaType);
		return false;
	}

	if ( fps < fpsMin )
		*needsFramerateEnforcing = true;

	// check media type

	int nativeFourcc = 0;
	if ( mapDirectShowMediaTypeToVidcapFourcc(
				pMediaType->subtype.Data1, nativeFourcc) )
	{
		dshowMgr_->freeMediaType(*pMediaType);
		return false;
	}

	*needsFormatConversion = ( nativeFourcc != fmtNominal->fourcc );

	if ( *needsFormatConversion && 
			(conv_conversion_func_get(nativeFourcc, fmtNominal->fourcc) == 0) )
	{
			dshowMgr_->freeMediaType(*pMediaType);
			return false;
	}

	// it's suitable. fill-in the native format values

	fmtNative->width = fmtNominal->width;
	fmtNative->height = fmtNominal->height;

	if ( *needsFormatConversion )
		fmtNative->fourcc = nativeFourcc;
	else
		fmtNative->fourcc = fmtNominal->fourcc;

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

	/*
	log_info("cf: can be satisfied with NATIVE format #%d: "
			"'%s'  %dx%d  %d/%d fps\n",
				formatNum,
				vidcap_fourcc_string_get(fmtNative->fourcc),
				fmtNative->width, fmtNative->height,
				fmtNative->fps_numerator,
				fmtNative->fps_denominator);
	*/

	// return this suitable media type
	*mediaFormat = pMediaType;

	//FIXME: adjust framerate and dimensions now - not at capture start time

	return true;
}

void
DirectShowSource::cancelCallbacks()
{
	// serialize with normal capture callbacks
	EnterCriticalSection(&captureMutex_);

	// only cancel callbacks once (unless re-registered)
	if ( !sourceContext_->capture_callback )
	{
		LeaveCriticalSection(&captureMutex_);
		return;
	}

	// call capture callback - but let the
	// app know that this is the last time
	sapi_src_capture_notify(sourceContext_, 0, 0, -1);

	LeaveCriticalSection(&captureMutex_);

	stop();
}

// Fake out interface queries
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

// The sample grabber is calling us back on its deliver thread.
// This is NOT the main app thread!
STDMETHODIMP
DirectShowSource::BufferCB( double dblSampleTime, BYTE * pBuff, long buffSize )
{
	// prevent deadlock while stopping
	if ( stoppingCapture_ )
		return 0;

	EnterCriticalSection(&captureMutex_);

	if ( !sourceContext_->capture_callback )
		return 0;

	int ret = sapi_src_capture_notify(sourceContext_,
			reinterpret_cast<const char *>(pBuff),
			static_cast<int>(buffSize), 0);

	LeaveCriticalSection(&captureMutex_);

	return ret;
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
		fourcc = VIDCAP_FOURCC_RGB24;
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

int
DirectShowSource::mapVidcapFourccToDirectShowMediaType(int fourcc, DWORD & data)
{
	switch ( fourcc )
	{
	case VIDCAP_FOURCC_RGB32:
		data = 0xe436eb7e;
		break;
	case VIDCAP_FOURCC_I420:
		data = 0x30323449; // '024I' aka I420
		break;
	case VIDCAP_FOURCC_YUY2:
		data = 0x32595559;
		break;
	default:
		log_warn("failed to map '%s' to DShow media type\n",
				vidcap_fourcc_string_get(fourcc));
		return -1;
	}

	return 0;
}
