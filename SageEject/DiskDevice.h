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

#include "Device.h"


class DiskDevice : public Device, public std::enable_shared_from_this< DiskDevice >
{
public:
	using Ptr = std::shared_ptr< const DiskDevice >;

	DiskDevice() noexcept
		: m_Number	( { (ULONG)-1, (ULONG)-1, (ULONG)-1 } )
	{
	}

	DiskDevice(IDeviceClassPtr device_class, const SP_DEVINFO_DATA& info, const std::wstring& path)
		: Device	( device_class, info, path )
		, m_Number	( { (ULONG)-1, (ULONG)-1, (ULONG)-1 } )
	{
		HANDLE drive = CreateFile( Path().c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr );
		if ( drive != INVALID_HANDLE_VALUE )
		{
			DWORD returned = 0;
			DeviceIoControl( drive, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &m_Number, sizeof( m_Number ), &returned, nullptr );
			CloseHandle( drive );
		}
	}

	inline DWORD DeviceType() const noexcept
	{
		return m_Number.DeviceType;
	}

	inline DWORD DeviceNumber() const noexcept
	{
		return m_Number.DeviceNumber;
	}

	inline DWORD PartitionNumber() const noexcept
	{
		return m_Number.PartitionNumber;
	}

	Ptr Parent() const
	{
		if ( DEVINST parent = m_Class->Parent( Instance() ) )
		{
			SP_DEVINFO_DATA info = {};
			if ( m_Class->DeviceInfo( m_Class->DeviceID( parent ), info ) )
			{
				return std::make_shared< DiskDevice >( m_Class, info, std::wstring() );
			}
		}
		return nullptr;
	}

	// Retrieve a removable device itself or removable parent device if any
	Ptr RemovableDevice() const
	{
		if ( IsRemovable() )
		{
			return shared_from_this();
		}
		else if ( const auto& parent = Parent() )
		{
			return parent->RemovableDevice();
		}
		return nullptr;
	}

private:
	STORAGE_DEVICE_NUMBER	m_Number;
};


class DiskDeviceClass : public DeviceClass< DiskDevice >
{
public:
	DiskDeviceClass()
		: DeviceClass< DiskDevice >( GUID_DEVINTERFACE_DISK )
	{
	}

	static inline auto Create()
	{
		return std::make_shared< DiskDeviceClass >();
	}
};
