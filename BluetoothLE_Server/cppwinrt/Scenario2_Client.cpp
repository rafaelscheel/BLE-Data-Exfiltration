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
#include "Scenario2_Client.h"
#include "Scenario2_Client.g.cpp"
#include "SampleConfiguration.h"
#include "BluetoothLEAttributeDisplay.h"

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Globalization;
using namespace Windows::Security::Cryptography;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;

namespace
{
    void SetVisibility(UIElement const& element, bool visible)
    {
        element.Visibility(visible ? Visibility::Visible : Visibility::Collapsed);
    }

    // Utility function to convert a string to an int32_t and detect bad input
    bool TryParseInt(const wchar_t* str, int32_t& result)
    {
        wchar_t* end;
        errno = 0;
        long converted = std::wcstol(str, &end, 0);

        if (str == end)
        {
            // Not parseable.
            return false;
        }

        if (errno == ERANGE || converted < INT_MIN || INT_MAX < converted)
        {
            // Out of range.
            return false;
        }

        if (*end != L'\0')
        {
            // Extra unparseable characters at the end.
            return false;
        }

        result = static_cast<int32_t>(converted);
        return true;
    }

}

namespace winrt::SDKTemplate::implementation
{
#pragma region UI Code
    Scenario2_Client::Scenario2_Client()
    {
        InitializeComponent();
    }

    void Scenario2_Client::OnNavigatedTo(NavigationEventArgs const&)
    {
        SelectedDeviceRun().Text(SampleState::SelectedBleDeviceName);
        if (SampleState::SelectedBleDeviceId.empty())
        {
            ConnectButton().IsEnabled(false);
        }
    }

    fire_and_forget Scenario2_Client::OnNavigatedFrom(NavigationEventArgs const&)
    {
        auto lifetime = get_strong();
        co_await ClearBluetoothLEDeviceAsync();
    }
#pragma endregion

#pragma region Enumerating Services
    IAsyncAction Scenario2_Client::ClearBluetoothLEDeviceAsync()
    {
        auto lifetime = get_strong();

        // Capture the characteristic we want to unregister, in case the user changes it during the await.
        GattCharacteristic characteristic = std::exchange(registeredCharacteristic, nullptr);
        event_token token = std::exchange(notificationsToken, {});

        if (characteristic)
        {
            // Clear the CCCD from the remote device so we stop receiving notifications
            GattCommunicationStatus result = co_await characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::None);
            if (result != GattCommunicationStatus::Success)
            {
                // Even if we are unable to unsubscribe, continue with the rest of the cleanup.
                rootPage.NotifyUser(L"Error: Unable to unsubscribe from notifications.", NotifyType::ErrorMessage);
            }
            else
            {
                characteristic.ValueChanged(token);
            }
        }

        if (bluetoothLEDevice != nullptr)
        {
            bluetoothLEDevice.Close();
            bluetoothLEDevice = nullptr;
        }
    }

    fire_and_forget Scenario2_Client::ConnectButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        ConnectButton().IsEnabled(false);

        co_await ClearBluetoothLEDeviceAsync();

        // BT_Code: BluetoothLEDevice.FromIdAsync must be called from a UI thread because it may prompt for consent.
        bluetoothLEDevice = co_await BluetoothLEDevice::FromIdAsync(SampleState::SelectedBleDeviceId);

        if (bluetoothLEDevice != nullptr)
        {
            // Note: BluetoothLEDevice.GattServices property will return an empty list for unpaired devices. For all uses we recommend using the GetGattServicesAsync method.
            // BT_Code: GetGattServicesAsync returns a list of all the supported services of the device (even if it's not paired to the system).
            // If the services supported by the device are expected to change during BT usage, subscribe to the GattServicesChanged event.
            GattDeviceServicesResult result{ nullptr };
            try
            {
                result = co_await bluetoothLEDevice.GetGattServicesAsync(BluetoothCacheMode::Uncached);
            }
            catch (hresult_canceled const&)
            {
                // The bluetoothLeDevice was disposed by OnNavigatedFrom while GetGattServicesAsync was running.
                ConnectButton().IsEnabled(true);
                co_return;
            }

            if (result.Status() == GattCommunicationStatus::Success)
            {
                IVectorView<GattDeviceService> services = result.Services();
                rootPage.NotifyUser(L"Found " + to_hstring(services.Size()) + L" services", NotifyType::StatusMessage);
                for (auto&& service : services)
                {
                    ComboBoxItem item;
                    item.Content(box_value(DisplayHelpers::GetServiceName(service)));
                    item.Tag(service);
                    ServiceList().Items().Append(item);
                }
                ConnectButton().Visibility(Visibility::Collapsed);
                ServiceList().Visibility(Visibility::Visible);
            }
            else
            {
                rootPage.NotifyUser(L"Error: " + Utilities::FormatGattCommunicationStatus(result.Status(), result.ProtocolError()), NotifyType::ErrorMessage);
            }
        }
        else
        {
            rootPage.NotifyUser(L"Unable to find device. Maybe it isn't connected any more.", NotifyType::ErrorMessage);
        }

        ConnectButton().IsEnabled(true);
    }

