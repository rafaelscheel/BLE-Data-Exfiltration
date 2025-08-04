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

#include "pch.h"
#include "CalculatorServerBackgroundTask.h"
#include "CalculatorServerBackgroundTask.g.cpp"
#include "SampleConfiguration.h"

using namespace winrt::Windows::ApplicationModel::Background;
using namespace winrt::Windows::Devices::Bluetooth::Background;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;

namespace winrt::SDKTemplate::implementation
{
    event<delegate<>> CalculatorServerBackgroundTask::ValuesChanged;

    void CalculatorServerBackgroundTask::Run(IBackgroundTaskInstance const& instance)
    {
        taskDeferral = instance.GetDeferral();
        this->taskInstance = instance;
        this->taskInstance.Canceled({ get_weak(), &CalculatorServerBackgroundTask::CompleteTask});

        // Needed for in-process servers. (For out-of-process servers, the background infrastructure
        // keeps the service object alive until it is shut down.)
        this->keepServiceAlive = *this;

        auto triggerDetails = instance.TriggerDetails().as<GattServiceProviderTriggerDetails>();
        this->serviceConnection = triggerDetails.Connection();

        InitializeService();
    }

    void CalculatorServerBackgroundTask::InitializeService()
    {
        // Create (or obtain existing) settings container for the calculator service.
        calculatorValues = ApplicationData::Current().LocalSettings().
            CreateContainer(Constants::CalculatorContainerName, ApplicationDataCreateDisposition::Always).Values();

        // Start with a blank slate.
        calculatorValues.Clear();

        // Let the UI know that the values have bbeen reset, so it can update.
        ValuesChanged();

        // Bind our characteristic handlers
        for (auto const& characteristic : this->serviceConnection.Service().Characteristics())
        {
            if (characteristic.Uuid() == Constants::BackgroundOperand1CharacteristicUuid)
            {
                operand1Characteristic = characteristic;
                operand1CharacteristicWriteToken = operand1Characteristic.WriteRequested({ get_weak(), &CalculatorServerBackgroundTask::Op1Characteristic_WriteRequestedAsync });
            }
            else if (characteristic.Uuid() == Constants::BackgroundOperand2CharacteristicUuid)
            {
                operand2Characteristic = characteristic;
                operand2CharacteristicWriteToken = operand2Characteristic.WriteRequested({ get_weak(), &CalculatorServerBackgroundTask::Op2Characteristic_WriteRequestedAsync });
            }
            else if (characteristic.Uuid() == Constants::BackgroundOperatorCharacteristicUuid)
            {
                operatorCharacteristic = characteristic;
                operatorCharacteristicWriteToken = operatorCharacteristic.WriteRequested({ get_weak(), &CalculatorServerBackgroundTask::OperatorCharacteristic_WriteRequestedAsync });
            }
            else if (characteristic.Uuid() == Constants::BackgroundResultCharacteristicUuid)
            {
                resultCharacteristic = characteristic;
                resultCharacteristicReadToken = resultCharacteristic.ReadRequested({ get_weak(), &CalculatorServerBackgroundTask::ResultCharacteristic_ReadRequestedAsync });
                resultCharacteristicChangedToken = resultCharacteristic.SubscribedClientsChanged({ get_weak(), &CalculatorServerBackgroundTask::ResultCharacteristic_SubscribedClientsChanged});
            }
        }

        if (!operand1Characteristic)
        {
            throw hresult_error(E_FAIL, L"Could not restore operand1 characteristic");
        }
        if (!operand2Characteristic)
        {
            throw hresult_error(E_FAIL, L"Could not restore operand2 characteristic");
        }
        if (!operatorCharacteristic)
        {
            throw hresult_error(E_FAIL, L"Could not restore operator characteristic");
        }
        if (!resultCharacteristic)
        {
            throw hresult_error(E_FAIL, L"Could not restore result characteristic");
        }

        this->serviceConnection.Start();
    }

    void CalculatorServerBackgroundTask::CompleteTask(IBackgroundTaskInstance const&, BackgroundTaskCancellationReason)
    {
        if (operand1Characteristic)
        {
            operand1Characteristic.WriteRequested(std::exchange(operand1CharacteristicWriteToken, {}));
            operand1Characteristic = nullptr;
        }
        if (operand2Characteristic)
        {
            operand2Characteristic.WriteRequested(std::exchange(operand2CharacteristicWriteToken, {}));
            operand2Characteristic = nullptr;
        }
        if (operatorCharacteristic)
        {
            operatorCharacteristic.WriteRequested(std::exchange(operatorCharacteristicWriteToken, {}));
            operatorCharacteristic = nullptr;
        }
        if (resultCharacteristic)
        {
            resultCharacteristic.ReadRequested(std::exchange(resultCharacteristicReadToken, {}));
            resultCharacteristic.SubscribedClientsChanged(std::exchange(resultCharacteristicChangedToken, {}));
            resultCharacteristic = nullptr;
        }

        if (taskDeferral)
        {
            taskDeferral.Complete();
        }

        // Needed for in-process servers. (For out-of-process servers, the background infrastructure
        // retains the service reference until it is shut down.)
        keepServiceAlive = nullptr;
    }

    fire_and_forget CalculatorServerBackgroundTask::Op1Characteristic_WriteRequestedAsync(GattLocalCharacteristic const& sender, GattWriteRequestedEventArgs const& args)
    {
        auto lifetime = get_strong();
        co_await ProcessWriteCharacteristicAndCalculation(args, CalculatorCharacteristics::Operand1);
    }

