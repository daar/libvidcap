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

#ifndef _DEVMONITOR_H_
#define _DEVMONITOR_H_

#include <windows.h>
#include <vidcap/vidcap.h>

// Allow use of features specific to Windows XP or later.      
#ifndef _WIN32_WINNT

// Change this to the appropriate value to target other versions of Windows.
// see: http://msdn2.microsoft.com/en-us/library/aa383745.aspx
#define _WIN32_WINNT 0x0501

#endif						

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

class DevMonitor
{

public:
	DevMonitor();
	~DevMonitor();
	int registerCallback(void *);

private:
	static LRESULT __stdcall
	processWindowsMsgs(HWND, UINT, WPARAM, LPARAM);

	static DWORD WINAPI
	monitorDevices(LPVOID lpParam);

private:
	HANDLE initDoneEvent_;
	enum threadStatusEnum { initializing=0, initFailed, initDone };
	threadStatusEnum threadStatus_;

	HWND windowHandle_;
	void * devMonitorThread_;
	DWORD devMonitorThreadID_;
	TCHAR * szTitle_;
	TCHAR * szWindowClass_;

	void * sapiCtx_;

	CRITICAL_SECTION  registrationMutex_;
};

#endif
