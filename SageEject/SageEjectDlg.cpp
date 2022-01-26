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

// CSageEjectDlg dialog

IMPLEMENT_DYNAMIC(CSageEjectDlg, CDialogExSized)

CSageEjectDlg::CSageEjectDlg(CWnd* pParent /*=nullptr*/)
	: CDialogExSized	( IDD, pParent )
	, m_hIcon	( AfxGetApp()->LoadIcon( IDR_MAINFRAME ) )
	, m_Cancel	( false )
	, m_nDrives	( GetLogicalDrives() )
	, m_pTaskbar( nullptr )
{
}

void CSageEjectDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogExSized::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_DEVICE, m_wndDevices);
	DDX_Control(pDX, IDC_PROGRESS, m_wndProgress);
	DDX_Text(pDX, IDC_VOLUME, m_sVolumeName);
	DDX_Text(pDX, IDC_ID, m_sID);
	DDX_Text(pDX, IDC_DOS, m_sDOS);
	DDX_Text(pDX, IDC_PATH, m_sPath);
	DDX_Text(pDX, IDC_DISK, m_sDisk);
	DDX_Text(pDX, IDC_NAME, m_sName);
	DDX_Control(pDX, IDOK, m_wndEject);
	DDX_Control(pDX, IDC_OPEN, m_wndOpen);
}

struct device_sorter : std::binary_function< VolumeDeviceClass::Ptr, VolumeDeviceClass::Ptr, bool >
{
	inline bool operator()(const VolumeDeviceClass::Ptr& Left, const VolumeDeviceClass::Ptr& Right) const noexcept
	{
		const DWORD ltype = ( Left->LogicalType()  == DRIVE_UNKNOWN ) ? DRIVE_FIXED : Left->LogicalType();
		const DWORD rtype = ( Right->LogicalType() == DRIVE_UNKNOWN ) ? DRIVE_FIXED : Right->LogicalType();
		return ( ( ltype <  rtype ) || ( ( ltype  == rtype ) && ( ( Left->DeviceNumber() < Right->DeviceNumber() ) ||
			( ( Left->DeviceNumber() == Right->DeviceNumber() ) && ( Left->PartitionNumber() < Right->PartitionNumber() ) ) ) ) );
	}
};

struct device_is : std::binary_function< VolumeDeviceClass::Ptr, std::wstring, bool >
{
	inline bool operator()(const VolumeDeviceClass::Ptr& Left, const std::wstring& Right) const noexcept
	{
		// * - matches any removable device or CD-ROM only if enabled by /CDROM option
		return ( Right == L"*" && ( Left->LogicalType() == DRIVE_REMOVABLE || ( theApp.CommandLine.CDROM && Left->LogicalType() == DRIVE_CDROM ) ) ) ||
			// matches by logical name
			( _tcsnicmp( Left->LogicalName().c_str(), Right.c_str(), Right.size() ) == 0 ) ||
			// matches by DOS name
			( _tcsnicmp( Left->LogicalDosName().c_str(), Right.c_str(), Right.size() ) == 0 ) ||
			// matches by volume name
			( _tcsnicmp( Left->VolumeName().c_str(), Right.c_str(), Right.size() ) == 0 );
	}
};

bool CSageEjectDlg::Enumerate(const CDiskSet& to_eject)
{
	TRACE( _T("Enumerating devices...\n") );

	CWaitCursor wc;

	m_EjectTask.clear();
	m_wndDevices.ResetContent();

	m_Devices = VolumeDeviceClass::Create()->Devices();

	std::sort( m_Devices.begin(), m_Devices.end(), device_sorter() );

	// Output to interface
	int to_eject_index = CB_ERR, removable_index = CB_ERR;
	int disk = 0;
	for ( const auto& device : m_Devices )
	{
		CString name = device->Description().c_str();
		if ( ! device->LogicalName().empty() )
		{
			name += L" ";
			name += device->LogicalName().c_str();
		}
		if ( ! device->LogicalTitle().empty() )
		{
			name += L" \"";
			name += device->LogicalTitle().c_str();
			name += L"\"";
		}
		if ( ! device->LogicalFS().empty() )
		{
			name += L" [";
			name += device->LogicalFS().c_str();
			name += L"]";
		}
		if ( device->DeviceNumber() != (UINT)-1 && device->PartitionNumber() != (UINT)-1 )
		{
			name.AppendFormat( IDS_DEVICE_NUMBER, device->DeviceNumber(), device->PartitionNumber() );
		}

		TRACE( _T("Device #%d: [%d] %s\n"), disk, device->LogicalType(), (LPCTSTR)name );
		m_wndDevices.SetItemData( m_wndDevices.AddString( name ), disk );

		const bool removable = ( device->LogicalType() == DRIVE_REMOVABLE ) || ( device->LogicalType() == DRIVE_CDROM );

		// Take first removable device
		if ( removable )
		{
			if ( removable_index == CB_ERR )
			{
				removable_index = disk;
			}
		}

		// Take first device to be ejected
		if ( std::find_if( to_eject.begin(), to_eject.end(), std::bind1st( device_is(), device ) ) != to_eject.end() )
		{
			if ( to_eject_index == CB_ERR )
			{
				to_eject_index = disk;
			}

			// Eject removable devices only
			if ( removable )
			{
				m_EjectTask.emplace_back( disk );
			}
		}

		++ disk;
	}

	// The best selection
	if ( to_eject_index != CB_ERR )
	{
		m_wndDevices.SetCurSel( to_eject_index );
	}
	else if ( removable_index != CB_ERR )
	{
		m_wndDevices.SetCurSel( removable_index );
	}

	OnCbnSelchangeDisk();

	// Auto-eject first device
	PostMessage( WM_PROCESS, AutoEject );

	return ( m_wndDevices.GetCount() > 0 );
}

