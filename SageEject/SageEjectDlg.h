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

#pragma once

#include "DialogExSized.h"
#include "Volume.h"

using CDeviceSet = std::vector< VolumeDeviceClass::Ptr >;
using CTaskSet = std::list< size_t >;

// CSageEjectDlg dialog

class CSageEjectDlg : public CDialogExSized
{
	DECLARE_DYNAMIC(CSageEjectDlg)

public:
	CSageEjectDlg(CWnd* pParent = nullptr);	// standard constructor

	enum { IDD = IDD_SAGEEJECT_DIALOG };

protected:
	HICON			m_hIcon;
	CComboBox		m_wndDevices;
	CMFCButton		m_wndOpen;
	CButton			m_wndEject;
	CProgressCtrl	m_wndProgress;
	CString			m_sVolumeName;
	CString			m_sDOS;
	CString			m_sID;
	CString			m_sPath;
	CString			m_sDisk;
	CString			m_sName;

	bool			m_Cancel;		// Cancel flag
	std::thread		m_Ejecting;		// Ejecting thread

	DWORD			m_nDrives;		// Current map of drives (bits)
	ITaskbarList3*	m_pTaskbar;
	CDeviceSet		m_Devices;
	CTaskSet		m_EjectTask;

	bool Enumerate(const CDiskSet& to_eject = CDiskSet());
	void Eject(size_t index);
	void StopServices();

	void DoDataExchange(CDataExchange* pDX) override;	// DDX/DDV support
	BOOL OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override;

	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnCbnSelchangeDisk();
	afx_msg void OnDestroy();
	afx_msg LRESULT OnProcess(WPARAM /* ProcessType */ type, LPARAM number);
	afx_msg BOOL OnDeviceChange(UINT nEventType, DWORD_PTR dwData);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnBnClickedOpen();

	DECLARE_MESSAGE_MAP()
};

#define WM_PROCESS	( WM_APP + 1 )

enum ProcessType { AutoEject, Ejecting, Flushing, Done, Cancel };

inline CString LoadString(UINT id)
{
	CString str;
	str.LoadString( id );
	return str;
}
