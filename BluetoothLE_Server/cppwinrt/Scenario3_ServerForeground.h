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

#include "Scenario3_ServerForeground.g.h"
#include "MainPage.h"
#include "SampleConfiguration.h"

namespace winrt::SDKTemplate::implementation
{
    struct Scenario3_ServerForeground : Scenario3_ServerForegroundT<Scenario3_ServerForeground>
    {
        Scenario3_ServerForeground();

        fire_and_forget OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs const&);
        void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs const& e);

        fire_and_forget PublishButton_Click(IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
        void Publishing2MPHY_Click(IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);

    private:
        enum class CalculatorCharacteristics
        {
            Operand1 = 1,
            Operand2 = 2,
            Operator = 3,
        };

        enum class CalculatorOperators
        {
            Add = 1,
            Subtract = 2,
            Multiply = 3,
            Divide = 4,
        };

        bool IsValidOperator(CalculatorOperators op)
        {
            return op >= CalculatorOperators::Add && op <= CalculatorOperators::Divide;
        }

        SDKTemplate::MainPage rootPage{ MainPage::Current() };

        // Managing the service.
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattServiceProvider serviceProvider{ nullptr };
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic operand1Characteristic{ nullptr };
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic operand2Characteristic{ nullptr };
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic operatorCharacteristic{ nullptr };
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic resultCharacteristic{ nullptr };
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattServiceProviderAdvertisingParameters advertisingParameters;

        // Implementing the service.
        int operand1Value = 0;
        int operand2Value = 0;
        CalculatorOperators operatorValue{};
        int resultValue = 0;

        bool navigatedTo = false;
        bool startingService = false; // reentrancy protection

        event_token operand1CharacteristicWriteToken{};
        event_token operand2CharacteristicWriteToken{};
        event_token operatorCharacteristicWriteToken{};
        event_token resultCharacteristicReadToken{};
        event_token resultCharacteristicClientsChangedToken{};
        event_token serviceProviderAdvertisementChangedToken{};

        void UnsubscribeServiceEvents();
        void UpdateUI();
        Windows::Foundation::IAsyncAction CreateAndAdvertiseServiceAsync();
        void ResultCharacteristic_SubscribedClientsChanged(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, IInspectable const& args);
        void ServiceProvider_AdvertisementStatusChanged(Windows::Devices::Bluetooth::GenericAttributeProfile::GattServiceProvider const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattServiceProviderAdvertisementStatusChangedEventArgs const& args);
        fire_and_forget ResultCharacteristic_ReadRequestedAsync(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadRequestedEventArgs args);
        void ComputeResult();
        fire_and_forget NotifyClientDevices(int computedValue);
        fire_and_forget Op1Characteristic_WriteRequestedAsync(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteRequestedEventArgs args);
        fire_and_forget Op2Characteristic_WriteRequestedAsync(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteRequestedEventArgs args);
        fire_and_forget OperatorCharacteristic_WriteRequestedAsync(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteRequestedEventArgs args);
        fire_and_forget ProcessWriteCharacteristic(Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteRequest const& request, CalculatorCharacteristics opCode);
    };
}

namespace winrt::SDKTemplate::factory_implementation
{
    struct Scenario3_ServerForeground : Scenario3_ServerForegroundT<Scenario3_ServerForeground, implementation::Scenario3_ServerForeground>
    {
    };
}
