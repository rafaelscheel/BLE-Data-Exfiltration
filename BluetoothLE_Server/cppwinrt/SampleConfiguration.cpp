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
#include <winrt/SDKTemplate.h>
#include "MainPage.h"
#include "SampleConfiguration.h"
#include "PresentationFormats.h"

namespace winrt
{
    using namespace winrt::Windows::ApplicationModel::Background;
    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Foundation::Collections;
    using namespace winrt::Windows::Foundation::Metadata;
    using namespace winrt::Windows::UI::Xaml;
}

namespace winrt::SDKTemplate
{
    hstring implementation::MainPage::FEATURE_NAME()
    {
        return L"BluetoothLE C++/WinRT Sample";
    }

    IVector<Scenario> implementation::MainPage::scenariosInner = winrt::single_threaded_observable_vector<Scenario>(
    {
        Scenario{ L"Client: Discover servers", xaml_typename<SDKTemplate::Scenario1_Discovery>() },
        Scenario{ L"Client: Connect to a server", xaml_typename<SDKTemplate::Scenario2_Client>() },
        Scenario{ L"Server: Publish foreground", xaml_typename<SDKTemplate::Scenario3_ServerForeground>() },
        Scenario{ L"Server: Publish background", xaml_typename<SDKTemplate::Scenario4_ServerBackground>() },
    });

    /// <summary>
    /// Called from App::OnBackgroundActivated to handle background activation in
    /// the main process. This entry point is used when BackgroundTaskBuilder.TaskEntryPoint is
    /// not set during background task registration.
    /// </summary>
    /// <param name="args">Object that describes the background task being activated.</param>
    void App_OnBackgroundActivated(Windows::ApplicationModel::Activation::BackgroundActivatedEventArgs const& args)
    {
        // Use the args.TaskInstance().Task().Name() and/or args.()TaskInstance.InstanceId() to determine
        // which background task to run. This sample has only one background task,
        // so can just start that task without needing to check.
        CalculatorServerBackgroundTask().Run(args.TaskInstance());
    }

    hstring SampleState::SelectedBleDeviceId;
    hstring SampleState::SelectedBleDeviceName{ L"No device selected" };

