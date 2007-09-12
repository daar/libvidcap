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

#ifndef _DIRECTSHOWSOURCE_H_
#define _DIRECTSHOWSOURCE_H_

#include <stdexcept>
#include <atlbase.h>
#include <qedit.h>

#include "DShowSrcManager.h"
#include "sapi_context.h"

class DirectShowSource : public ISampleGrabberCB
{

public:
	DirectShowSource(struct sapi_src_context *src, DShowSrcManager *);
	~DirectShowSource();

	int start();
	int stop();
	int bindFormat(const vidcap_fmt_info * fmtInfo);
	int validateFormat(const vidcap_fmt_info * fmtNominal,
			vidcap_fmt_info * fmtNative) const;

	void cancelCallbacks();

	const char * getID() const
	{
		return sourceContext_->src_info.identifier;
	}

private:
	int setupCaptureGraphFoo();
	void cleanupCaptureGraphFoo();
	bool checkFormat(const vidcap_fmt_info * fmtNominal,
			vidcap_fmt_info * fmtNative,
			int formatNum,
			bool *needsFramerateEnforcing, bool *needsFormatConversion,
			AM_MEDIA_TYPE **candidateMediaFormat) const;

	bool findBestFormat(const vidcap_fmt_info * fmtNominal,
			vidcap_fmt_info * fmtNative, AM_MEDIA_TYPE **mediaFormat) const;

	void freeMediaType(AM_MEDIA_TYPE &) const;

	// Fake out COM
	STDMETHODIMP_(ULONG) AddRef() { return 2; }
	STDMETHODIMP_(ULONG) Release() { return 1; }
	STDMETHODIMP QueryInterface(REFIID riid, void ** ppv);

	// Not used
	STDMETHODIMP SampleCB( double SampleTime, IMediaSample * pSample )
	{
		return S_OK;
	}

	// The sample grabber calls us back from its deliver thread
	STDMETHODIMP
	BufferCB( double dblSampleTime, BYTE * pBuffer, long lBufferSize );

	static int mapDirectShowMediaTypeToVidcapFourcc(DWORD data, int & fourcc);

private:
	struct sapi_src_context * sourceContext_;
	DShowSrcManager * dshowMgr_;
	IBaseFilter * pSource_;
	ICaptureGraphBuilder2 *pCapGraphBuilder_;
	IAMStreamConfig * pStreamConfig_;
	IGraphBuilder *pFilterGraph_;
	IMediaEventEx * pMediaEventIF_;
	IBaseFilter * pSampleGrabber_;
	ISampleGrabber * pSampleGrabberIF_;
	IBaseFilter * pNullRenderer_;
	IMediaControl * pMediaControlIF_;
	AM_MEDIA_TYPE *nativeMediaType_;
	CRITICAL_SECTION  captureMutex_;
	bool stoppingCapture_;
};

#endif
