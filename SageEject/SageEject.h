
// SageEject.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols

using CDiskSet = std::set< wchar_t >;

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
			: Help( false )
		{
		}

		void ParseParam(const TCHAR* pszParam, BOOL bFlag, BOOL bLast) override
		{
			UNUSED_ALWAYS( bLast );

			if ( bFlag )
			{
				if ( pszParam[ 0 ] ==_T('?') )
				{
					Help = true;
				}
			}
			else if ( pszParam[ 0 ] ==_T('*') || ( _istalpha( pszParam[ 0 ] ) && pszParam[ 1 ] == _T(':') ) )
			{
				ToEject.emplace( pszParam[ 0 ] );
			}
		}

		bool		Help;
		CDiskSet	ToEject;
	};

	CCmd CommandLine;

	DECLARE_MESSAGE_MAP()
};

extern CSageEjectApp theApp;
