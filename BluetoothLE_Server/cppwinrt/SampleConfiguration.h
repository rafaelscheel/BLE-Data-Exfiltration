//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once
#include "pch.h"

namespace winrt::SDKTemplate
{
    // This function is called by App::OnBackgroundActivated in the samples template
    void App_OnBackgroundActivated(Windows::ApplicationModel::Activation::BackgroundActivatedEventArgs const&);

    struct Constants
    {
        static constexpr guid CalculatorServiceUuid{ L"caecface-e1d9-11e6-bf01-fe55135034f0" };
        static constexpr guid BackgroundCalculatorServiceUuid{ L"caecface-e1d9-11e6-bf01-fe55135034f5" };

        static constexpr guid Operand1CharacteristicUuid{ L"caec2ebc-e1d9-11e6-bf01-fe55135034f1" };
        static constexpr guid Operand2CharacteristicUuid{ L"caec2ebc-e1d9-11e6-bf01-fe55135034f2" };
        static constexpr guid OperatorCharacteristicUuid{ L"caec2ebc-e1d9-11e6-bf01-fe55135034f3" };
        static constexpr guid ResultCharacteristicUuid  { L"caec2ebc-e1d9-11e6-bf01-fe55135034f4" };

        static constexpr guid BackgroundOperand1CharacteristicUuid{ L"caec2ebc-e1d9-11e6-bf01-fe55135034f6" };
        static constexpr guid BackgroundOperand2CharacteristicUuid{ L"caec2ebc-e1d9-11e6-bf01-fe55135034f7" };
        static constexpr guid BackgroundOperatorCharacteristicUuid{ L"caec2ebc-e1d9-11e6-bf01-fe55135034f8" };
        static constexpr guid BackgroundResultCharacteristicUuid  { L"caec2ebc-e1d9-11e6-bf01-fe55135034f9" };

        static Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristicParameters gattOperand1Parameters();
        static Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristicParameters gattOperand2Parameters();
        static Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristicParameters gattOperatorParameters();
        static Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristicParameters gattResultParameters();

        static constexpr wchar_t CalculatorContainerName[] = L"Calculator";
    };

    struct SampleState
    {
        static hstring SelectedBleDeviceId;
        static hstring SelectedBleDeviceName;
    };

    Windows::Foundation::Rect GetElementRect(Windows::UI::Xaml::FrameworkElement const& element);

    struct FeatureDetection
    {
        // Reports whether the extended advertising and scanning features are supported:
        //
        // BluetoothAdapter.IsLowEnergyUncoded2MPhySupported property
        // BluetoothAdapter.IsLowEnergyCodedPhySupported property
        // BluetoothLEAdvertisementReceivedEventArgs.PrimaryPhy property
        // BluetoothLEAdvertisementReceivedEventArgs.SecondaryPhy property
        // BluetoothLEAdvertisementPublisher.PrimaryPhy property
        // BluetoothLEAdvertisementPublisher.SecondaryPhy property
        // BluetoothLEAdvertisementPublisherTrigger.PrimaryPhy property
        // BluetoothLEAdvertisementPublisherTrigger.SecondaryPhy property
        // BluetoothLEAdvertisementScanParameters class
        // BluetoothLEAdvertisementWatcher.UseUncoded1MPhy property
        // BluetoothLEAdvertisementWatcher.UseCodedPhy property
        // BluetoothLEAdvertisementWatcher.ScanParameters property
        // BluetoothLEAdvertisementWatcher.UseHardwareFilter property
        // BluetoothLEAdvertisementWatcherTrigger.UseUncoded1MPhy property
        // BluetoothLEAdvertisementWatcherTrigger.UseCodedPhy property
        // BluetoothLEAdvertisementWatcherTrigger.ScanParameters property
        // GattServiceProvider.UpdateAdvertisingParameters method
        // GattServiceProviderConnection.UpdateAdvertisingParameters method
        // GattServiceProviderAdvertisingParameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy property
        // GattServiceProviderAdvertisingParameters.UseLowEnergyUncoded2MPhyAsSecondaryPhy property
        //
        // All of these features are available as a group.
        //
        static bool AreExtendedAdvertisingPhysAndScanParametersSupported();
    };

    struct BufferHelpers
    {
        template<typename T>
        static std::optional<T> FromBuffer(Windows::Storage::Streams::IBuffer const& buffer)
        {
            static_assert(std::is_trivially_copyable_v <T>, "FromBuffer can be used only with memcpy'able types");
            uint32_t length = buffer.Length();
            if (length == sizeof(T))
            {
                T result;
                memcpy_s(&result, sizeof(result), buffer.data(), length);
                return result;
            }
            return std::nullopt;
        }

        template<typename T>
        static Windows::Storage::Streams::IBuffer ToBuffer(T const& value)
        {
            static_assert(std::is_trivially_copyable_v <T>, "ToBuffer can be used only with memcpy'able types");
            Windows::Storage::Streams::Buffer buffer(sizeof(value));
            memcpy_s(buffer.data(), sizeof(value), &value, sizeof(value));
            buffer.Length(sizeof(value));
            return buffer;
        }
    };

    struct Utilities
    {
        static hstring FormatGattCommunicationStatus(Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus status, winrt::Windows::Foundation::IReference<uint8_t> protocolError);
    };

    /// <summary>
    /// The DeferralCompleter holds a deferral and completes it at destruction.
    /// </summary>
    template<typename T>
    struct DeferralCompleter
    {
        DeferralCompleter(T const& deferral) : m_deferral(deferral) {}
        ~DeferralCompleter() { m_deferral.Complete(); }
        T m_deferral;
    };
}

namespace winrt
{
    hstring to_hstring(Windows::Devices::Bluetooth::BluetoothError error);
    hstring to_hstring(Windows::Devices::Bluetooth::GenericAttributeProfile::GattServiceProviderAdvertisementStatus status);
}
