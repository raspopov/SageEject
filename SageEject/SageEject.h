
// SageEject.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols

using CDiskSet = std::set< std::wstring >;

// CSageEjectApp:
// See SageEject.cpp for the implementation of this class
//

class CSageEjectApp : public CWinAppEx
{
public:
	CSageEjectApp();

	BOOL InitInstance() override;

	class CCmd : public CCommandLineInfo
	{
	public:
		CCmd() noexcept
			: Help	( false )
			, CDROM	( false )
			, Auto	( false )
		{
		}

		void ParseParam(const TCHAR* pszParam, BOOL bFlag, BOOL bLast) override
		{
			UNUSED_ALWAYS( bLast );

			if ( bFlag )
			{
				if ( _tcsicmp( pszParam, _T("?") ) == 0 || _tcsicmp( pszParam, _T("help") ) == 0 )
				{
					Help = true;
				}
				else if ( _tcsicmp( pszParam, _T("cdrom") ) == 0 )
				{
					CDROM = true;
				}
				else if ( _tcsicmp( pszParam, _T("auto") ) == 0 )
				{
					Auto = true;
				}
			}
			else
			{
				ToEject.emplace( pszParam );
			}
		}

		bool		Help;		// Need help information
		bool		CDROM;		// Include CD-ROM
		bool		Auto;		// Auto-exit if no ejection needed
		CDiskSet	ToEject;	// Devices to be ejected
	};

	CCmd CommandLine;

	DECLARE_MESSAGE_MAP()
};

extern CSageEjectApp theApp;