    fire_and_forget CalculatorServerBackgroundTask::Op2Characteristic_WriteRequestedAsync(GattLocalCharacteristic const& sender, GattWriteRequestedEventArgs const& args)
    {
        auto lifetime = get_strong();
        co_await ProcessWriteCharacteristicAndCalculation(args, CalculatorCharacteristics::Operand2);
    }

    fire_and_forget CalculatorServerBackgroundTask::OperatorCharacteristic_WriteRequestedAsync(GattLocalCharacteristic const& sender, GattWriteRequestedEventArgs const& args)
    {
        auto lifetime = get_strong();
        co_await ProcessWriteCharacteristicAndCalculation(args, CalculatorCharacteristics::Operator);
    }

    fire_and_forget CalculatorServerBackgroundTask::ResultCharacteristic_ReadRequestedAsync(GattLocalCharacteristic const& sender, GattReadRequestedEventArgs const& args)
    {
        auto lifetime = get_strong();
        co_await ProcessReadRequest(args);
    }

    IAsyncAction CalculatorServerBackgroundTask::ProcessReadRequest(GattReadRequestedEventArgs const& args)
    {
        // We perform a co_await, so we require a deferral.
        // Put the deferral in a DeferralCompleter to ensure that it completes
        // when control exits the function.
        auto deferral = DeferralCompleter(args.GetDeferral());

        auto request = co_await args.GetRequestAsync();
        if (request ==nullptr)
        {
            // No access allowed to the device.
            co_return;
        }

        OutputDebugString((L"resultVal: " + to_hstring(resultVal) + L"\n").c_str());
        request.RespondWithValue(BufferHelpers::ToBuffer(resultVal));
    }

    IAsyncAction CalculatorServerBackgroundTask::ProcessWriteCharacteristicAndCalculation(GattWriteRequestedEventArgs const& args, CalculatorCharacteristics opCode)
    {
        // We perform a co_await, so we require a deferral.
        // Put the deferral in a DeferralCompleter to ensure that it completes
        // when control exits the function.
        auto deferral = DeferralCompleter(args.GetDeferral());

        auto request = co_await args.GetRequestAsync();
        if (request == nullptr)
        {
            // No access allowed to the device.
            co_return;
        }

        std::optional<int> value = BufferHelpers::FromBuffer<int>(request.Value());
        if (!value)
        {
            // Invalid buffer length for 32-bit integer.
            // If a response was requested, then respond with a protocol error
            // indicating that the length is incorrect.
            if (request.Option() == GattWriteOption::WriteWithResponse)
            {
                request.RespondWithProtocolError(GattProtocolError::InvalidAttributeValueLength());
            }
            co_return;
        }

        // Record the written value and also save it in a place the foreground UI can see.
        switch (opCode)
        {
        case CalculatorCharacteristics::Operand1:
            operand1Received = *value;
            calculatorValues.Insert(L"Operand1", box_value(operand1Received));
            break;
        case CalculatorCharacteristics::Operand2:
            operand2Received = *value;
            calculatorValues.Insert(L"Operand2", box_value(operand2Received));
            break;
        case CalculatorCharacteristics::Operator:
            if (IsValidEnumValue(static_cast<CalculatorOperators>(*value)))
            {
                operatorReceived = static_cast<CalculatorOperators>(*value);
                calculatorValues.Insert(L"Operator", box_value(static_cast<int>(operatorReceived)));
            }
            else
            {
                if (request.Option() == GattWriteOption::WriteWithResponse)
                {
                    request.RespondWithProtocolError(GattProtocolError::InvalidPdu());
                }
                co_return;
            }
            break;
        }

        // Check if the operator is valid and compute the result.
        if (IsValidEnumValue(operatorReceived))
        {
            ComputeResult();
        }

        // Let the UI know that the values have changed, so it can update.
        ValuesChanged();

        if (request.Option() == GattWriteOption::WriteWithResponse)
        {
            request.Respond();
        }
    }

    void CalculatorServerBackgroundTask::ResultCharacteristic_SubscribedClientsChanged(GattLocalCharacteristic const&, IInspectable const&)
    {
        // This function could be used to monitor the clients that have subscribed to the characteristic and perform actions based on the clients that have subscribed.
    }

    fire_and_forget CalculatorServerBackgroundTask::ComputeResult()
    {
        auto lifetime = get_strong();

        switch (operatorReceived)
        {
        case CalculatorOperators::Add:
            resultVal = operand1Received + operand2Received;
            break;
        case CalculatorOperators::Subtract:
            resultVal = operand1Received - operand2Received;
            break;
        case CalculatorOperators::Multiply:
            resultVal = operand1Received * operand2Received;
            break;
        case CalculatorOperators::Divide:
            if (operand2Received == 0 || (operand1Received == std::numeric_limits<int>::min() && operand2Received == -1))
            {
                // It could use AppServiceConnection to the foreground app to indicate an calculation error.
            }
            else
            {
                resultVal = operand1Received / operand2Received;
            }
            break;
        default:
            // It could use AppServiceConnection to the foreground app to indicate an error.
            break;
        }

        calculatorValues.Insert(L"Result", box_value(resultVal));

        // BT_Code: Returns a collection of all clients that the notification was attempted and the result.
        auto results = co_await resultCharacteristic.NotifyValueAsync(BufferHelpers::ToBuffer(resultVal));

        for ([[maybe_unused]] auto const& result : results)
        {
            // An application can iterate through each registered client that was notified and retrieve the results:
            //
            // result.SubscribedClient(): The details on the remote client.
            // result.Status(): The GattCommunicationStatus
            // result.ProtocolError(): iff Status() == GattCommunicationStatus::ProtocolError
        }
    }
}
