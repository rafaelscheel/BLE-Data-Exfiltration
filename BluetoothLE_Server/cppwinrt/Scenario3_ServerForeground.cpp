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
#include "Scenario3_ServerForeground.h"
#include "Scenario3_ServerForeground.g.cpp"
#include "SampleConfiguration.h"

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Navigation;

namespace winrt::SDKTemplate::implementation
{
    // This scenario declares support for a calculator service.
    // Remote clients (including this sample on another machine) can supply:
    // - Operands 1 and 2
    // - an operator (+,-,*,/)
    // and get a result

#pragma region UI Code
    Scenario3_ServerForeground::Scenario3_ServerForeground()
    {
        InitializeComponent();

        ServiceIdRun().Text(to_hstring(Constants::CalculatorServiceUuid));

        // BT_Code: IsDiscoverable determines whether a remote device can query the local device for support
        // of this service.
        advertisingParameters.IsDiscoverable(true);
    }

    fire_and_forget Scenario3_ServerForeground::OnNavigatedTo(NavigationEventArgs const&)
    {
        navigatedTo = true;

        auto lifetime = get_strong();

        // BT_Code: New for Creator's Update - Bluetooth adapter has properties of the local BT radio.
        BluetoothAdapter adapter = co_await BluetoothAdapter::GetDefaultAsync();

        if (adapter != nullptr && adapter.IsPeripheralRoleSupported())
        {
            // BT_Code: Specify that the server advertises as connectable.
            // IsConnectable determines whether a call to publish will attempt to start advertising and
            // put the service UUID in the ADV packet (best effort)
            advertisingParameters.IsConnectable(true);

            ServerPanel().Visibility(Visibility::Visible);
        }
        else
        {
            // No Bluetooth adapter or adapter cannot act as server.
            PeripheralWarning().Visibility(Visibility::Visible);
        }

        // Check whether the local Bluetooth adapter and Windows support 2M and Coded PHY.
        if (!FeatureDetection::AreExtendedAdvertisingPhysAndScanParametersSupported())
        {
            Publishing2MPHYReasonRun().Text(L"(Not supported by this version of Windows)");
        }
        else if (adapter != nullptr && adapter.IsLowEnergyUncoded2MPhySupported())
        {
            Publishing2MPHY().IsEnabled(true);
        }
        else
        {
            Publishing2MPHYReasonRun().Text(L"(Not supported by default Bluetooth adapter)");
        }
    }

    void Scenario3_ServerForeground::OnNavigatedFrom(NavigationEventArgs const&)
    {
        navigatedTo = false;

        UnsubscribeServiceEvents();
        // Do not null out the characteristics because tasks may still be using them.

        if (serviceProvider != nullptr)
        {
            if (serviceProvider.AdvertisementStatus() != GattServiceProviderAdvertisementStatus::Stopped)
            {
                serviceProvider.StopAdvertising();
            }
            serviceProvider = nullptr;
        }
    }

