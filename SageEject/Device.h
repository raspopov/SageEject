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

#include "DeviceClass.h"


class Device
{
public:
	Device() noexcept
		: m_Class		( nullptr )
		, m_Info		()
		, m_Capabilities()
	{
	}

	Device(IDeviceClassPtr device_class, const SP_DEVINFO_DATA& info, const std::wstring& path)
		: m_Class		( device_class )
		, m_Info		( info )
		, m_Path		( path )
		, m_Description ( device_class->GetProperty( &info, SPDRP_DEVICEDESC, std::wstring() ) )
		, m_HardwareID	( device_class->GetProperty( &info, SPDRP_HARDWAREID, std::wstring() ) )
		, m_Name		( device_class->GetProperty( &info, SPDRP_FRIENDLYNAME, std::wstring() ) )
		, m_Capabilities( device_class->GetProperty( &info, SPDRP_CAPABILITIES, 0 ) )
	{
	}

	virtual bool Eject() const
	{
		TRACE( _T("Ejecting: %s ...\n"), m_HardwareID.c_str() );

		PNP_VETO_TYPE veto = {};
		TCHAR veto_name[ MAX_PATH ] = {};
		CONFIGRET res = CM_Request_Device_Eject( Instance(), &veto, veto_name, _countof( veto_name ), 0 );
		if ( res == CR_SUCCESS )
		{
			return true;
		}

		TRACE( _T("CM_Request_Device_Eject error: 0x%x Veto: %s\n"), res, veto_name );

		//veto_name[ 0 ] = 0;
		//res = CM_Query_And_Remove_SubTree( Instance(), &veto, veto_name, _countof( veto_name ), CM_REMOVE_UI_NOT_OK );
		//if ( res == CR_SUCCESS )
		//{
		//	return true;
		//}
		//TRACE( _T("CM_Query_And_Remove_SubTree error: 0x%x Veto: %s\n"), res, veto_name );

		return false;
	}

	inline DEVINST Instance() const noexcept
	{
		return m_Info.DevInst;
	}

	inline const std::wstring& Path() const noexcept
	{
		return m_Path;
	}

	inline const std::wstring& HardwareID() const noexcept
	{
		return m_HardwareID;
	}

	inline const std::wstring& Description() const noexcept
	{
		return m_Description;
	}

	inline const std::wstring& Name() const noexcept
	{
		return m_Name;
	}

	inline bool IsRemovable() const noexcept
	{
		return ( ( m_Capabilities & CM_DEVCAP_REMOVABLE ) != 0 );
	}

protected:
	IDeviceClassPtr		m_Class;

private:
	SP_DEVINFO_DATA		m_Info;
	std::wstring		m_Path;
	std::wstring		m_HardwareID;
	std::wstring		m_Description;
	std::wstring		m_Name;
	DWORD				m_Capabilities;
};
