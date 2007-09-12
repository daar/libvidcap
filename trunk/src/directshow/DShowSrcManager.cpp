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

#include <vidcap/vidcap.h>
#include "logging.h"

#include "DShowSrcManager.h"
#include "DirectShowSource.h"

DShowSrcManager * DShowSrcManager::instance_ = 0;

DShowSrcManager::DShowSrcManager()
	: numRefs_(0)
{
	graphMon_ = new GraphMonitor(
			(cancelCallbackFunc)&DShowSrcManager::cancelSrcCaptureCallback,
					this);

	HRESULT hr = CoInitialize(NULL);

	if ( FAILED(hr) )
	{
		throw std::runtime_error("failed calling CoInitialize()");
	}
}

DShowSrcManager::~DShowSrcManager()
{
	delete graphMon_;

	acquiredSourceIDs_.erase( acquiredSourceIDs_.begin(),
			acquiredSourceIDs_.end() );
	srcGraphList_.erase( srcGraphList_.begin(), srcGraphList_.end() );

	CoUninitialize();
}

DShowSrcManager *
DShowSrcManager::instance()
{
	if ( !instance_ )
		instance_ = new DShowSrcManager();

	instance_->numRefs_++;

	return instance_;
}

void
DShowSrcManager::release()
{
	numRefs_--;
	if ( !numRefs_ )
	{
		delete instance_;
		instance_ = 0;
	}
}

int
DShowSrcManager::registerNotifyCallback(void * sapiCtx)
{
	return devMon_.registerCallback(static_cast<sapi_context *>(sapiCtx));
}

void
DShowSrcManager::registerSrcGraph(void *src, IMediaEventEx *pME)
{
	// create a context with all relevant info
	srcGraphContext *pSrcGraphContext = new srcGraphContext();
	pSrcGraphContext->pME = pME;
	pSrcGraphContext->pSrc = src;
		
	// add source graph context to list
	srcGraphList_.push_back(pSrcGraphContext);

	// request that GraphMonitor monitor the graph for errors
	graphMon_->addGraph(pME);
}

void
DShowSrcManager::unregisterSrcGraph(IMediaEventEx *pME)
{
	// find appropriate source  in list
	for ( unsigned int i = 0; i < srcGraphList_.size(); i++ )
	{
		// found matching MediaEvent interface?
		if ( srcGraphList_[i]->pME == pME )
		{
			// remove entry from srcGraphList_
			srcGraphContext *pSrcGraphContext = srcGraphList_[i];

			srcGraphList_.erase( srcGraphList_.begin() + i );

			delete pSrcGraphContext;

			// request that GraphMonitor stop monitoring the graph for errors
			graphMon_->removeGraph(pME);
			return; 
		}
	}
	log_warn("failed to find source to unregister for monitoring\n");
}

bool
DShowSrcManager::cancelSrcCaptureCallback(IMediaEventEx *pME, void *ctx)
{
	DShowSrcManager *self = static_cast<DShowSrcManager *>(ctx);

	// find appropriate source in list
	for ( unsigned int i = 0; i < self->srcGraphList_.size(); i++ )
	{
		// found matching MediaEvent interface?
		if ( self->srcGraphList_[i]->pME == pME )
		{
			// Found source to cancel callbacks for
			// Call canceller
			DirectShowSource * pSrc = 
					static_cast<DirectShowSource *>(self->srcGraphList_[i]->pSrc);

			// cancel the source's callback
			pSrc->cancelCallbacks();
			return true;
		}
	}

	return false;
}

