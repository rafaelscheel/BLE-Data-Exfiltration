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
#include "Scenario4_ServerBackground.h"
#include "Scenario4_ServerBackground.g.cpp"
#include "SampleConfiguration.h"
#include "CalculatorServerBackgroundTask.h"
#include "PresentationFormats.h"

using namespace winrt;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Background;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Background;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Metadata;
using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Navigation;

namespace winrt::SDKTemplate::implementation
{
    Scenario4_ServerBackground::Scenario4_ServerBackground()
    {
        InitializeComponent();

        // Create (or obtain existing) settings container for the calculator service.
        calculatorValues = ApplicationData::Current().LocalSettings().
            CreateContainer(Constants::CalculatorContainerName, ApplicationDataCreateDisposition::Always).Values();

        ServiceIdRun().Text(to_hstring(Constants::BackgroundCalculatorServiceUuid));
    }

    fire_and_forget Scenario4_ServerBackground::OnNavigatedTo(NavigationEventArgs const&)
    {
        auto lifetime = get_strong();

        navigatedTo = true;

        // Find the task if we previously registered it.
        for (const auto& taskPair : BackgroundTaskRegistration::AllTasks())
        {
            auto task = taskPair.Value();
            if (task.Name() == taskName)
            {
                taskRegistration = task;
                rootPage.NotifyUser(L"Background publisher already registered.", NotifyType::StatusMessage);
                PublishButton().Content(box_value(L"Stop Service"));
                break;
            }
        }

        // BT_Code: New for Creator's Update - Bluetooth adapter has properties of the local BT radio.
        auto adapter = co_await BluetoothAdapter::GetDefaultAsync();

        // Check whether the default Bluetooth adapter can act as a server.
        if (adapter != nullptr && adapter.IsPeripheralRoleSupported())
        {
            ServerPanel().Visibility(Visibility::Visible);

            // Check whether the local Bluetooth adapter and Windows support 2M and Coded PHY.
            if (!FeatureDetection::AreExtendedAdvertisingPhysAndScanParametersSupported())
            {
                Publishing2MPHYReasonRun().Text(L"(Not supported by this version of Windows)");
            }
            else if (adapter.IsLowEnergyUncoded2MPhySupported())
            {
                Publishing2MPHY().IsEnabled(true);
            }
            else
            {
                Publishing2MPHYReasonRun().Text(L"(Not supported by default Bluetooth adapter)");
            }

        }
        else
        {
            // No Bluetooth adapter or adapter cannot act as server.
            PeripheralWarning().Visibility(Visibility::Visible);
        }

        // Register handlers to stop updating UI when suspending and restart when resuming.
        appSuspendingToken = Application::Current().Suspending({ get_weak(), &Scenario4_ServerBackground::App_Suspending });
        appResumingToken = Application::Current().Resuming({ get_weak(), &Scenario4_ServerBackground::App_Resuming });

        StartUpdatingUI();
    }

    void Scenario4_ServerBackground::OnNavigatedFrom(NavigationEventArgs const&)
    {
        navigatedTo = false;

        Application::Current().Suspending(std::exchange(appSuspendingToken, {}));
        Application::Current().Resuming(std::exchange(appResumingToken, {}));

        StopUpdatingUI();
    }

    void Scenario4_ServerBackground::UpdateUI()
    {
        std::optional<int> value = calculatorValues.TryLookup(L"Operand1").try_as<int>();
        if (value)
        {
            Operand1TextBox().Text(to_hstring(*value));
        }
        else
        {
            Operand1TextBox().Text(L"N/A");
        }

        const wchar_t* operationText = L"N/A";
        value = calculatorValues.TryLookup(L"Operator").try_as<int>();
        if (value)
        {
            switch ((CalculatorOperators)*value)
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
        }
        OperationTextBox().Text(operationText);

        value = calculatorValues.TryLookup(L"Operand2").try_as<int>();
        if (value)
        {
            Operand2TextBox().Text(to_hstring(*value));
        }
        else
        {
            Operand2TextBox().Text(L"N/A");
        }

        value = calculatorValues.TryLookup(L"Result").try_as<int>();
        if (value)
        {
            ResultTextBox().Text(to_hstring(*value));
        }
        else
        {
            ResultTextBox().Text(L"N/A");
        }
    }

    fire_and_forget Scenario4_ServerBackground::OnCalculatorValuesChanged()
    {
        auto lifetime = get_strong();

        // This event is raised from the background task, so dispatch to the UI thread.
        co_await winrt::resume_foreground(Dispatcher());
        UpdateUI();
    }

    void Scenario4_ServerBackground::StartUpdatingUI()
    {
        // Start listening for changes from the calculator service.
        calculatorValuesChangedToken = CalculatorServerBackgroundTask::ValuesChanged.add({ get_weak(), &Scenario4_ServerBackground::OnCalculatorValuesChanged });

        // Force an immediate update to show the latest values.
        UpdateUI();
    }

    void Scenario4_ServerBackground::StopUpdatingUI()
    {
        // Stop listening for changes from the calculator service..
        CalculatorServerBackgroundTask::ValuesChanged.remove(std::exchange(calculatorValuesChangedToken, {}));
    }

    /// <summary>
    /// Invoked when application execution is being suspended.
    /// </summary>
    /// <param name="sender">The source of the suspend request.</param>
    /// <param name="e">Details about the suspend request.</param>
    void Scenario4_ServerBackground::App_Suspending(IInspectable const&, SuspendingEventArgs const&)
    {
        StopUpdatingUI();
    }

    /// <summary>
    /// Invoked when application execution is being resumed.
    /// </summary>
    /// <param name="sender">The source of the resume request.</param>
    /// <param name="e"></param>
    void Scenario4_ServerBackground::App_Resuming(IInspectable const&, IInspectable const&)
    {
        StartUpdatingUI();
    }