    fire_and_forget Scenario3_ServerForeground::PublishButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        if (serviceProvider == nullptr)
        {
            // Server not initialized yet - initialize it and start publishing
            // Don't try to start if already starting.
            if (startingService)
            {
                co_return;
            }
            PublishButton().Content(box_value(L"Starting..."));
            startingService = true;
            co_await CreateAndAdvertiseServiceAsync();
            startingService = false;
            if (serviceProvider != nullptr)
            {
                rootPage.NotifyUser(L"Service successfully started", NotifyType::StatusMessage);
            }
            else
            {
                UnsubscribeServiceEvents();
                rootPage.NotifyUser(L"Service not started", NotifyType::ErrorMessage);
            }
        }
        else
        {
            // BT_Code: Stops advertising support for custom GATT Service
            UnsubscribeServiceEvents();
            serviceProvider.StopAdvertising();
            serviceProvider = nullptr;
        }
        PublishButton().Content(box_value(serviceProvider == nullptr ? L"Start Service": L"Stop Service"));

    }

    void Scenario3_ServerForeground::Publishing2MPHY_Click(IInspectable const&, RoutedEventArgs const&)
    {
        // Update the advertising parameters based on the checkbox.
        bool shouldAdvertise2MPhy = Publishing2MPHY().IsChecked().Value();
        advertisingParameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy(!shouldAdvertise2MPhy);
        advertisingParameters.UseLowEnergyUncoded2MPhyAsSecondaryPhy(shouldAdvertise2MPhy);

        if (serviceProvider != nullptr)
        {
            // Reconfigure the advertising parameters on the fly.
            serviceProvider.UpdateAdvertisingParameters(advertisingParameters);
        }
    }

    void Scenario3_ServerForeground::UnsubscribeServiceEvents()
    {
        if (operand1CharacteristicWriteToken)
        {
            operand1Characteristic.WriteRequested(std::exchange(operand1CharacteristicWriteToken, {}));
        }
        if (operand2CharacteristicWriteToken)
        {
            operand2Characteristic.WriteRequested(std::exchange(operand2CharacteristicWriteToken, {}));
        }
        if (operatorCharacteristicWriteToken)
        {
            operatorCharacteristic.WriteRequested(std::exchange(operatorCharacteristicWriteToken, {}));
        }
        if (resultCharacteristicReadToken)
        {
            resultCharacteristic.ReadRequested(std::exchange(resultCharacteristicReadToken, {}));
        }
        if (resultCharacteristicClientsChangedToken)
        {
            resultCharacteristic.SubscribedClientsChanged(std::exchange(resultCharacteristicClientsChangedToken, {}));
        }
        if (serviceProviderAdvertisementChangedToken)
        {
            serviceProvider.AdvertisementStatusChanged(std::exchange(serviceProviderAdvertisementChangedToken, {}));
        }
    }

    void Scenario3_ServerForeground::UpdateUI()
    {
        const wchar_t* operationText = L"N/A";
        switch (operatorValue)
        {
        case CalculatorOperators::Add:
            operationText = L"+";
            break;
        case CalculatorOperators::Subtract:
            operationText = L"\u2212"; // Minus sign
            break;
        case CalculatorOperators::Multiply:
            operationText = L"\u00d7"; // Multiplication sign
            break;
        case CalculatorOperators::Divide:
            operationText = L"\u00f7"; // Division sign
            break;
        }
        OperationTextBox().Text(operationText);
        Operand1TextBox().Text(to_hstring(operand1Value));
        Operand2TextBox().Text(to_hstring(operand2Value));
        ResultTextBox().Text(to_hstring(resultValue));
    }