int
DShowSrcManager::scan(struct sapi_src_list * srcList) const
{
	//FIXME: consider multiple sources per device

	int newListLen = 0;

	// Create an enumerator (self-releasing)
	CComPtr<ICreateDevEnum> pCreateDevEnum;

	pCreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);

	if ( !pCreateDevEnum )
	{
		log_error("failed creating device enumerator - to scan\n");
		return 0;
	}

	// Enumerate video capture devices
	CComPtr<IEnumMoniker> pEm;

	pCreateDevEnum->CreateClassEnumerator(
			CLSID_VideoInputDeviceCategory, &pEm, 0);

	if ( !pEm )
	{
		log_error("Failed creating enumerator moniker\n");
		return 0;
	}

	pEm->Reset();

	ULONG ulFetched;
	IMoniker * pM;

	while ( pEm->Next(1, &pM, &ulFetched) == S_OK )
	{
		CComPtr< IBindCtx > pbc;

		HRESULT hr = CreateBindCtx(0, &pbc);
		if ( FAILED(hr) )
		{
			log_error("failed CreateBindCtx\n");
			return 0;
		}

		IBaseFilter * pCaptureFilter = 0;
		CComPtr<IPin> pOutPin = 0;

		hr = pM->BindToObject(pbc, 0, IID_IBaseFilter,
				(void **)&pCaptureFilter);

		if ( FAILED(hr) )
		{
			log_error("failed BindToObject\n");
			goto clean_continue;
		}

		//NOTE: We do this to eliminate some 'ghost' entries that
		//      don't correspond to a physically plugged-in device
		pOutPin = getOutPin( pCaptureFilter, 0 );
		if ( !pOutPin )
			goto clean_continue;

		newListLen++;

		struct vidcap_src_info *srcInfo;
		if ( newListLen > srcList->len )
			srcList->list = (struct vidcap_src_info *)realloc(srcList->list,
					newListLen * sizeof(struct vidcap_src_info));

		srcInfo = &srcList->list[newListLen - 1];

		//FIXME: rename to getDeviceId()
		sprintDeviceInfo(pM, pbc,
				srcInfo->identifier, srcInfo->description,
				min(sizeof(srcInfo->identifier),
					sizeof(srcInfo->description)));

clean_continue:
		if ( pCaptureFilter )
			pCaptureFilter->Release();

		if ( pOutPin )
			pOutPin.Release();

		pM->Release();
	}

	srcList->len = newListLen;

	return srcList->len;
}

bool
DShowSrcManager::okayToBuildSource(const char *id)
{
	// Enforce exclusive access to a source
	for ( unsigned int i = 0; i < acquiredSourceIDs_.size(); i++ )
	{
		if ( !strcmp(acquiredSourceIDs_[i], id) )
		{
			// a source with this id already exists - and was acquired
			log_warn("source already acquired: '%s'\n", id);
			return false;
		}
	}

	// add source to collection
	acquiredSourceIDs_.push_back(id);

	return true;
}

void DShowSrcManager::sourceReleased(const char *id)
{
	for ( unsigned int i = 0; i < acquiredSourceIDs_.size(); i++ )
	{
		if ( !strcmp(acquiredSourceIDs_[i], id) )
		{
			// found source with matching id

			acquiredSourceIDs_.erase( acquiredSourceIDs_.begin() + i );

			return;
		}
	}

	log_warn("couldn't find source '%s' to release\n", id);
}

bool
DShowSrcManager::getJustCapDevice(const char *devLongName,
		IBindCtx **ppBindCtx,
		IMoniker **ppMoniker) const
{
	HRESULT hr;

	// Create an enumerator
	CComPtr<ICreateDevEnum> pCreateDevEnum;

	pCreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);

	if ( !pCreateDevEnum )
	{
		log_error("failed creating device enumerator - to get a source\n");
		return false;
	}

	// Enumerate video capture devices
	CComPtr<IEnumMoniker> pEm;

	pCreateDevEnum->CreateClassEnumerator(
			CLSID_VideoInputDeviceCategory, &pEm, 0);

	if ( !pEm )
	{
		log_error("failed creating enumerator moniker\n");
		return false;
	}

	pEm->Reset();

	ULONG ulFetched;
	IMoniker * pM;

	// Iterate over all video capture devices
	int i=0;
	while ( pEm->Next(1, &pM, &ulFetched) == S_OK )
	{
		IBindCtx *pbc;

		hr = CreateBindCtx(0, &pbc);

		if ( FAILED(hr) )
		{
			log_error("failed CreateBindCtx\n");
			pM->Release();
			return false;
		}

		// Get the device names
		char *shortName;
		char *longName;
		if ( getDeviceInfo(pM, pbc, &shortName, &longName) )
		{
			log_warn("failed to get device info.\n");
			pbc->Release();
			continue;
		}

		// Compare with the desired dev name
		if ( !strcmp(longName, devLongName) )
		{
			// Got the correct device
			*ppMoniker = pM;
			*ppBindCtx = pbc;

			free(shortName);
			free(longName);
			return true;
		}

		// Wrong device. Cleanup and try again
		free(shortName);
		free(longName);

		pbc->Release();
	}

	pM->Release();

	return false;
}

///// Private functions /////