    void Scenario4_ServerBackground::Publishing2MPHY_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (taskRegistration == nullptr)
        {
            // The service is not registered. We will set the parameters when we register the service.
            return;
        }

        // Look for the service so we can update its advertising parameters on the fly.
        GattServiceProviderConnection connection = GattServiceProviderConnection::AllServices().TryLookup(triggerId);
        if (connection)
        {
            // Found it. Update the advertising parameters.
            GattServiceProviderAdvertisingParameters parameters = serviceProviderTrigger.AdvertisingParameters();

            bool shouldAdvertise2MPhy = Publishing2MPHY().IsChecked().Value();
            parameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy(!shouldAdvertise2MPhy);
            parameters.UseLowEnergyUncoded2MPhyAsSecondaryPhy(shouldAdvertise2MPhy);
            connection.UpdateAdvertisingParameters(parameters);
        }
    }

    fire_and_forget Scenario4_ServerBackground::PublishOrStopButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        // Register or stop a background publisherTrigger. It will start background advertising if register successfully.
        // First get the existing tasks to see if we already registered for it, if that is the case, stop and unregister the existing task.
        if (taskRegistration == nullptr)
        {
            // Don't try to start if already starting.
            if (startingService)
            {
                co_return;
            }

            PublishButton().Content(box_value(L"Starting..."));
            startingService = true;
            co_await CreateBackgroundServiceAsync();
            startingService = false;
        }
        else
        {
            taskRegistration.Unregister(true);
            taskRegistration = nullptr;
        }
        PublishButton().Content(box_value(taskRegistration == nullptr ? L"Start service" : L"Stop Service"));
    }

    IAsyncAction Scenario4_ServerBackground::CreateBackgroundServiceAsync()
    {
        auto lifetime = get_strong();

        GattServiceProviderTriggerResult serviceResult =
            co_await GattServiceProviderTrigger::CreateAsync(triggerId, Constants::BackgroundCalculatorServiceUuid);

        if (serviceResult.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create background calculator service: " + to_hstring(serviceResult.Error()), NotifyType::ErrorMessage);
            co_return;
        }

        serviceProviderTrigger = serviceResult.Trigger();

        // Create the operand1 characteristic.
        auto result = co_await serviceProviderTrigger.Service().CreateCharacteristicAsync(
            Constants::BackgroundOperand1CharacteristicUuid, Constants::gattOperand1Parameters());
        if (result.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create operand1 characteristic: " + to_hstring(result.Error()), NotifyType::ErrorMessage);
            co_return;
        }

        // Create the operand2 characteristic.
        result = co_await serviceProviderTrigger.Service().CreateCharacteristicAsync(
            Constants::BackgroundOperand2CharacteristicUuid, Constants::gattOperand2Parameters());
        if (result.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create operand2 characteristic: " + to_hstring(result.Error()), NotifyType::ErrorMessage);
            co_return;
        }

        // Create the operator characteristic.
        result = co_await serviceProviderTrigger.Service().CreateCharacteristicAsync(
            Constants::BackgroundOperatorCharacteristicUuid, Constants::gattOperatorParameters());
        if (result.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create operator characteristic: " + to_hstring(result.Error()), NotifyType::ErrorMessage);
            co_return;
        }

        // Create the result characteristic.
        result = co_await serviceProviderTrigger.Service().CreateCharacteristicAsync(
            Constants::BackgroundResultCharacteristicUuid, Constants::gattResultParameters());
        if (result.Error() != BluetoothError::Success)
        {
            rootPage.NotifyUser(L"Could not create result characteristic: " + to_hstring(result.Error()), NotifyType::ErrorMessage);
            co_return;
        }

        // Configure the advertising parameters.
        GattServiceProviderAdvertisingParameters parameters = serviceProviderTrigger.AdvertisingParameters();
        parameters.IsConnectable(true);
        parameters.IsDiscoverable(false);

        if (Publishing2MPHY().IsChecked())
        {
            parameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy(false);
            parameters.UseLowEnergyUncoded2MPhyAsSecondaryPhy(true);
        }

        // Applications registering for background publisherTrigger must request for permission.
        BackgroundAccessStatus backgroundAccessStatus = co_await BackgroundExecutionManager::RequestAccessAsync();
        // Here, we do not fail the registration even if the access is not granted. Instead, we allow
        // the publisherTrigger to be registered and when the access is granted for the Application at a later time,
        // the publisherTrigger will automatically start working again.

        // Last chance: Did the user navigate away while we were doing all this work?
        // If so, then abandon our work without starting the provider.
        // Must do this after the last co_await. (Could also do it after earlier awaits.)
        if (!navigatedTo)
        {
            co_return;
        }

        // At this point we assume we haven't found any existing tasks matching the one we want to register
        // First, configure the task trigger and name.
        // (Leaving the task entry point blank makes it an in-process background task. This trigger supports both
        // in-process and out-of-process background tasks, but in-process is simpler.)
        BackgroundTaskBuilder builder;
        builder.Name(taskName);
        builder.SetTrigger(serviceProviderTrigger);

        // Now perform the registration.
        taskRegistration = builder.Register();

        // Even though the publisherTrigger is registered successfully, it might be blocked. Notify the user if that is the case.
        if ((backgroundAccessStatus == BackgroundAccessStatus::AlwaysAllowed) ||
            (backgroundAccessStatus == BackgroundAccessStatus::AllowedSubjectToSystemPolicy))
        {
            rootPage.NotifyUser(L"Background publisher registered.", NotifyType::StatusMessage);
        }
        else
        {
            rootPage.NotifyUser(L"Background tasks may be disabled for this app", NotifyType::ErrorMessage);
        }
    }
}
