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

#include "Scenario1_Discovery.g.h"
#include "MainPage.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Devices::Enumeration;

namespace winrt::SDKTemplate::implementation
{
    struct Scenario1_Discovery : Scenario1_DiscoveryT<Scenario1_Discovery>
    {
        Scenario1_Discovery();

        void OnNavigatedFrom(NavigationEventArgs const& e);

        IObservableVector<IInspectable> KnownDevices()
        {
            return knownDevices.as<IObservableVector<IInspectable>>();
        }

        void EnumerateButton_Click(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget PairButton_Click(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        bool Not(bool value) { return !value; }

    private:
        SDKTemplate::MainPage rootPage{ MainPage::Current() };
        IObservableVector<SDKTemplate::BluetoothLEDeviceDisplay> knownDevices = single_threaded_observable_vector<SDKTemplate::BluetoothLEDeviceDisplay>();
        DeviceWatcher deviceWatcher{ nullptr };
        event_token deviceWatcherAddedToken;
        event_token deviceWatcherUpdatedToken;
        event_token deviceWatcherRemovedToken;
        event_token deviceWatcherEnumerationCompletedToken;
        event_token deviceWatcherStoppedToken;

        void StartBleDeviceWatcher();
        void StopBleDeviceWatcher();
        SDKTemplate::BluetoothLEDeviceDisplay FindBluetoothLEDeviceDisplay(hstring const& id);
        uint32_t FindBluetoothLEDeviceDisplayIndex(hstring const& id);
        static constexpr uint32_t invalid_vector_index = 0U - 1U;

        fire_and_forget DeviceWatcher_Added(DeviceWatcher sender, DeviceInformation deviceInfo);
        fire_and_forget DeviceWatcher_Updated(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate);
        fire_and_forget DeviceWatcher_Removed(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate);
        fire_and_forget DeviceWatcher_EnumerationCompleted(DeviceWatcher sender, IInspectable const&);
        fire_and_forget DeviceWatcher_Stopped(DeviceWatcher sender, IInspectable const&);
    };
}

namespace winrt::SDKTemplate::factory_implementation
{
    struct Scenario1_Discovery : Scenario1_DiscoveryT<Scenario1_Discovery, implementation::Scenario1_Discovery>
    {
    };
}