void CSageEjectDlg::Eject(size_t index)
{
	if ( m_Ejecting.joinable() )
	{
		// Already ejecting
		return;
	}

	ASSERT( index < m_Devices.size() );
	auto device = m_Devices.at( index );

	const int count = m_wndDevices.GetCount();
	for ( int i = 0; i < count; ++i )
	{
		if  ( m_wndDevices.GetItemData( i ) == index )
		{
			m_wndDevices.SetCurSel( i );

			OnCbnSelchangeDisk();
			break;
		}
	}

	if ( device->LogicalType() != DRIVE_REMOVABLE && device->LogicalType() != DRIVE_CDROM )
	{
		// Impossible
		return;
	}

	m_wndEject.EnableWindow( FALSE );
	m_wndDevices.EnableWindow( FALSE );

	m_wndProgress.ShowWindow( SW_SHOW );
	m_wndProgress.SetMarquee( TRUE, 100 );

	if ( m_pTaskbar )
	{
		m_pTaskbar->SetProgressState( GetSafeHwnd(), TBPF_INDETERMINATE );
	}

	m_Cancel = false;
	m_Ejecting = std::thread( [=]
	{
		CoInitialize( nullptr );

		// Required primarily for remote terminal sessions
		CAccessToken oToken;
		if ( oToken.GetProcessToken( TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY ) )
		{
			if ( ! oToken.EnablePrivilege( SE_UNDOCK_NAME ) )
			{
				TRACE( _T("EnablePrivilege(SE_UNDOCK_NAME) error: %d\n"), GetLastError() );
			}
			if ( ! oToken.EnablePrivilege( SE_LOAD_DRIVER_NAME ) )
			{
				TRACE( _T("EnablePrivilege(SE_LOAD_DRIVER_NAME) error: %d\n"), GetLastError() );
			}
		}

		for ( size_t i = 0; ! m_Cancel; ++i )
		{
			PostMessage( WM_PROCESS, Ejecting, i );

			// Trying to eject device
			if ( device->Eject() )
			{
				break;
			}

			Sleep( 1000 );

			PostMessage( WM_PROCESS, Flushing, i );

			// Trying to stop "curious" services
			StopServices();

			// Trying to flush device
			device->Flush();

			Sleep( 1000 );
		}

		PostMessage( WM_PROCESS, static_cast< WPARAM >( m_Cancel ? Cancel : Done ) );

		CoUninitialize();
	} );
}