    Rect GetElementRect(FrameworkElement const& element)
    {
        auto transform = element.TransformToVisual(nullptr);
        Point point = transform.TransformPoint({});
        return { point, { static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight()) } };
    }

    GattLocalCharacteristicParameters Constants::gattOperand1Parameters()
    {
        GattLocalCharacteristicParameters gattOperandParameters;
        gattOperandParameters.CharacteristicProperties(GattCharacteristicProperties::Write | GattCharacteristicProperties::WriteWithoutResponse);
        gattOperandParameters.WriteProtectionLevel(GattProtectionLevel::Plain);
        gattOperandParameters.UserDescription(L"Operand 1 Characteristic");
        return gattOperandParameters;
    }

    GattLocalCharacteristicParameters Constants::gattOperand2Parameters()
    {
        GattLocalCharacteristicParameters gattOperandParameters;
        gattOperandParameters.CharacteristicProperties(GattCharacteristicProperties::Write | GattCharacteristicProperties::WriteWithoutResponse);
        gattOperandParameters.WriteProtectionLevel(GattProtectionLevel::Plain);
        gattOperandParameters.UserDescription(L"Operand 2 Characteristic");
        return gattOperandParameters;
    }

    GattLocalCharacteristicParameters Constants::gattOperatorParameters()
    {
        GattLocalCharacteristicParameters gattOperatorParameters;
        gattOperatorParameters.CharacteristicProperties(GattCharacteristicProperties::Write | GattCharacteristicProperties::WriteWithoutResponse);
        gattOperatorParameters.WriteProtectionLevel(GattProtectionLevel::Plain);
        gattOperatorParameters.UserDescription(L"Operator Characteristic");
        return gattOperatorParameters;
    }

    GattLocalCharacteristicParameters Constants::gattResultParameters()
    {
        GattLocalCharacteristicParameters gattResultParameters;
        gattResultParameters.CharacteristicProperties(GattCharacteristicProperties::Read | GattCharacteristicProperties::Notify);
        gattResultParameters.WriteProtectionLevel(GattProtectionLevel::Plain);
        gattResultParameters.UserDescription(L"Result Characteristic");

        // Add presentation format - 32-bit unsigned integer, with exponent 0, the unit is unitless, with no company description
        GattPresentationFormat intFormat = GattPresentationFormat::FromParts(
            GattPresentationFormatTypes::UInt32(),
            PresentationFormats::Exponent,
            static_cast<uint16_t>(PresentationFormats::Units::Unitless),
            static_cast<uint8_t>(PresentationFormats::NamespaceId::BluetoothSigAssignedNumber),
            PresentationFormats::Description);
        gattResultParameters.PresentationFormats().Append(intFormat);

        return gattResultParameters;
    }

    hstring Utilities::FormatGattCommunicationStatus(GattCommunicationStatus status, IReference<uint8_t> protocolError)
    {
        static const std::unordered_map<uint8_t, const wchar_t*> protocolErrorTable
        {
            { GattProtocolError::InvalidHandle(), L"Invalid handle" },
            { GattProtocolError::ReadNotPermitted(), L"Read not permitted" },
            { GattProtocolError::WriteNotPermitted(), L"Write not permitted" },
            { GattProtocolError::InvalidPdu(), L"Invalid PDU" },
            { GattProtocolError::InsufficientAuthentication(), L"Insufficient authentication" },
            { GattProtocolError::RequestNotSupported(), L"Request not supported" },
            { GattProtocolError::InvalidOffset(), L"Invalid offset" },
            { GattProtocolError::InsufficientAuthorization(), L"Insufficient authorization" },
            { GattProtocolError::PrepareQueueFull(), L"Prepare queue full" },
            { GattProtocolError::AttributeNotFound(), L"Attribute not found" },
            { GattProtocolError::AttributeNotLong(), L"Attribute not long" },
            { GattProtocolError::InsufficientEncryptionKeySize(), L"Encryption key size too short" },
            { GattProtocolError::InvalidAttributeValueLength(), L"Invalid attribute value length" },
            { GattProtocolError::UnlikelyError(), L"Unlikely error" },
            { GattProtocolError::InsufficientEncryption(), L"Insufficient encryption" },
            { GattProtocolError::UnsupportedGroupType(), L"Unsupported group type" },
            { GattProtocolError::InsufficientResources(), L"Insufficient resources" },
        };
        switch (status)
        {
        case GattCommunicationStatus::Success:
            return L"Success";
        case GattCommunicationStatus::Unreachable:
            return L"Device is unreachable";
        case GattCommunicationStatus::ProtocolError:
            if (protocolError)
            {
                auto it = protocolErrorTable.find(protocolError.Value());
                if (it != protocolErrorTable.end())
                {
                    return hstring(L"Protocol error: ") + it->second;
                }
                return hstring(L"Protocol error: ") + protocolError.Value();
            }
            else
            {
                return hstring(L"Protocol error");
            }
        case GattCommunicationStatus::AccessDenied:
            return L"Access denied";
        default:
            return L"Code " + to_hstring(static_cast<int>(status));
        }
    }

    bool FeatureDetection::AreExtendedAdvertisingPhysAndScanParametersSupported()
    {
        static bool supported = []
        {
            // We will use GattServiceProviderAdvertisingParameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy
            // to detect this feature group.
            bool isPresentInMetadata = ApiInformation::IsPropertyPresent(
                winrt::name_of<GattServiceProviderAdvertisingParameters>(),
                L"UseLowEnergyUncoded1MPhyAsSecondaryPhy");
            if (!isPresentInMetadata)
            {
                return false;
            }

            // The feature is present in metadata. See if it is available at runtime.
            // During the initial rollout of the feature, it may be unavailable at runtime
            // despite being declared in metadata.
            return GattServiceProviderAdvertisingParameters().try_as<IGattServiceProviderAdvertisingParameters3>() != nullptr;
        }();

        return supported;
    }
}

namespace winrt
{
    hstring to_hstring(Windows::Devices::Bluetooth::BluetoothError error)
    {
        switch (error)
        {
        case BluetoothError::Success: return L"Success";
        case BluetoothError::RadioNotAvailable: return L"RadioNotAvailable";
        case BluetoothError::ResourceInUse: return L"ResourceInUse";
        case BluetoothError::DeviceNotConnected: return L"DeviceNotConnected";
        case BluetoothError::OtherError: return L"OtherError";
        case BluetoothError::DisabledByPolicy: return L"DisabledByPolicy";
        case BluetoothError::NotSupported: return L"NotSupported";
        case BluetoothError::DisabledByUser: return L"DisabledByUser";
        case BluetoothError::ConsentRequired: return L"ConsentRequired";
        case BluetoothError::TransportNotSupported: return L"TransportNotSupported";
        }
        return L"Code " + to_hstring(static_cast<int>(error));

    }

    hstring to_hstring(GattServiceProviderAdvertisementStatus status)
    {
        switch (status)
        {
        case GattServiceProviderAdvertisementStatus::Created: return L"Created";
        case GattServiceProviderAdvertisementStatus::Stopped: return L"Stopped";
        case GattServiceProviderAdvertisementStatus::Started: return L"Started";
        case GattServiceProviderAdvertisementStatus::Aborted: return L"Aborted";
        }
        return L"Code " + to_hstring(static_cast<int>(status));
    }
}