#pragma endregion

    /// <summary>
    /// Uses the relevant Service/Characteristic UUIDs to initialize, hook up event handlers and start a service on the local system.
    /// </summary>
    /// <returns></returns>
    IAsyncAction Scenario3_ServerForeground::CreateAndAdvertiseServiceAsync()
    {
        // BT_Code: Initialize and starting a custom GATT Service using GattServiceProvider.
        auto lifetime = get_strong();

        GattServiceProviderResult serviceResult = co_await GattServiceProvider::CreateAsync(Constants::CalculatorServiceUuid);
        if (serviceResult.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create service provider: " + to_hstring(serviceResult.Error()), NotifyType::ErrorMessage);
            co_return;
        }
        GattServiceProvider provider = serviceResult.ServiceProvider();

        // BT_Code: Initializes custom local parameters w/ properties, protection levels as well as common descriptors like User Description.
        GattLocalCharacteristicResult result = co_await provider.Service().CreateCharacteristicAsync(
            Constants::Operand1CharacteristicUuid, Constants::gattOperand1Parameters());
        if (result.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create operand1 characteristic: " + to_hstring(result.Error()), NotifyType::ErrorMessage);
            co_return;
        }

        operand1Characteristic = result.Characteristic();
        operand1CharacteristicWriteToken = operand1Characteristic.WriteRequested(
            { get_weak(), &Scenario3_ServerForeground::Op1Characteristic_WriteRequestedAsync });

        // Create the second operand characteristic.
        result = co_await provider.Service().CreateCharacteristicAsync(
            Constants::Operand2CharacteristicUuid, Constants::gattOperand2Parameters());
        if (result.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create operand2 characteristic: " + to_hstring(result.Error()), NotifyType::ErrorMessage);
            co_return;
        }
        operand2Characteristic = result.Characteristic();
        operand2CharacteristicWriteToken = operand2Characteristic.WriteRequested(
            { get_weak(), &Scenario3_ServerForeground::Op2Characteristic_WriteRequestedAsync });

        // Create the operator characteristic.
        result = co_await provider.Service().CreateCharacteristicAsync(
            Constants::OperatorCharacteristicUuid, Constants::gattOperatorParameters());
        if (result.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create operator characteristic: " + to_hstring(result.Error()), NotifyType::ErrorMessage);
            co_return;
        }
        operatorCharacteristic = result.Characteristic();
        operatorCharacteristicWriteToken = operatorCharacteristic.WriteRequested(
            { get_weak(), &Scenario3_ServerForeground::OperatorCharacteristic_WriteRequestedAsync });

        // Create the result characteristic.
        result = co_await provider.Service().CreateCharacteristicAsync(
            Constants::ResultCharacteristicUuid, Constants::gattResultParameters());
        if (result.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create result characteristic: " + to_hstring(result.Error()), NotifyType::ErrorMessage);
            co_return;
        }
        resultCharacteristic = result.Characteristic();
        resultCharacteristicReadToken = resultCharacteristic.ReadRequested(
            { get_weak(), &Scenario3_ServerForeground::ResultCharacteristic_ReadRequestedAsync });

        resultCharacteristicClientsChangedToken = resultCharacteristic.SubscribedClientsChanged(
            { get_weak(), &Scenario3_ServerForeground::ResultCharacteristic_SubscribedClientsChanged });

        // The advertising parameters were updated at various points in this class.
        // IsDiscoverable was set in the class constructor.
        // IsConnectable was set in OnNavigatedTo when we confirmed that the device supports peripheral role.
        // UseLowEnergyUncoded1MPhy/2MPhyAsSecondaryPhy was set when the user toggled the Publishing2MPHY button.

        // Last chance: Did the user navigate away while we were doing all this work?
        // If so, then abandon our work without starting the provider.
        // Must do this after the last await. (Could also do it after earlier awaits.)
        if (!navigatedTo)
        {
            co_return;
        }

        serviceProviderAdvertisementChangedToken = provider.AdvertisementStatusChanged(
            { get_weak(), &Scenario3_ServerForeground::ServiceProvider_AdvertisementStatusChanged });
        provider.StartAdvertising(advertisingParameters);

        // Let the other methods know that we have a provider that is advertising.
        serviceProvider = provider;
    }

    void Scenario3_ServerForeground::ResultCharacteristic_SubscribedClientsChanged(GattLocalCharacteristic const& sender, IInspectable const&)
    {
        rootPage.NotifyUser(L"New device subscribed. New subscribed count: " + to_hstring(sender.SubscribedClients().Size()), NotifyType::StatusMessage);
    }

    void Scenario3_ServerForeground::ServiceProvider_AdvertisementStatusChanged(GattServiceProvider const& sender, GattServiceProviderAdvertisementStatusChangedEventArgs const&)
    {
        // Created - The default state of the advertisement, before the service is published for the first time.
        // Stopped - Indicates that the application has canceled the service publication and its advertisement.
        // Started - Indicates that the system was successfully able to issue the advertisement request.
        // Aborted - Indicates that the system was unable to submit the advertisement request, or it was canceled due to resource contention.

        rootPage.NotifyUser(L"New Advertisement Status: AdvertisementStatus = " + to_hstring(sender.AdvertisementStatus()), NotifyType::StatusMessage);
    }

    fire_and_forget Scenario3_ServerForeground::ResultCharacteristic_ReadRequestedAsync(GattLocalCharacteristic const&, GattReadRequestedEventArgs args)
    {
        // BT_Code: Process a read request.
        auto lifetime = get_strong();
        auto completer = DeferralCompleter(args.GetDeferral());

        // Get the request information.  This requires device access before an app can access the device's request.
        GattReadRequest request = co_await args.GetRequestAsync();
        if (request == nullptr)
        {
            // No access allowed to the device.  Application should indicate this to the user.
            rootPage.NotifyUser(L"Access to device not allowed", NotifyType::ErrorMessage);
        }
        else
        {
            // Can get details about the request such as the size and offset, as well as monitor the state to see if it has been completed/cancelled externally.
            // request.Offset
            // request.Length
            // request.State
            // request.StateChanged += <Handler>

            // Gatt code to handle the response
            request.RespondWithValue(BufferHelpers::ToBuffer(resultValue));
        }
    }

    void Scenario3_ServerForeground::ComputeResult()
    {
        switch (operatorValue)
        {
        case CalculatorOperators::Add:
            resultValue = operand1Value + operand2Value;
            break;
        case CalculatorOperators::Subtract:
            resultValue = operand1Value - operand2Value;
            break;
        case CalculatorOperators::Multiply:
            resultValue = operand1Value * operand2Value;
            break;
        case CalculatorOperators::Divide:
            if (operand2Value == 0 || (operand1Value == std::numeric_limits<int>::min() && operand2Value == -1))
            {
                rootPage.NotifyUser(L"Division overflow", NotifyType::ErrorMessage);
            }
            else
            {
                resultValue = operand1Value / operand2Value;
            }
            break;
        default:
            rootPage.NotifyUser(L"Invalid Operator", NotifyType::ErrorMessage);
            break;
        }
        NotifyClientDevices(resultValue);
    }

    fire_and_forget Scenario3_ServerForeground::NotifyClientDevices(int computedValue)
    {
        auto lifetime = get_strong();

        // BT_Code: Returns a collection of all clients that the notification was attempted and the result.
        IVectorView<GattClientNotificationResult> results = co_await resultCharacteristic.NotifyValueAsync(BufferHelpers::ToBuffer(computedValue));

        rootPage.NotifyUser(L"Sent value " + to_hstring(computedValue) + L" to clients.", NotifyType::StatusMessage);
        for (GattClientNotificationResult result : results)
        {
            // An application can iterate through each registered client that was notified and retrieve the results:
            //
            // result.SubscribedClient(): The details on the remote client.
            // result.Status(): The GattCommunicationStatus
            // result.ProtocolError(): iff Status() == GattCommunicationStatus::ProtocolError
        }
    }

    fire_and_forget Scenario3_ServerForeground::Op1Characteristic_WriteRequestedAsync(GattLocalCharacteristic const&, GattWriteRequestedEventArgs args)
    {
        // BT_Code: Processing a write request.
        auto lifetime = get_strong();
        auto completer = DeferralCompleter(args.GetDeferral());

        // Get the request information.  This requires device access before an app can access the device's request.
        GattWriteRequest request = co_await args.GetRequestAsync();
        if (request == nullptr)
        {
            // No access allowed to the device.  Application should indicate this to the user.
        }
        else
        {
            ProcessWriteCharacteristic(request, CalculatorCharacteristics::Operand1);
        }
    }

    fire_and_forget Scenario3_ServerForeground::Op2Characteristic_WriteRequestedAsync(GattLocalCharacteristic const&, GattWriteRequestedEventArgs args)
    {
        auto lifetime = get_strong();
        auto completer = DeferralCompleter(args.GetDeferral());

        // Get the request information.  This requires device access before an app can access the device's request.
        GattWriteRequest request = co_await args.GetRequestAsync();
        if (request == nullptr)
        {
            // No access allowed to the device.  Application should indicate this to the user.
        }
        else
        {
            ProcessWriteCharacteristic(request, CalculatorCharacteristics::Operand2);
        }
    }

    fire_and_forget Scenario3_ServerForeground::OperatorCharacteristic_WriteRequestedAsync(GattLocalCharacteristic const&, GattWriteRequestedEventArgs args)
    {
        auto lifetime = get_strong();
        auto completer = DeferralCompleter(args.GetDeferral());

        // Get the request information.  This requires device access before an app can access the device's request.
        GattWriteRequest request = co_await args.GetRequestAsync();
        if (request == nullptr)
        {
            // No access allowed to the device.  Application should indicate this to the user.
        }
        else
        {
            ProcessWriteCharacteristic(request, CalculatorCharacteristics::Operator);
        }
    }

    fire_and_forget Scenario3_ServerForeground::ProcessWriteCharacteristic(GattWriteRequest const& request, CalculatorCharacteristics opCode)
    {
        auto lifetime = get_strong();

        std::optional<int> value = BufferHelpers::FromBuffer<int>(request.Value());
        if (!value)
        {
            // Input is the wrong length. Respond with a protocol error if requested.
            if (request.Option() == GattWriteOption::WriteWithResponse)
            {
                request.RespondWithProtocolError(GattProtocolError::InvalidAttributeValueLength());
            }
            co_return;
        }

        switch (opCode)
        {
        case CalculatorCharacteristics::Operand1:
            operand1Value = *value;
            break;
        case CalculatorCharacteristics::Operand2:
            operand2Value = *value;
            break;
        case CalculatorCharacteristics::Operator:
            if (!IsValidOperator(static_cast<CalculatorOperators>(*value)))
            {
                if (request.Option() == GattWriteOption::WriteWithResponse)
                {
                    request.RespondWithProtocolError(GattProtocolError::InvalidPdu());
                }
                co_return;
            }
            operatorValue = static_cast<CalculatorOperators>(*value);
            break;
        }
        // Complete the request if needed
        if (request.Option() == GattWriteOption::WriteWithResponse)
        {
            request.Respond();
        }

        ComputeResult();

        co_await winrt::resume_foreground(Dispatcher());
        UpdateUI();
    }
}
