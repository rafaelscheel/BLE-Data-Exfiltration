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

#include "Scenario2_Client.g.h"
#include "MainPage.h"
#include "SampleConfiguration.h"

namespace winrt::SDKTemplate::implementation
{
    struct Scenario2_Client : Scenario2_ClientT<Scenario2_Client>
    {
        Scenario2_Client();

        void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs const& e);
        fire_and_forget OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs const&);

        fire_and_forget ConnectButton_Click(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget ServiceList_SelectionChanged(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget CharacteristicList_SelectionChanged(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget CharacteristicReadButton_Click(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget CharacteristicWriteButton_Click(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget CharacteristicWriteButtonInt_Click(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget ValueChangedSubscribeToggle_Click(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);

    private:
        SDKTemplate::MainPage rootPage{ MainPage::Current() };
        Windows::Devices::Bluetooth::BluetoothLEDevice bluetoothLEDevice{ nullptr };
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic selectedCharacteristic{ nullptr };

        // Only one registered characteristic at a time.
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic registeredCharacteristic{ nullptr };

        event_token notificationsToken{};

        Windows::Foundation::IAsyncAction ClearBluetoothLEDeviceAsync();
        void AddValueChangedHandler();
        void RemoveValueChangedHandler();
        void EnableCharacteristicPanels(Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties properties);
        Windows::Foundation::IAsyncOperation<bool> WriteBufferToSelectedCharacteristicAsync(Windows::Storage::Streams::IBuffer buffer);
        hstring FormatValueByPresentation(
            Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic const& characteristic,
            Windows::Storage::Streams::IBuffer const& buffer);
        fire_and_forget Characteristic_ValueChanged(Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattValueChangedEventArgs args);
        static uint16_t ParseHeartRateValue(Windows::Storage::Streams::IBuffer const& buffer);
    };
}

namespace winrt::SDKTemplate::factory_implementation
{
    struct Scenario2_Client : Scenario2_ClientT<Scenario2_Client, implementation::Scenario2_Client>
    {
    };
}
