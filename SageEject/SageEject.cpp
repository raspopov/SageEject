// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
This file is part of SageEject

Copyright (C) 2022 Nikolay Raspopov <raspopov@cherubicsoft.com>

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
( at your option ) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see < http://www.gnu.org/licenses/>.
*/

#include "pch.h"
#include "SageEject.h"
#include "SageEjectDlg.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

// CSageEjectApp

BEGIN_MESSAGE_MAP(CSageEjectApp, CWinAppEx)
	ON_COMMAND(ID_HELP, &CWinAppEx::OnHelp)
END_MESSAGE_MAP()

// CSageEjectApp construction

CSageEjectApp::CSageEjectApp()
{
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	EnableHtmlHelp();
}

// The one and only CSageEjectApp object

CSageEjectApp theApp;

// CSageEjectApp initialization

BOOL CSageEjectApp::InitInstance()
{
	const INITCOMMONCONTROLSEX icc = { sizeof ( INITCOMMONCONTROLSEX ), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
	InitCommonControlsEx( &icc );

	SetAppID( AfxGetAppName() );

	CWinAppEx::InitInstance();

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager( RUNTIME_CLASS( CMFCVisualManagerWindows ) );

	SetRegistryKey( AFX_IDS_COMPANY_NAME );

	EnableTaskbarInteraction();

	theApp.ParseCommandLine( CommandLine );

	if ( CommandLine.Help )
	{
		CString help;
		help.Format( IDS_HELP, PathFindFileName( theApp.m_pszExeName ) );
		AfxMessageBox( help, MB_OK | MB_ICONINFORMATION );
	}
	else
	{
		CSageEjectDlg dlg;
		m_pMainWnd = &dlg;
		dlg.DoModal();
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
