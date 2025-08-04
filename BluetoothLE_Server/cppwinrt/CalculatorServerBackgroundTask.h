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

#include "CalculatorServerBackgroundTask.g.h"

namespace winrt::SDKTemplate::implementation
{
    enum class CalculatorCharacteristics
    {
        Operand1 = 1,
        Operand2 = 2,
        Operator = 3
    };

    enum class CalculatorOperators
    {
        Invalid = 0,
        Add = 1,
        Subtract = 2,
        Multiply = 3,
        Divide = 4
    };

    inline bool IsValidEnumValue(CalculatorOperators op)
    {
        switch (op)
        {
        case CalculatorOperators::Add:
        case CalculatorOperators::Subtract:
        case CalculatorOperators::Multiply:
        case CalculatorOperators::Divide:
            return true;
        default:
            return false;
        }
    }

    struct CalculatorServerBackgroundTask : CalculatorServerBackgroundTaskT<CalculatorServerBackgroundTask>
    {
        CalculatorServerBackgroundTask() = default;

        void Run(Windows::ApplicationModel::Background::IBackgroundTaskInstance const& taskInstance);

        static event<delegate<>> ValuesChanged;

    private:
        bool shouldAdvertise2MPhy = false;
        Windows::ApplicationModel::Background::BackgroundTaskDeferral taskDeferral = nullptr;
        Windows::ApplicationModel::Background::IBackgroundTaskInstance taskInstance = nullptr;

        Windows::Devices::Bluetooth::Background::GattServiceProviderConnection serviceConnection = nullptr;

        Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic operand1Characteristic = nullptr;
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic operand2Characteristic = nullptr;
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic operatorCharacteristic = nullptr;
        Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic resultCharacteristic = nullptr;

        int operand1Received = 0;
        int operand2Received = 0;
        CalculatorOperators operatorReceived = CalculatorOperators::Invalid;
        int resultVal = 0;

        Windows::Foundation::Collections::IPropertySet calculatorValues;

        // Needed for in-process servers. (For out-of-process servers, the background infrastructure
        // keeps the service object alive until it is shut down.)
        IInspectable keepServiceAlive = nullptr;

        event_token operand1CharacteristicWriteToken{};
        event_token operand2CharacteristicWriteToken{};
        event_token operatorCharacteristicWriteToken{};
        event_token resultCharacteristicReadToken{};
        event_token resultCharacteristicChangedToken{};

        void InitializeService();
        void CompleteTask(Windows::ApplicationModel::Background::IBackgroundTaskInstance const& taskInstance, Windows::ApplicationModel::Background::BackgroundTaskCancellationReason reason);
        fire_and_forget Op1Characteristic_WriteRequestedAsync(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteRequestedEventArgs const& args);
        fire_and_forget Op2Characteristic_WriteRequestedAsync(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteRequestedEventArgs const& args);
        fire_and_forget OperatorCharacteristic_WriteRequestedAsync(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteRequestedEventArgs const& args);
        Windows::Foundation::IAsyncAction ProcessWriteCharacteristicAndCalculation(Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteRequestedEventArgs const& args, CalculatorCharacteristics opCode);
        fire_and_forget ResultCharacteristic_ReadRequestedAsync(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadRequestedEventArgs const& args);
        Windows::Foundation::IAsyncAction ProcessReadRequest(Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadRequestedEventArgs const& args);
        void ResultCharacteristic_SubscribedClientsChanged(Windows::Devices::Bluetooth::GenericAttributeProfile::GattLocalCharacteristic const& sender, IInspectable const& args);
        fire_and_forget ComputeResult();
    };
}

namespace winrt::SDKTemplate::factory_implementation
{
    struct CalculatorServerBackgroundTask : CalculatorServerBackgroundTaskT<CalculatorServerBackgroundTask, implementation::CalculatorServerBackgroundTask>
    {
    };
}