#pragma region Enumerating Characteristics
    fire_and_forget Scenario2_Client::ServiceList_SelectionChanged(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        auto lifetime = get_strong();

        CharacteristicList().Items().Clear();
        CharacteristicList().Visibility(Visibility::Collapsed);
        RemoveValueChangedHandler();

        auto selectedItem = ServiceList().SelectedItem().as<ComboBoxItem>();
        GattDeviceService service = selectedItem ? selectedItem.Tag().as<GattDeviceService>() : nullptr;

        if (service == nullptr)
        {
            rootPage.NotifyUser(L"No service selected", NotifyType::ErrorMessage);
            co_return;
        }

        IVectorView<GattCharacteristic> characteristics{ nullptr };
        try
        {
            // Ensure we have access to the device.
            auto accessStatus = co_await service.RequestAccessAsync();
            if (accessStatus != DeviceAccessStatus::Allowed)
            {
                // Not granted access
                rootPage.NotifyUser(L"Error accessing service.", NotifyType::ErrorMessage);
                co_return;
            }

            // BT_Code: Get all the child characteristics of a service. Use the cache mode to specify uncached characterstics only
            // and the new Async functions to get the characteristics of unpaired devices as well.
            GattCharacteristicsResult result = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
            if (result.Status() != GattCommunicationStatus::Success)
            {
                rootPage.NotifyUser(L"Error accessing service: " + Utilities::FormatGattCommunicationStatus(result.Status(), result.ProtocolError()), NotifyType::ErrorMessage);
                co_return;
            }
            characteristics = result.Characteristics();
        }
        catch (...)
        {
            rootPage.NotifyUser(L"Restricted service. Can't read characteristics: " + to_message(), NotifyType::ErrorMessage);
            co_return;
        }

        for (GattCharacteristic&& c : characteristics)
        {
            ComboBoxItem item;
            item.Content(box_value(DisplayHelpers::GetCharacteristicName(c)));
            item.Tag(c);
            CharacteristicList().Items().Append(item);
        }
        CharacteristicList().Visibility(Visibility::Visible);
    }