void
DShowSrcManager::sprintDeviceInfo(IMoniker * pM, IBindCtx * pbc,
		char* idBuff, char *descBuff, int buffsSize) const
{
	USES_CONVERSION;

	HRESULT hr;

	CComPtr< IMalloc > pMalloc;

	hr = CoGetMalloc(1, &pMalloc);

	if ( FAILED(hr) )
	{
		log_error("failed CoGetMalloc\n");
		return;
	}

	LPOLESTR pDisplayName;

	hr = pM->GetDisplayName(pbc, 0, &pDisplayName);

	if ( FAILED(hr) )
	{
		log_warn("failed GetDisplayName\n");
		return;
	}

	// This gets stack memory, no dealloc needed.
	char * pszDisplayName = OLE2A(pDisplayName);

	pMalloc->Free(pDisplayName);

	CComPtr< IPropertyBag > pPropBag;

	hr = pM->BindToStorage(pbc, 0, IID_IPropertyBag,
			(void **)&pPropBag);

	if ( FAILED(hr) )
	{
		log_error("failed getting video device property bag\n");
		return;
	}

	VARIANT v;
	VariantInit(&v);

	char * pszFriendlyName = 0;

	hr = pPropBag->Read(L"FriendlyName", &v, 0);

	if ( SUCCEEDED(hr) )
		pszFriendlyName = _com_util::ConvertBSTRToString(v.bstrVal);

	sprintf_s(descBuff, buffsSize, "%s", pszFriendlyName);
	sprintf_s(idBuff, buffsSize, "%s", pszDisplayName);

	delete [] pszFriendlyName;
}

IPin *
DShowSrcManager::getOutPin( IBaseFilter * pFilter, int nPin ) const
{
	CComPtr<IPin> pComPin = 0;

	getPin(pFilter, PINDIR_OUTPUT, nPin, &pComPin);

	return pComPin;
}

HRESULT
DShowSrcManager::getPin( IBaseFilter * pFilter, PIN_DIRECTION dirrequired,
			int iNum, IPin **ppPin) const
{
	*ppPin = NULL;

	CComPtr< IEnumPins > pEnum;

	HRESULT hr = pFilter->EnumPins(&pEnum);

	if ( FAILED(hr) )
		return hr;

	ULONG ulFound;
	IPin * pPin;

	hr = E_FAIL;

	while ( S_OK == pEnum->Next(1, &pPin, &ulFound) )
	{
		PIN_DIRECTION pindir;

		pPin->QueryDirection(&pindir);

		if ( pindir == dirrequired )
		{
			if ( iNum == 0 )
			{
				*ppPin = pPin;
				hr = S_OK;
				break;
			}
			iNum--;
		}

		pPin->Release();
	}

	return hr;
}

// Allocates and returns the friendlyName and displayName for a device
int
DShowSrcManager::getDeviceInfo(IMoniker * pM, IBindCtx * pbc,
		char** easyName, char **longName) const
{
	USES_CONVERSION;

	HRESULT hr;

	CComPtr< IMalloc > pMalloc;

	hr = CoGetMalloc(1, &pMalloc);

	if ( FAILED(hr) )
	{
		log_error("failed CoGetMalloc\n");
		return 1;
	}

	LPOLESTR pDisplayName;

	hr = pM->GetDisplayName(pbc, 0, &pDisplayName);

	if ( FAILED(hr) )
	{
		log_warn("failed GetDisplayName\n");
		return 1;
	}

	// This gets stack memory, no dealloc needed.
	char * pszDisplayName = OLE2A(pDisplayName);

	// Allocate a copy of this long name
	*longName = _strdup(pszDisplayName);

	pMalloc->Free(pDisplayName);

	CComPtr< IPropertyBag > pPropBag;

	hr = pM->BindToStorage(pbc, 0, IID_IPropertyBag,
			(void **)&pPropBag);

	if ( FAILED(hr) )
	{
		log_warn("failed getting video device property bag\n");
		return 1;
	}

	VARIANT v;
	VariantInit(&v);

	char * pszFriendlyName = 0;

	hr = pPropBag->Read(L"FriendlyName", &v, 0);

	if ( SUCCEEDED(hr) )
		pszFriendlyName =
			_com_util::ConvertBSTRToString(v.bstrVal);

	// Allocate a copy of this short name
	*easyName = _strdup(pszFriendlyName);

	delete [] pszFriendlyName;
	return 0;
}
