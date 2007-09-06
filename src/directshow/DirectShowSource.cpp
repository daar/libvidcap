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
	  pIntermediateFilter_(0),
	  intermediateMediaType_(0),
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

#if 0
	if ( !buildFormatList(pBindCtx, pMoniker) )
	{
		log_warn("Failed building format list for '%s'.\n", getID());
		goto constructionFailure;
	}
#else

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
#endif

	// FIXME: does this really need to be a member? Maybe should be in start().
	pSourceOutPin_ = dshowMgr_->getOutPin( pSource_, 0 );

	if ( !pSourceOutPin_ )
	{
		log_error("Failed getting source output pin\n");
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

	// These below were initialized in constructor
	pMediaEventIF_->Release();

	pFilterGraph_->Release();

	pSourceOutPin_.Release();

	pSource_->Release();

	pStreamConfig_->Release();

	pCapGraphBuilder_->Release();

	dshowMgr_->sourceReleased( getID() );

	DeleteCriticalSection(&captureMutex_);
}

#if 0
int
DirectShowSource::buildFormatList(IBindCtx * pBindCtx, IMoniker * pMoniker)
{
	HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_ICaptureGraphBuilder2,
			(void **)&pCapGraphBuilder_);

	if ( FAILED(hr) )
	{
		log_error("failed creating capture graph builder (%d)\n", hr);
		return 0;
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
		return 0;
	}

	int iCount = 0, iSize = 0;
	hr = pStreamConfig_->GetNumberOfCapabilities(&iCount, &iSize);

	// Check the size to make sure we pass in the correct structure.
	if (iSize != sizeof(VIDEO_STREAM_CONFIG_CAPS) )
	{
		log_error("capabilities struct is wrong size (%d not %d)\n",
				iSize, sizeof(VIDEO_STREAM_CONFIG_CAPS));
		pStreamConfig_->Release();
		pCapGraphBuilder_->Release();
		return 0;
	}

	// enumerate each source format
	for (int iFormat = 0; iFormat < iCount; iFormat++)
	{
		// get the video stream capabilities structure
		VIDEO_STREAM_CONFIG_CAPS scc;
		AM_MEDIA_TYPE *pMediaType;
		hr = pStreamConfig_->GetStreamCaps(iFormat, &pMediaType,
				(BYTE*)&scc);

		if (FAILED(hr))
		{
			log_warn("failed getting stream capabilities [%d of %d] (%d)\n",
					iFormat+1, iCount, hr);
			continue;
		}

		int fourcc;

		if ( mapDirectShowMediaTypeToVidcapFourcc(
					pMediaType->subtype.Data1, fourcc) )
			continue;

		// calculate range of supported frame rates
		double fpsMin = static_cast<double>( 1000000000 / scc.MaxFrameInterval)
				/ 100.0;
		double fpsMax = static_cast<double>( 1000000000 / scc.MinFrameInterval)
				/ 100.0;

		// enumerate a format for each frame rate
		for ( int i = 0; i < hot_fps_list_len; ++i )
		{
			int hot_fps_num = hot_fps_list[i].fps_numerator;
			int hot_fps_den = hot_fps_list[i].fps_denominator;
			double fps = static_cast<double>(hot_fps_num) /
				static_cast<double>(hot_fps_den);

			if ( fps < fpsMin || fps > fpsMax )
				continue;

			for ( int j = 0; j < hot_resolution_list_len; ++j )
			{
				vidcap_fmt_info * fmt;

				const int hot_width =
					hot_resolution_list[j].width;
				const int hot_height =
					hot_resolution_list[j].height;

				if ( hot_width < scc.MinOutputSize.cx ||
				     hot_height < scc.MinOutputSize.cy ||
				     hot_width > scc.MaxOutputSize.cx ||
				     hot_height > scc.MaxOutputSize.cy )
				{
					continue;
				}

				fmtListLen_++;
				pFmtList_ = static_cast<vidcap_fmt_info *>(
						realloc(pFmtList_,
							fmtListLen_ *
							sizeof(vidcap_fmt_info)));

				if ( !pFmtList_ )
				{
					log_error("failed reallocating format list\n");

					dshowMgr_->freeMediaType(*pMediaType);
					pStreamConfig_->Release();
					pCapGraphBuilder_->Release();
					return 0;
				}

				fmt = &pFmtList_[fmtListLen_ - 1];

				fmt->width = hot_width;
				fmt->height = hot_height;
				fmt->fourcc = fourcc;
				fmt->fps_numerator = hot_fps_num;
				fmt->fps_denominator = hot_fps_den;

				// TODO: revisit OutputGranularity if a device
				// is found that uses it
			}
		}

		dshowMgr_->freeMediaType(*pMediaType);
	}

	return fmtListLen_;
}
#endif