void CSageEjectDlg::StopServices()
{
	// Services to stop
	static const LPCTSTR services[] =
	{
		// Windows Search (disabling access to X:\System Volume Information\IndexerVolumeGuid)
		_T("WSearch"),
		// Virtual Disk Service (VDS)
		//_T("vds"),
		// Windows SuperFetch (includes modern ReadyBoost)
		//_T("SysMain"),
		// Windows Vista ReadyBoost
		//_T("EMDMgmt")
	};
	if ( SC_HANDLE hSCM = OpenSCManager( nullptr, nullptr, SC_MANAGER_CONNECT | GENERIC_EXECUTE ) )
	{
		for ( auto service : services )
		{
			if ( SC_HANDLE hService = OpenService( hSCM, service, SERVICE_STOP | SERVICE_QUERY_STATUS ) )
			{
				SERVICE_STATUS ss = {};
				if ( ControlService( hService, SERVICE_CONTROL_STOP, &ss ) )
				{
					for ( const DWORD dwStartTime = GetTickCount(); ! m_Cancel; )
					{
						SERVICE_STATUS_PROCESS ssp = {};
						DWORD dwBytesNeeded = 0;
						if ( ! QueryServiceStatusEx( hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof( SERVICE_STATUS_PROCESS ), &dwBytesNeeded ) )
						{
							TRACE( _T("QueryServiceStatusEx(\"%s\") error: %u\n"), service, GetLastError() );
							break;
						}
						if ( ssp.dwCurrentState != SERVICE_STOP_PENDING )
						{
							// Already stopped
							break;
						}
						TRACE( _T("Stopping service: %s ...\n"), service );
						const DWORD dwWaitTime = min( 10000, max( 1000, ssp.dwWaitHint / 10 ) );
						Sleep( dwWaitTime );
						if ( GetTickCount() > dwStartTime + 20000 )
						{
							TRACE( _T("ControlService(\"%s\") error: Time-out\n"), service );
							break;
						}
					}
				}
				else if ( GetLastError() != ERROR_SERVICE_NOT_ACTIVE )
				{
					TRACE( _T("ControlService(\"%s\") error: %u\n"), service, GetLastError() );
				}
				CloseServiceHandle( hService );
			}
			else
			{
				TRACE( _T("OpenService(\"%s\") error: %u\n"), service, GetLastError() );
			}
		}
		CloseServiceHandle( hSCM );
	}
	else
	{
		TRACE( _T("OpenSCManager() error: %u\n"), GetLastError() );
	}
}

BEGIN_MESSAGE_MAP(CSageEjectDlg, CDialogExSized)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_CBN_SELCHANGE(IDC_DEVICE, &CSageEjectDlg::OnCbnSelchangeDisk)
	ON_WM_DESTROY()
	ON_MESSAGE(WM_PROCESS,&CSageEjectDlg::OnProcess)
	ON_WM_DEVICECHANGE()
	ON_WM_DROPFILES()
	ON_BN_CLICKED(IDC_OPEN, &CSageEjectDlg::OnBnClickedOpen)
END_MESSAGE_MAP()

// CSageEjectDlg message handlers

