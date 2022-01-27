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

#include "DiskDevice.h"


class Volume : public DiskDevice
{
public:
	Volume() noexcept
		: m_LogicalType ( DRIVE_UNKNOWN )
		, m_Length		()
	{
	}

	Volume(IDeviceClassPtr device_class, const SP_DEVINFO_DATA& info, const std::wstring& path)
		: DiskDevice	( device_class, info, path )
		, m_LogicalType	( DRIVE_UNKNOWN )
		, m_Length		()
	{
		// Retrieve volume name
		WCHAR name[ MAX_PATH ] = {};
		if ( GetVolumeNameForVolumeMountPoint( ( path + L"\\" ).c_str(), name, _countof( name ) ) )
		{
			m_VolumeName = name;

			WCHAR v_title[ MAX_PATH ] = {}, v_fs[ MAX_PATH ] = {};
			if ( GetVolumeInformation( name, v_title, MAX_PATH, nullptr, nullptr, nullptr, v_fs, MAX_PATH ) )
			{
				m_LogicalTitle = v_title;
				m_LogicalFS = v_fs;
			}

			if ( ! GetDiskFreeSpaceEx( name, nullptr, &m_Length, nullptr ) )
			{
				TRACE( _T("GetDiskFreeSpaceEx(%s) error: %d\n"), name, GetLastError() );
			}

			WCHAR dos_name[ MAX_PATH ] = {};
			name[ wcslen( name ) - 1 ] = 0;
			if ( QueryDosDevice( &name[ 4 ], dos_name, MAX_PATH ) )
			{
				m_LogicalDosName = dos_name;
			}

			// Enumerate drives
			WCHAR drives[ 100 ] = {};
			if ( GetLogicalDriveStrings( _countof( drives ), drives ) <= _countof( drives ) )
			{
				for ( LPCWSTR drive = drives; *drive; drive += wcslen( drive ) + 1 )
				{
					// Retrieve volume name
					WCHAR point[ MAX_PATH ] = {};
					if ( GetVolumeNameForVolumeMountPoint( drive, point, _countof( point ) ) && m_VolumeName == point )
					{
						m_LogicalName = drive;
						m_LogicalType = GetDriveType( drive );
						break;
					}
				}
			}
		}
	}

	inline ULONGLONG Length() const noexcept
	{
		return m_Length.QuadPart;
	}

	inline const std::wstring& VolumeName() const noexcept
	{
		return m_VolumeName;
	}

	inline const std::wstring& LogicalName() const noexcept
	{
		return m_LogicalName;
	}

	inline DWORD LogicalType() const noexcept
	{
		return m_LogicalType;
	}

	inline const std::wstring& LogicalFS() const noexcept
	{
		return m_LogicalFS;
	}

	inline const std::wstring& LogicalTitle() const noexcept
	{
		return m_LogicalTitle;
	}

	inline const std::wstring& LogicalDosName() const noexcept
	{
		return m_LogicalDosName;
	}

	std::vector< Ptr > Disks() const
	{
		std::vector< Ptr > disks;

		if ( const auto& device_class = DiskDeviceClass::Create() )
		{
			for ( const auto& device : device_class->Devices() )
			{
				if ( device->DeviceNumber() == DeviceNumber() && DeviceType() == FILE_DEVICE_DISK && PartitionNumber() != (UINT)-1 )
				{
					disks.emplace_back( device );
				}
			}
		}

		return disks;
	}

	void Flush()
	{
		// Open logical disk
		std::wstring full_name = L"\\\\.\\" + LogicalName();
		while ( full_name.back() == L'\\' )
		{
			full_name.pop_back();
		}

		TRACE( _T("Flushing: %s ...\n"), full_name.c_str() );

		// Write access first (no buffering!)
		HANDLE drive = CreateFile( full_name.c_str(), GENERIC_READ | GENERIC_WRITE | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr );
		if ( drive == INVALID_HANDLE_VALUE )
		{
			TRACE( _T("Failed to get write access: %d\n"), GetLastError() );

			// Read access second (no buffering!)
			drive = CreateFile( full_name.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
				nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr );
		}
		if ( drive != INVALID_HANDLE_VALUE )
		{
			// Flushing (requires GENERIC_WRITE access above)
			if ( ! FlushFileBuffers( drive ) )
			{
				TRACE( _T("FlushFileBuffers error: %d\n"), GetLastError() );
			}

			// Unlocking media
			PREVENT_MEDIA_REMOVAL rem = { FALSE };
			DWORD returned = 0;
			if ( ! DeviceIoControl( drive, IOCTL_STORAGE_MEDIA_REMOVAL, &rem, sizeof( rem ), nullptr, 0, &returned, nullptr ) )
			{
				TRACE( _T("IOCTL_STORAGE_MEDIA_REMOVAL error: %d\n"), GetLastError() );
			}

			// Unlocking ejection
			// DeviceIoControl( drive, IOCTL_STORAGE_EJECTION_CONTROL, &rem, sizeof( rem ), nullptr, 0, &returned, nullptr );

			// Locking volume
			// DeviceIoControl( drive, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &returned, nullptr );

			CloseHandle( drive );
		}
		else
		{
			TRACE( _T("CreateFile( %s ) error: %d\n"), full_name.c_str(), GetLastError() );
		}
	}

	bool Eject() const override
	{
		bool res = true;

		if ( m_LogicalType == DRIVE_CDROM )
		{
			// Open logical disk
			std::wstring full_name = L"\\\\.\\" + LogicalName();
			while ( full_name.back() == L'\\' )
			{
				full_name.pop_back();
			}

			HANDLE drive = CreateFile( full_name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr );
			if ( drive != INVALID_HANDLE_VALUE )
			{
				// Ejecting media
				DWORD returned = 0;
				if ( ! DeviceIoControl( drive, IOCTL_DISK_EJECT_MEDIA, nullptr, 0, nullptr, 0, &returned, nullptr ) )
				{
					TRACE( _T("IOCTL_DISK_EJECT_MEDIA( %s ) error: %d\n"), full_name.c_str(), GetLastError() );
					res = false;
				}

				CloseHandle( drive );
			}
			else
			{
				TRACE( _T("CreateFile( %s ) error: %d\n"), full_name.c_str(), GetLastError() );
			}
		}
		else
		{
			// Eject all devices of disks of current volume
			for ( const auto& disk : Disks() )
			{
				if ( auto removable = disk->RemovableDevice() )
				{
					if ( ! removable->Eject() )
					{
						res = false;
					}
				}
			}
		}

		return res;
	}

private:
	std::wstring	m_VolumeName;
	std::wstring	m_LogicalName;
	DWORD			m_LogicalType;
	std::wstring	m_LogicalFS;
	std::wstring	m_LogicalTitle;
	std::wstring	m_LogicalDosName;
	ULARGE_INTEGER	m_Length;
};


class VolumeDeviceClass : public DeviceClass< Volume >
{
public:
	VolumeDeviceClass()
		: DeviceClass< Volume >( GUID_DEVINTERFACE_VOLUME )
	{
	}

	static inline auto Create()
	{
		return std::make_shared< VolumeDeviceClass >();
	}
};