bool
DirectShowSource::canConvertToRGB32()
{
	if ( pIntermediateFilter_ )
		pIntermediateFilter_->Release();

	// used if format conversion is necessary
	intermediateMediaType_ = 0;
	pIntermediateFilter_ = 0;

	IFilterMapper2 *pMapper = NULL;
	IEnumMoniker *pEnum = NULL;

	HRESULT hr;
	hr = CoCreateInstance(CLSID_FilterMapper2,
			NULL,
			CLSCTX_INPROC,
			IID_IFilterMapper2,
			(void **) &pMapper);

	if (FAILED(hr))
	{
		log_error("failed to create FilterMapper2 (%d)\n", hr);
		return false;
	}

	// search for a filter to convert to RGB32

	GUID arrayInTypes[2];
	GUID arrayOutTypes[2];
	memset(&arrayInTypes[1], 0, sizeof(arrayInTypes[1]));
	memset(&arrayOutTypes[1], 0, sizeof(arrayOutTypes[1]));

	CComPtr< IEnumMediaTypes > pMediaEnum;
	hr = pSourceOutPin_->EnumMediaTypes(&pMediaEnum);
	if ( FAILED(hr) )
	{
		log_error("failed creating media type enumerator (rc=%d)\n", hr);
		pMapper->Release();
		return false;
	}

	// enumerate all media types of source
	ULONG ulFound;
	AM_MEDIA_TYPE * pMedia;
	while ( S_OK == pMediaEnum->Next(1, &pMedia, &ulFound) )
	{
		VIDEOINFOHEADER * vih = (VIDEOINFOHEADER *)pMedia->pbFormat;

		// find entry that matches desired format
		if ( vih->bmiHeader.biWidth != sourceContext_->fmt_nominal.width ||
		     vih->bmiHeader.biHeight != sourceContext_->fmt_nominal.height )
		{
			// invalid dimensions
			dshowMgr_->freeMediaType(*pMedia);
			continue;
		}

		arrayInTypes[0] = pMedia->majortype;
		arrayInTypes[1] = pMedia->subtype;

		arrayOutTypes[0] = MEDIATYPE_Video;
		arrayOutTypes[1] = MEDIASUBTYPE_RGB32;

#if 0
		log_info("considering input format %c%c%c%c\n",
				(arrayInTypes[1].Data1 >> 0) & 0xff,
				(arrayInTypes[1].Data1 >> 8) & 0xff,
				(arrayInTypes[1].Data1 >> 16) & 0xff,
				(arrayInTypes[1].Data1 >> 24) & 0xff);
#endif

		IEnumMoniker *pFilterEnum = NULL;
		hr = pMapper->EnumMatchingFilters(
				&pFilterEnum,
				0,                  // Reserved.
				TRUE,               // Use exact match?
				MERIT_DO_NOT_USE+1, // Minimum merit.
				TRUE,               // At least one input pin?

				1,                  // # of major type/subtype pairs for input
				arrayInTypes,       // Array of major/subtype pairs for input

				NULL,               // Input medium.
				NULL,               // Input pin category.
				FALSE,              // Must be a renderer?
				TRUE,               // At least one output pin?

				1,                  // # of major type/subtype pairs for output
				arrayOutTypes,      // Array of major/subtype pairs for output

				NULL,               // Output medium.
				NULL);              // Output pin category.

		// Enumerate the filter monikers
		IMoniker *pMoniker;
		ULONG cFetched;
		while ( pFilterEnum->Next(1, &pMoniker, &cFetched) == S_OK )
		{
			IPropertyBag *pPropBag = NULL;
			hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag,
					(void **)&pPropBag);

			if ( FAILED(hr) )
			{
				log_warn("Couldn't bind to storage for this matching filter\n");
				pMoniker->Release();
				continue;
			}

			// Get the friendly name of the filter
			VARIANT varName;
			VariantInit(&varName);
			hr = pPropBag->Read(L"FriendlyName", &varName, 0);
			if ( SUCCEEDED(hr) )
			{
				char * pszFriendlyName =
					_com_util::ConvertBSTRToString(
							varName.bstrVal);

				log_info("conversion filter '%s' should work\n",
						pszFriendlyName);

				delete [] pszFriendlyName;
			}
			VariantClear(&varName);

			// Create an instance of the filter
			hr = pMoniker->BindToObject(NULL,
					NULL,
					IID_IBaseFilter,
					(void**)&pIntermediateFilter_);

			if (FAILED(hr))
			{
				//FIXME: name it
				log_warn("failed to instantiate conversion filter (%d)\n",
						hr);

				pPropBag->Release();
				pMoniker->Release();
				pIntermediateFilter_ = 0;
				continue;
			}

			// TODO:
			// get output pin of intermediate filter?
			// get it's media type?

			intermediateMediaType_ = arrayInTypes[1].Data1;

			pPropBag->Release();
			pMoniker->Release();
			pFilterEnum->Release();
			dshowMgr_->freeMediaType(*pMedia);
			pMediaEnum.Release();
			pMapper->Release();

			return true;
		}

		// cleanup
		pFilterEnum->Release();
		dshowMgr_->freeMediaType(*pMedia);

		// failed to find filter suitable to convert this media type to RGB32
	}

	pMediaEnum.Release();
	pMapper->Release();

	// Can't build filter graph to convert this source's output to RGB32
	return false;
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
	AM_MEDIA_TYPE * pAmMediaType = getMediaType(pSourceOutPin_);
	if ( !pAmMediaType )
	{
		log_error("failed to match media type\n");
		goto bail_3;
	}

	// set the frame rate
	VIDEOINFOHEADER * vih = (VIDEOINFOHEADER *) pAmMediaType->pbFormat;
	vih->AvgTimePerFrame = 10000000 *
		sourceContext_->fmt_nominal.fps_denominator /
		sourceContext_->fmt_nominal.fps_numerator;

	// set the stream's media type
	hr = pStreamConfig_->SetFormat(pAmMediaType);
	if ( FAILED(hr) )
	{
		log_error("failed setting stream format (%d)\n", hr);
		goto bail_4;
	}

	// Set sample grabber's media type to match that of the source
	// OR the conversion filter
	if ( intermediateMediaType_ )
	{
		//FIXME: Shouldn't this be necessary?
		pAmMediaType->subtype.Data1 = intermediateMediaType_;
		//hr = pSampleGrabberIF_->SetMediaType(pAmMediaType);
		hr = pSampleGrabberIF_->SetMediaType(NULL);
	}
	else
	{
		hr = pSampleGrabberIF_->SetMediaType(pAmMediaType);
	}
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

	if ( pIntermediateFilter_ )
	{
		hr = pFilterGraph_->AddFilter(pIntermediateFilter_,
				L"ConversionFilter");
		if ( FAILED(hr) )
		{
			log_error("failed to add conversion filter (%d)\n", hr);
			goto bail_5;
		}

		hr = pCapGraphBuilder_->RenderStream(NULL,  //&PIN_CATEGORY_CAPTURE,
				NULL,  //&MEDIATYPE_Video,
				pSource_,
				pIntermediateFilter_,
				pSampleGrabber_);
		if ( FAILED(hr) )
		{
			log_error("failed to connect source, conversion filter, "
					"sample grabber (%lu  0x%x)\n", hr, hr);
			switch ( hr )
			{
			case VFW_S_NOPREVIEWPIN:
				log_error("VFW_S_NOPREVIEWPIN\n");
				break;

			case E_FAIL:
				log_error("E_FAIL\n");
				break;

			case E_POINTER:
				log_error("E_POINTER\n");
				break;

			case VFW_E_NOT_IN_GRAPH:
				log_error("VFW_E_NOT_IN_GRAPH\n");
				break;

			case E_INVALIDARG:
				log_error("E_INVALIDARG\n");
				break;

			default:
				log_error("default\n");
				break;
			}
			goto bail_5;
		}

		hr = pCapGraphBuilder_->RenderStream(
				NULL, //&PIN_CATEGORY_CAPTURE,
				NULL, //&MEDIATYPE_Video,
				pSampleGrabber_,
				NULL,
				pNullRenderer_);
		if ( FAILED(hr) )
		{
			log_error("failed to connect grabber and null renderer (%d)\n",
					hr);
			goto bail_5;
		}
	}
	else
	{
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
	dshowMgr_->freeMediaType(*pAmMediaType);
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

		if ( pIntermediateFilter_ )
			pIntermediateFilter_->Release();
		pIntermediateFilter_ = 0;

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
DirectShowSource::bindFormat(const vidcap_fmt_info * fmtInfo)
{
	return 0;
}

bool
DirectShowSource::validateFormat(const vidcap_fmt_info * fmtNominal,
		vidcap_fmt_info * fmtNative) const
{
	// TODO: actually validate

	return false;
}

//FIXME: should this be generic?
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

	log_info("cancelled source's capture callbacks\n");

	stop();

	log_info("cancelled source's capture callbacks and stopped capture\n");
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

//FIXME: Is there a better way to get the media type
//       from the width, height and fourcc?
AM_MEDIA_TYPE *
DirectShowSource::getMediaType( CComPtr< IPin > pPin) const
{
	CComPtr< IEnumMediaTypes > pEnum;

	HRESULT hr = pPin->EnumMediaTypes(&pEnum);

	if ( FAILED(hr) )
	{
		log_error("failed creating media type enumerator (rc=%d)\n", hr);
		return NULL;
	}

	ULONG ulFound;
	AM_MEDIA_TYPE * pMedia;
	DWORD boundMediaType;

	if ( mapVidcapFourccToDirectShowMediaType(
				sourceContext_->fmt_nominal.fourcc,
				boundMediaType) )
		return NULL;

	int requiredMediaType = intermediateMediaType_ ?
		intermediateMediaType_ : boundMediaType;

	while ( S_OK == pEnum->Next(1, &pMedia, &ulFound) )
	{
		//printMediaFormatType(pMedia);

		VIDEOINFOHEADER * vih =
				(VIDEOINFOHEADER *) pMedia->pbFormat;
		int thisFourcc;

		if ( mapDirectShowMediaTypeToVidcapFourcc(pMedia->subtype.Data1,
				thisFourcc) )
			continue;

		// find entry that matches desired format
		if ( vih->bmiHeader.biWidth ==
				sourceContext_->fmt_nominal.width &&
				vih->bmiHeader.biHeight ==
				sourceContext_->fmt_nominal.height &&
				(thisFourcc ==
				 sourceContext_->fmt_nominal.fourcc ||
				 pMedia->subtype.Data1 == requiredMediaType) )
		{
			return pMedia;
		}

		dshowMgr_->freeMediaType(*pMedia);
	}

	return NULL;
}

void
DirectShowSource::printMediaFormatType(AM_MEDIA_TYPE *pMedia)
{
	if ( pMedia->formattype == FORMAT_DvInfo )
		log_info("FORMAT_DvInfo\n");
	else if ( pMedia->formattype == FORMAT_MPEG2Video )
		log_info("FORMAT_MPEG2Video\n");
	else if ( pMedia->formattype == FORMAT_MPEGStreams )
		log_info("FORMAT_MPEGStreams\n");
	else if ( pMedia->formattype == FORMAT_MPEGVideo )
		log_info("FORMAT_MPEGVideo\n");
	else if ( pMedia->formattype == FORMAT_None )
		log_info("FORMAT_None\n");
	else if ( pMedia->formattype == FORMAT_VideoInfo )
		log_info("FORMAT_VideoInfo\n");
	else if ( pMedia->formattype == FORMAT_VideoInfo2 )
		log_info("FORMAT_VideoInfo2\n");
	else if ( pMedia->formattype == FORMAT_WaveFormatEx )
		log_info("FORMAT_WaveFormatEx\n");
	else
		log_info("unknown\n");
}

int
DirectShowSource::mapDirectShowMediaTypeToVidcapFourcc(DWORD data, int & fourcc)
{
	switch ( data )
	{
	case 0xe436eb7e: // MEDIASUBTYPE_RGB32.Data1
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
		log_warn("failed to map 0x%08x to fourcc\n", data);
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
		data = 0xe436eb7e; // MEDIASUBTYPE_RGB32.Data1
		break;
	case VIDCAP_FOURCC_I420:
		data = 0x30323449; // '024I' aka I420
		break;
	case VIDCAP_FOURCC_YUY2:
		data = 0x32595559;
		break;
	default:
		log_warn("failed to map '%s' to DS media type\n",
				vidcap_fourcc_string_get(fourcc));
		return -1;
	}

	return 0;
}