BOOL CSageEjectDlg::OnInitDialog()
{
	CDialogExSized::OnInitDialog();

	EnableToolTips();
	DragAcceptFiles();

	SetIcon( m_hIcon, TRUE );	// Set big icon
	SetIcon( m_hIcon, FALSE );	// Set small icon

	m_wndOpen.SetImage( (HICON)LoadImage( AfxGetResourceHandle(), MAKEINTRESOURCE( IDI_OPEN ), IMAGE_ICON, 16, 16, LR_SHARED ) );

	SetWindowText( AfxGetAppName() );

	m_pTaskbar = theApp.GetITaskbarList3();

	if ( ( GetKeyState( VK_SHIFT ) & 0x8000 ) == 0 )
	{
		RestoreWindowPlacement();
	}

	m_wndDevices.SetCueBanner( LoadString( IDS_SELECT ) );

	Enumerate( theApp.CommandLine.ToEject );

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CSageEjectDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogExSized::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags the minimized window.

HCURSOR CSageEjectDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CSageEjectDlg::OnOK()
{
	const int index = m_wndDevices.GetCurSel();
	if ( index != CB_ERR )
	{
		m_EjectTask.clear();

		Eject( m_wndDevices.GetItemData( index ) );
	}
}

LRESULT CSageEjectDlg::OnProcess(WPARAM /* ProcessType */ type, LPARAM number)
{
	static const CString ejecting = LoadString( IDS_EJECTING );
	static const CString flushing = LoadString( IDS_FLUSHING );
	CString title = AfxGetAppName();

	switch ( type )
	{
	case Ejecting:
		title.AppendFormat( ejecting, number );
		SetWindowText( title );
		return 0;

	case Flushing:
		title.AppendFormat( flushing, number );
		SetWindowText( title );
		return 0;
	}

	if ( m_Ejecting.joinable() )
	{
		if ( type == AutoEject )
		{
			// Already ejecting
			return 0;
		}

		CWaitCursor wc;

		// Wait for complete
		m_Ejecting.join();
	}

	SetWindowText( title );

	m_wndDevices.EnableWindow( TRUE );

	m_wndProgress.ShowWindow( SW_HIDE );
	m_wndProgress.SetMarquee( FALSE, 100 );

	if ( m_pTaskbar )
	{
		m_pTaskbar->SetProgressState( GetSafeHwnd(), TBPF_NOPROGRESS );
	}

	UpdateWindow();

	switch ( type )
	{
	case AutoEject:
	case Done:
		// Auto-eject first device from queue
		if ( ! m_EjectTask.empty() )
		{
			Eject( m_EjectTask.front() );

			m_EjectTask.pop_front();
		}
		else if ( type == Done || theApp.CommandLine.Auto )
		{
			// All disks ejected
			CDialogExSized::OnOK();
		}
		break;

	case Cancel:
		TRACE( _T("Failed to eject!\n") );

		Enumerate();
		break;
	}

	return 0;
}

BOOL CSageEjectDlg::OnDeviceChange(UINT /*nEventType*/, DWORD_PTR /*dwData*/)
{
	const DWORD nDrives = GetLogicalDrives();
	if ( m_nDrives != nDrives )
	{
		DWORD nMask = 1;
		for ( TCHAR i = _T('A'); i <= _T('Z'); ++i, nMask <<= 1 )
		{
			if ( ( ( m_nDrives ^ nDrives ) & nMask ) != 0 )
			{
				if ( ( m_nDrives & nMask ) == 0 )
				{
					TRACE( _T("Mounted disk: %c\n"), i );
				}
				else
				{
					TRACE( _T("Unmounted disk: %c\n"), i );
				}

				if ( ! m_Ejecting.joinable() )
				{
					Enumerate();
				}
			}
		}

		m_nDrives = nDrives;
	}
	return TRUE;
}

void CSageEjectDlg::OnDropFiles(HDROP hDropInfo)
{
	CDiskSet to_eject;

	UINT count = DragQueryFile( hDropInfo, 0xFFFFFFFF, nullptr, 0 );
	for ( UINT i = 0; i < count; ++i )
	{
		if ( UINT size = DragQueryFile( hDropInfo, i, nullptr, 0 ) )
		{
			CString file;
			size = DragQueryFile( hDropInfo, i, file.GetBuffer( size ), size + 1 );
			file.ReleaseBuffer();

			if ( _istalpha( file.GetAt( 0 ) ) && file.GetAt( 1 ) == _T(':') )
			{
				to_eject.emplace( file.Left( 2 ) );
			}
		}
	}

	DragFinish( hDropInfo );

	// Auto-eject drives containing dropped files
	if ( ! m_Ejecting.joinable() )
	{
		Enumerate( to_eject );
	}
}

void CSageEjectDlg::OnCbnSelchangeDisk()
{
	const int index = m_wndDevices.GetCurSel();
	if ( index != CB_ERR )
	{
		auto device = m_Devices.at( m_wndDevices.GetItemData( index ) );

		TRACE( _T("Selected: %s\n"), device->LogicalName().c_str() );

		m_sVolumeName = device->VolumeName().c_str();
		m_sID = device->HardwareID().c_str();
		m_sDOS = device->LogicalDosName().c_str();
		m_sPath = device->Path().c_str();
		m_sDisk.Empty();
		m_sName = device->Name().c_str();
		for ( const auto& disk : device->Disks() )
		{
			m_sDisk = disk->Path().c_str();
			m_sName = disk->Name().c_str();
		}

		UpdateData( FALSE );

		m_wndEject.EnableWindow( device->LogicalType() == DRIVE_REMOVABLE || device->LogicalType() == DRIVE_CDROM );
		m_wndOpen.EnableWindow( ! device->LogicalName().empty() );
	}
	else
	{
		// Nothing
		m_wndEject.EnableWindow( FALSE );
		m_wndOpen.EnableWindow( FALSE );
	}

	UpdateWindow();
}

void CSageEjectDlg::OnDestroy()
{
	if ( m_Ejecting.joinable() )
	{
		CWaitCursor wc;

		m_Cancel = true;
		m_Ejecting.join();
	}

	if ( m_pTaskbar )
	{
		m_pTaskbar = nullptr;
		theApp.ReleaseTaskBarRefs();
	}

	CDialogExSized::OnDestroy();
}

void CSageEjectDlg::OnCancel()
{
	if ( m_Ejecting.joinable() )
	{
		m_Cancel = true;
	}
	else
	{
		CDialogExSized::OnCancel();
	}
}

void CSageEjectDlg::OnBnClickedOpen()
{
	const int index = m_wndDevices.GetCurSel();
	if ( index != CB_ERR )
	{
		auto device = m_Devices.at( m_wndDevices.GetItemData( index ) );

		if ( ! device->LogicalName().empty() )
		{
			ShellExecute( GetSafeHwnd(), _T("open"), device->LogicalName().c_str(), nullptr, nullptr, SW_SHOWNORMAL );
		}
	}
}