#pragma endregion

    void Scenario2_Client::AddValueChangedHandler()
    {
        ValueChangedSubscribeToggle().Content(box_value(L"Unsubscribe from value changes"));
        if (!notificationsToken)
        {
            registeredCharacteristic = selectedCharacteristic;
            notificationsToken = registeredCharacteristic.ValueChanged({ get_weak(), &Scenario2_Client::Characteristic_ValueChanged });
        }
    }
    void Scenario2_Client::RemoveValueChangedHandler()
    {
        ValueChangedSubscribeToggle().Content(box_value(L"Subscribe to value changes"));
        if (notificationsToken)
        {
            registeredCharacteristic.ValueChanged(std::exchange(notificationsToken, {}));
            registeredCharacteristic = nullptr;
        }
    }

    fire_and_forget Scenario2_Client::CharacteristicList_SelectionChanged(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        auto lifetime = get_strong();

        auto selectedItem = CharacteristicList().SelectedItem().as<ComboBoxItem>();
        selectedCharacteristic = selectedItem ? selectedItem.Tag().as<GattCharacteristic>() : nullptr;

        if (selectedCharacteristic == nullptr)
        {
            EnableCharacteristicPanels(GattCharacteristicProperties::None);
            rootPage.NotifyUser(L"No characteristic selected", NotifyType::ErrorMessage);
            co_return;
        }

        // Get all the child descriptors of a characteristics. Use the cache mode to specify uncached descriptors only
        // and the new Async functions to get the descriptors of unpaired devices as well.
        GattDescriptorsResult result = co_await selectedCharacteristic.GetDescriptorsAsync(BluetoothCacheMode::Uncached);
        if (result.Status() != GattCommunicationStatus::Success)
        {
            rootPage.NotifyUser(L"Descriptor read failure: " + Utilities::FormatGattCommunicationStatus(result.Status(), result.ProtocolError()), NotifyType::ErrorMessage);
        }

        // Enable/disable operations based on the GattCharacteristicProperties.
        EnableCharacteristicPanels(selectedCharacteristic.CharacteristicProperties());
    }

    void Scenario2_Client::EnableCharacteristicPanels(GattCharacteristicProperties properties)
    {
        // BT_Code: Hide the controls which do not apply to this characteristic.
        SetVisibility(CharacteristicReadButton(), (properties & GattCharacteristicProperties::Read) != GattCharacteristicProperties::None);

        SetVisibility(CharacteristicWritePanel(),
            (properties & (GattCharacteristicProperties::Write | GattCharacteristicProperties::WriteWithoutResponse)) != GattCharacteristicProperties::None);
        CharacteristicWriteValue().Text(L"");

        SetVisibility(ValueChangedSubscribeToggle(),
            (properties & (GattCharacteristicProperties::Indicate | GattCharacteristicProperties::Notify)) != GattCharacteristicProperties::None);

        ValueChangedSubscribeToggle().IsEnabled((registeredCharacteristic == nullptr) || (registeredCharacteristic == selectedCharacteristic));
    }

    fire_and_forget Scenario2_Client::CharacteristicReadButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        // Capture the characteristic we are reading from, in case the use changes the selection during the await.
        GattCharacteristic characteristic = selectedCharacteristic;

        // BT_Code: Read the actual value from the device by using Uncached.
        try
        {
            GattReadResult result = co_await selectedCharacteristic.ReadValueAsync(BluetoothCacheMode::Uncached);
            if (result.Status() == GattCommunicationStatus::Success)
            {
                rootPage.NotifyUser(L"Read result: " + FormatValueByPresentation(characteristic, result.Value()), NotifyType::StatusMessage);
            }
            else
            {
                // This can happen when a device reports that it supports reading, but it actually doesn't.
                rootPage.NotifyUser(L"Read failed: " + Utilities::FormatGattCommunicationStatus(result.Status(), result.ProtocolError()), NotifyType::ErrorMessage);
            }
        }
        catch (hresult_error const& ex)
        {
            if (ex.code() == RO_E_CLOSED)
            {
                // Server is no longer available.
                rootPage.NotifyUser(L"Read failed: Service is no longer available.", NotifyType::ErrorMessage);
            }
            else
            {
                throw;
            }
        }
    }

    fire_and_forget Scenario2_Client::CharacteristicWriteButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        hstring text = CharacteristicWriteValue().Text();
        if (!text.empty())
        {
            IBuffer writeBuffer = CryptographicBuffer::ConvertStringToBinary(CharacteristicWriteValue().Text(),
                BinaryStringEncoding::Utf8);

            // WriteBufferToSelectedCharacteristicAsync will display an error message on failure
            // so we don't have to.
            co_await WriteBufferToSelectedCharacteristicAsync(writeBuffer);
        }
        else
        {
            rootPage.NotifyUser(L"No data to write to device", NotifyType::ErrorMessage);
        }
    }

    fire_and_forget Scenario2_Client::CharacteristicWriteButtonInt_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        int32_t writeValue;
        if (TryParseInt(CharacteristicWriteValue().Text().c_str(), writeValue))
        {
            // WriteBufferToSelectedCharacteristicAsync will display an error message on failure
            // so we don't have to.
            co_await WriteBufferToSelectedCharacteristicAsync(BufferHelpers::ToBuffer(writeValue));
        }
        else
        {
            rootPage.NotifyUser(L"Data to write has to be an int32", NotifyType::ErrorMessage);
        }
    }

    IAsyncOperation<bool> Scenario2_Client::WriteBufferToSelectedCharacteristicAsync(IBuffer buffer)
    {
        auto lifetime = get_strong();

        try
        {
            // BT_Code: Writes the value from the buffer to the characteristic.
            GattWriteResult result = co_await selectedCharacteristic.WriteValueWithResultAsync(buffer);

            if (result.Status() == GattCommunicationStatus::Success)
            {
                rootPage.NotifyUser(L"Successfully wrote value to device", NotifyType::StatusMessage);
                co_return true;
            }
            else
            {
                // This can happen, for example, if a device reports that it supports writing, but it actually doesn't.
                rootPage.NotifyUser(L"Write failed: " + Utilities::FormatGattCommunicationStatus(result.Status(), result.ProtocolError()), NotifyType::ErrorMessage);
                co_return false;
            }
        }
        catch (hresult_error const& ex)
        {
            if (ex.code() == RO_E_CLOSED)
            {
                // Server is no longer available.
                rootPage.NotifyUser(L"Write failed: Service is no longer available,", NotifyType::ErrorMessage);
                co_return false;
            }
            throw;
        }
    }

    fire_and_forget Scenario2_Client::ValueChangedSubscribeToggle_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        hstring operation;

        // initialize status
        auto cccdValue = GattClientCharacteristicConfigurationDescriptorValue::None;
        if (notificationsToken)
        {
            // Unsubscribe by specifying "None"
            operation = L"Unsubscribe";
            cccdValue = GattClientCharacteristicConfigurationDescriptorValue::None;
        }
        else if ((selectedCharacteristic.CharacteristicProperties() & GattCharacteristicProperties::Indicate) != GattCharacteristicProperties::None)
        {
            // Subscribe with "indicate"
            operation = L"Subscribe";
            cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
        }
        else if ((selectedCharacteristic.CharacteristicProperties() & GattCharacteristicProperties::Notify) != GattCharacteristicProperties::None)
        {
            // Subscribe with "notify"
            operation = L"Subscribe";
            cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Notify;
        }
        else
        {
            // Unreachable because the button is disabled if it cannot indicate or notify.
        }

        // BT_Code: Must write the CCCD in order for server to send indications.
        // We receive them in the ValueChanged event handler.
        try
        {
            GattWriteResult result = co_await selectedCharacteristic.WriteClientCharacteristicConfigurationDescriptorWithResultAsync(cccdValue);

            if (result.Status() == GattCommunicationStatus::Success)
            {
                rootPage.NotifyUser(operation + L" succeeded", NotifyType::StatusMessage);
                if (cccdValue != GattClientCharacteristicConfigurationDescriptorValue::None)
                {
                    AddValueChangedHandler();
                }
                else
                {
                    RemoveValueChangedHandler();
                }
            }
            else
            {
                // This can happen if a device reports that it supports indicate, but it actually doesn't.
                rootPage.NotifyUser(operation + L" failed: " + Utilities::FormatGattCommunicationStatus(result.Status(), result.ProtocolError()), NotifyType::ErrorMessage);
            }
        }
        catch (hresult_error const& ex)
        {
            if (ex.code() == RO_E_CLOSED)
            {
                // Server is no longer available.
                rootPage.NotifyUser(L"Write failed: Service is no longer available,", NotifyType::ErrorMessage);
            }
            else
            {
                throw;
            }
        }
    }

    fire_and_forget Scenario2_Client::Characteristic_ValueChanged(GattCharacteristic const& sender, GattValueChangedEventArgs args)
    {
        auto lifetime = get_strong();

        // BT_Code: An Indicate or Notify reported that the value has changed.
        // Display the new value with a timestamp.
        hstring newValue = FormatValueByPresentation(sender, args.CharacteristicValue());
        std::time_t now = clock::to_time_t(clock::now());
        char buffer[26];
        ctime_s(buffer, ARRAYSIZE(buffer), &now);
        hstring message = L"Value at " + to_hstring(buffer) + L": " + newValue;
        co_await resume_foreground(Dispatcher());
        CharacteristicLatestValue().Text(message);
    }

    hstring Scenario2_Client::FormatValueByPresentation(GattCharacteristic const& characteristic, IBuffer const& buffer)
    {
        auto uuid = characteristic.Uuid();
        // BT_Code: Choose a presentation format.
        GattPresentationFormat presentationFormat = nullptr;
        IVectorView<GattPresentationFormat> formats = selectedCharacteristic.PresentationFormats();
        uint32_t formatCount = formats.Size();
        if (formatCount == 1)
        {
            // Get the presentation format since there's only one way of presenting it
            presentationFormat = formats.GetAt(0);
        }
        else if (formatCount > 1)
        {
            // It's difficult to figure out how to split up a characteristic and encode its different parts properly.
            // This sample doesn't try. It just encodes the whole thing to a string to make it easy to print out.
        }

        // BT_Code: For the purpose of this sample, this function converts only UInt32 and
        // UTF-8 buffers to readable text. It can be extended to support other formats if your app needs them.
        if (presentationFormat != nullptr)
        {
            if (presentationFormat.FormatType() == GattPresentationFormatTypes::UInt32())
            {
                std::optional<uint32_t> value = BufferHelpers::FromBuffer<uint32_t>(buffer);
                if (value)
                {
                    return to_hstring(*value);
                }
                return L"(error: Invalid UInt32)";

            }
            else if (presentationFormat.FormatType() == GattPresentationFormatTypes::Utf8())
            {
                try
                {
                    return CryptographicBuffer::ConvertBinaryToString(BinaryStringEncoding::Utf8, buffer);
                }
                catch (hresult_invalid_argument const&)
                {
                    return L"(error: Invalid UTF-8 string)";
                }
            }
            else
            {
                // Add support for other format types as needed.
                return L"Unsupported format: " + CryptographicBuffer::EncodeToHexString(buffer);
            }
        }
        else if (buffer.Length() == 0)
        {
            return L"<empty data>";
        }
        // We don't know what format to use. Let's try some well-known profiles, or default back to UTF-8.
        else if (characteristic.Uuid() == GattCharacteristicUuids::HeartRateMeasurement())
        {
            try
            {
                return L"Heart Rate: " + to_hstring(ParseHeartRateValue(buffer));
            }
            catch (hresult_invalid_argument const&)
            {
                return L"Heart Rate: (unable to parse)";
            }
        }
        else if (characteristic.Uuid() == GattCharacteristicUuids::BatteryLevel())
        {
            // battery level is encoded as a percentage value in the first byte according to
            // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.battery_level.xml
            uint8_t percent = buffer.data()[0];
            return L"Battery Level: " + to_hstring(percent) + L"%";
        }
        // This is our custom calc service Result UUID. Format it like an Int
        else if ((characteristic.Uuid() == Constants::ResultCharacteristicUuid) ||
            (characteristic.Uuid() == Constants::BackgroundResultCharacteristicUuid))
        {
            std::optional<int32_t> value = BufferHelpers::FromBuffer<int32_t>(buffer);
            if (value)
            {
                return to_hstring(*value);
            }
            return L"Invalid response from calc server";
        }
        else
        {
            // Okay, so maybe UTF-8?
            try
            {
                return CryptographicBuffer::ConvertBinaryToString(BinaryStringEncoding::Utf8, buffer);
            }
            catch (...)
            {
                // Nope, not even UTF-8. Just show hex.
                return L"Unknown format: " + CryptographicBuffer::EncodeToHexString(buffer);
            }
        }
    }

    /// <summary>
    /// Process the raw data received from the device into application usable data,
    /// according the the Bluetooth Heart Rate Profile.
    /// https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.heart_rate_measurement.xml&u=org.bluetooth.characteristic.heart_rate_measurement.xml
    /// This function throws an std::out_of_range if the data cannot be parsed.
    /// </summary>
    /// <param name="data">Raw data received from the heart rate monitor.</param>
    /// <returns>The heart rate measurement value.</returns>
    uint16_t Scenario2_Client::ParseHeartRateValue(IBuffer const& buffer)
    {
        // Heart Rate profile defined flag values
        const uint8_t heartRateValueFormat16 = 0x1;

        uint8_t* data = buffer.data();
        uint32_t length = buffer.Length();

        if (length < 1)
        {
            throw hresult_invalid_argument();
        }
        if (data[0] & heartRateValueFormat16)
        {
            if (length < 3)
            {
                throw hresult_invalid_argument();
            }
            return data[1] | (data[2] << 8);
        }
        if (length < 2)
        {
            throw hresult_invalid_argument();
        }
        return data[1];
    }
}
