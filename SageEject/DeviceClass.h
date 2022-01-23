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

class IDeviceClass : public std::enable_shared_from_this< IDeviceClass >
{
public:
	virtual ~IDeviceClass() {}

	virtual DEVINST Parent(DEVINST device) const = 0;
	virtual std::wstring DeviceID(DEVINST device) const = 0;
	virtual bool DeviceInfo(const std::wstring& id, SP_DEVINFO_DATA& info) const = 0;
	virtual std::wstring InstanceID(const SP_DEVINFO_DATA* devData) const = 0;
	virtual std::wstring GetProperty(const SP_DEVINFO_DATA* devData, DWORD property, const std::wstring& defaultValue) const = 0;
    virtual DWORD GetProperty(const SP_DEVINFO_DATA* devData, DWORD property, DWORD defaultValue) const = 0;
	virtual GUID GetProperty(const SP_DEVINFO_DATA* devData, int property, const GUID& defaultValue) const = 0;
};

using IDeviceClassPtr = std::shared_ptr< IDeviceClass >;


template< class T >
class DeviceClass : public IDeviceClass
{
public:
	using Ptr = std::shared_ptr< T >;

	virtual ~DeviceClass()
	{
		if ( m_InfoSet != INVALID_HANDLE_VALUE )
		{
			SetupDiDestroyDeviceInfoList( m_InfoSet );
			m_InfoSet = INVALID_HANDLE_VALUE;
		}
	}

	inline operator bool() const noexcept
	{
		return ( m_InfoSet != INVALID_HANDLE_VALUE );
	}

	std::vector< Ptr > Devices()
	{
		std::vector< Ptr > devices;

		if ( m_InfoSet != INVALID_HANDLE_VALUE )
		{
			for ( DWORD index = 0; ; ++index )
			{
				SP_DEVICE_INTERFACE_DATA interfaceData = { sizeof( SP_DEVICE_INTERFACE_DATA ) };
				if ( ! SetupDiEnumDeviceInterfaces( m_InfoSet, nullptr, &m_Guid, index, &interfaceData ) )
				{
					break;
				}

				if ( auto device = CreateDevice( &interfaceData ) )
				{
					devices.emplace_back( device );
				}
			}
		}

		return devices;
	}

	DEVINST Parent(DEVINST device) const override
	{
		DEVINST parentDevInst;
		CONFIGRET res = CM_Get_Parent( &parentDevInst, device, 0 );
		if ( res == CR_SUCCESS )
		{
			return parentDevInst;
		}
		return 0;
	}

	std::wstring DeviceID(DEVINST device) const override
	{
		DWORD size = 0;
		CONFIGRET res = CM_Get_Device_ID_Size( &size, device, 0 );
		if ( res == CR_SUCCESS )
		{
			auto buffer = std::make_unique< wchar_t[] >( size );
			res = CM_Get_Device_ID( device, buffer.get(), size, 0 );
			if ( res == CR_SUCCESS )
			{
				return std::wstring( buffer.get(), size );
			}
		}
		return std::wstring();
	}

	bool DeviceInfo(const std::wstring& id, SP_DEVINFO_DATA& info) const override
	{
		info.cbSize = sizeof( SP_DEVINFO_DATA );
		return SetupDiOpenDeviceInfo( m_InfoSet, id.c_str(), nullptr, 0, &info );
	}

	std::wstring InstanceID(const SP_DEVINFO_DATA* devData) const override
    {
        DWORD size = 0;
		if ( SetupDiGetDeviceInstanceId( m_InfoSet, const_cast< SP_DEVINFO_DATA* >( devData ), nullptr, 0, &size ) ||
			 GetLastError() == ERROR_INSUFFICIENT_BUFFER )
		{
			auto buffer = std::make_unique< wchar_t[] >( size );
			if ( SetupDiGetDeviceInstanceId( m_InfoSet, const_cast< SP_DEVINFO_DATA* >( devData ), buffer.get(), size, &size ) )
			{
				return std::wstring( buffer.get(), size );
			}
		}
		return std::wstring();
    }

	std::wstring GetProperty(const SP_DEVINFO_DATA* devData, DWORD property, const std::wstring& defaultValue) const override
    {
        DWORD propertyBufferSize = 0;
		if ( SetupDiGetDeviceRegistryProperty( m_InfoSet, const_cast< SP_DEVINFO_DATA* >( devData ), property, nullptr,
			 nullptr, 0, &propertyBufferSize ) || GetLastError() == ERROR_INSUFFICIENT_BUFFER )
		{
			auto propertyBuffer = std::make_unique< char[] >( propertyBufferSize );
			if ( SetupDiGetDeviceRegistryProperty( m_InfoSet, const_cast< SP_DEVINFO_DATA* >( devData ), property, nullptr,
				 reinterpret_cast< PBYTE >( propertyBuffer.get() ), propertyBufferSize, &propertyBufferSize ) &&
				propertyBufferSize >= sizeof( wchar_t ) )
			{
				return std::wstring( reinterpret_cast< wchar_t* >( propertyBuffer.get() ), propertyBufferSize / sizeof( wchar_t ) - 1 );
			}
		}
		return defaultValue;
    }

    DWORD GetProperty(const SP_DEVINFO_DATA* devData, DWORD property, DWORD defaultValue) const override
    {
		DWORD propertyValue = 0;
		if ( SetupDiGetDeviceRegistryProperty( m_InfoSet, const_cast< SP_DEVINFO_DATA* >( devData ), property, nullptr,
			 reinterpret_cast< PBYTE >( &propertyValue ), sizeof( propertyValue ), nullptr ) )
		{
			return propertyValue;
		}
		return defaultValue;
    }

	GUID GetProperty(const SP_DEVINFO_DATA* devData, int property, const GUID& defaultValue) const override
    {
		GUID propertyValue = {};
		if ( SetupDiGetDeviceRegistryProperty( m_InfoSet, const_cast< SP_DEVINFO_DATA* >( devData ), property, nullptr,
			 reinterpret_cast< PBYTE >( &propertyValue ), sizeof( propertyValue ), nullptr ) )
		{
			return propertyValue;
		}
        return defaultValue;
    }

protected:
	DeviceClass(const GUID& guid)
		: m_Guid	( guid )
		, m_InfoSet	( INVALID_HANDLE_VALUE )
	{
		m_InfoSet = SetupDiGetClassDevs( &m_Guid, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT );
	}

private:
	GUID		m_Guid;
	HDEVINFO	m_InfoSet;

	Ptr CreateDevice(SP_DEVICE_INTERFACE_DATA* pinterfaceData)
	{
		DWORD detailBufferSize = 0;
		if ( SetupDiGetDeviceInterfaceDetail( m_InfoSet, pinterfaceData, nullptr, 0, &detailBufferSize, nullptr ) ||
			 GetLastError() == ERROR_INSUFFICIENT_BUFFER )
		{
			auto detailBuffer = std::make_unique< char[] >( detailBufferSize );
			auto detailData = reinterpret_cast< PSP_DEVICE_INTERFACE_DETAIL_DATA >( detailBuffer.get() );
			detailData->cbSize = sizeof( SP_DEVICE_INTERFACE_DETAIL_DATA );
			SP_DEVINFO_DATA info = { sizeof( SP_DEVINFO_DATA ) };
			if ( SetupDiGetDeviceInterfaceDetail( m_InfoSet, pinterfaceData, detailData, detailBufferSize, &detailBufferSize, &info ) )
			{
				return std::make_shared< T >( shared_from_this(), info, detailData->DevicePath );
			}
		}

		return nullptr;
	}
};
