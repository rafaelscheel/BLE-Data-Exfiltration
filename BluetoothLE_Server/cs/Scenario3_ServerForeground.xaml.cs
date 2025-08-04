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
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using GattHelper.Converters;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Navigation;

namespace SDKTemplate
{
    // This scenario declares support for a calculator service.
    // Remote clients (including this sample on another machine) can supply:
    // - Operands 1 and 2
    // - an operator (+,-,*,/)
    // and get a result
    public sealed partial class Scenario3_ServerForeground : Page
    {
        private MainPage rootPage = MainPage.Current;

        // Managing the service.
        private GattServiceProvider serviceProvider;
        private GattLocalCharacteristic RecieveFileNameAndStartCharacteristic;
        private GattLocalCharacteristic RecieveFileContentCharacterictic;
        private GattLocalCharacteristic RecieveFileFinishedCharacteristic;
        private GattLocalCharacteristic resultCharacteristic;
        private GattServiceProviderAdvertisingParameters advertisingParameters;

        // Implementing the service.
        private int operand1Value = 0;
        private int operand2Value = 0;
        CalculatorOperators operatorValue = 0;
        private int resultValue = 0;

        private bool navigatedTo = false;
        private bool startingService = false; // reentrancy protection

        //private static string outputPath = @"c:\temp\";
        //StorageFolder outFolder;
        StorageFile outFile;
        private IRandomAccessStream stream;
        private DataWriter memoryWriter;
        private string fileName;
        private int loop = 1;

        private static string FormatBytes(long bytes)
        {
            string[] Suffix = { "B", "KB", "MB", "GB", "TB" };
            int i;
            double dblSByte = bytes;
            for (i = 0; i < Suffix.Length && bytes >= 1024; i++, bytes /= 1024)
            {
                dblSByte = bytes / 1024.0;
            }

            return String.Format("{0:0.##} {1}", dblSByte, Suffix[i]);
        }

        private enum RecieveCharacteristics
        {
            RecieveFileNameAndStart = 1,
            RecieveFileContent = 2,
            RecieveFileFinished = 3,
        }

        private enum CalculatorOperators
        {
            Add = 1,
            Subtract = 2,
            Multiply = 3,
            Divide = 4,
        }

        #region UI Code
        public Scenario3_ServerForeground()
        {
            InitializeComponent();

            advertisingParameters = new GattServiceProviderAdvertisingParameters
            {
                // IsDiscoverable determines whether a remote device can query the local device for support
                // of this service
                IsDiscoverable = true
            };

            ServiceIdRun.Text = Constants.FileTransferServiceUuid.ToString();
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            navigatedTo = true;

            // BT_Code: New for Creator's Update - Bluetooth adapter has properties of the local BT radio.
            BluetoothAdapter adapter = await BluetoothAdapter.GetDefaultAsync();

            if (adapter != null && adapter.IsPeripheralRoleSupported)
            {
                // BT_Code: Specify that the server advertises as connectable.
                // IsConnectable determines whether a call to publish will attempt to start advertising and
                // put the service UUID in the ADV packet (best effort)
                advertisingParameters.IsConnectable = true;

                ServerPanel.Visibility = Visibility.Visible;
            }
            else
            {
                // No Bluetooth adapter or adapter cannot act as server.
                PeripheralWarning.Visibility = Visibility.Visible;
            }

            // Check whether the local Bluetooth adapter and Windows support 2M and Coded PHY.
            if (!FeatureDetection.AreExtendedAdvertisingPhysAndScanParametersSupported)
            {
                Publishing2MPHYReasonRun.Text = "(Not supported by this version of Windows)";
            }
            else if (adapter != null && adapter.IsLowEnergyUncoded2MPhySupported)
            {
                Publishing2MPHY.IsEnabled = true;
            }
            else
            {
                Publishing2MPHYReasonRun.Text = "(Not supported by default Bluetooth adapter)";
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            navigatedTo = false;

            UnsubscribeServiceEvents();
            // Do not null out the characteristics because tasks may still be using them.

            if (serviceProvider != null)
            {
                if (serviceProvider.AdvertisementStatus != GattServiceProviderAdvertisementStatus.Stopped)
                {
                    serviceProvider.StopAdvertising();
                }
                serviceProvider = null;
            }
        }

        private async void PublishButton_Click(object sender, RoutedEventArgs e)
        {
            if (serviceProvider == null)
            {
                // Server not initialized yet - initialize it and start publishing
                // Don't try to start if already starting.
                if (startingService)
                {
                    return;
                }
                PublishButton.Content = "Starting...";
                startingService = true;
                await CreateAndAdvertiseServiceAsync();
                startingService = false;
                if (serviceProvider != null)
                {
                    rootPage.NotifyUser("Service successfully started", NotifyType.StatusMessage);
                }
                else
                {
                    UnsubscribeServiceEvents();
                    rootPage.NotifyUser("Service not started", NotifyType.ErrorMessage);
                }
            }
            else
            {
                // BT_Code: Stops advertising support for custom GATT Service
                UnsubscribeServiceEvents();
                serviceProvider.StopAdvertising();
                serviceProvider = null;
            }
            PublishButton.Content = serviceProvider == null ? "Start Service": "Stop Service";
        }

        private void Publishing2MPHY_Click(object sender, RoutedEventArgs e)
        {
            // Update the advertising parameters based on the checkbox.
            bool shouldAdvertise2MPHY = Publishing2MPHY.IsChecked.Value;
            advertisingParameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy = !shouldAdvertise2MPHY;
            advertisingParameters.UseLowEnergyUncoded2MPhyAsSecondaryPhy = shouldAdvertise2MPHY;

            if (serviceProvider != null)
            {
                // Reconfigure the advertising parameters on the fly.
                serviceProvider.UpdateAdvertisingParameters(advertisingParameters);
            }
        }

        private void UnsubscribeServiceEvents()
        {
            if (RecieveFileNameAndStartCharacteristic != null)
            {
                RecieveFileNameAndStartCharacteristic.WriteRequested -= RecieveFileNameAndStart_WriteRequestedAsync;
            }
            if (RecieveFileContentCharacterictic != null)
            {
                RecieveFileContentCharacterictic.WriteRequested -= RecieveFileContent_WriteRequestedAsync;
            }
            if (RecieveFileFinishedCharacteristic != null)
            {
                RecieveFileFinishedCharacteristic.WriteRequested -= RecieveFileFinished_WriteRequestedAsync;
            }
            if (resultCharacteristic != null)
            {
                resultCharacteristic.ReadRequested -= ResultCharacteristic_ReadRequestedAsync;
                resultCharacteristic.SubscribedClientsChanged -= ResultCharacteristic_SubscribedClientsChanged;
            }
            if (serviceProvider != null)
            {
                serviceProvider.AdvertisementStatusChanged -= ServiceProvider_AdvertisementStatusChanged;
            }
        }

        private void UpdateUI()
        {
            string operationText = "N/A";
            switch (operatorValue)
            {
                case CalculatorOperators.Add:
                    operationText = "+";
                    break;
                case CalculatorOperators.Subtract:
                    operationText = "\u2212"; // Minus sign
                    break;
                case CalculatorOperators.Multiply:
                    operationText = "\u00d7"; // Multiplication sign
                    break;
                case CalculatorOperators.Divide:
                    operationText = "\u00f7"; // Division sign
                    break;
            }
            OperationTextBox.Text = operationText;
            Operand1TextBox.Text = operand1Value.ToString();
            Operand2TextBox.Text = operand2Value.ToString();
            ResultTextBox.Text = resultValue.ToString();
        }
        #endregion

        /// <summary>
        /// Uses the relevant Service/Characteristic UUIDs to initialize, hook up event handlers and start a service on the local system.
        /// </summary>
        /// <returns></returns>
        private async Task CreateAndAdvertiseServiceAsync()
        {
            // Set up storage folder for file transfer.
            //outFolder = await  StorageFolder.GetFolderFromPathAsync(outputPath);

            // BT_Code: Initialize and starting a custom GATT Service using GattServiceProvider.
            GattServiceProviderResult serviceResult = await GattServiceProvider.CreateAsync(Constants.FileTransferServiceUuid);
            if (serviceResult.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create service provider: {serviceResult.Error}", NotifyType.ErrorMessage);
                return;
            }
            GattServiceProvider provider = serviceResult.ServiceProvider;

            GattLocalCharacteristicResult result = await provider.Service.CreateCharacteristicAsync(
                Constants.RecieveFileNameAndStartCharacteristicUuid, Constants.gattRecieveFileNameAndStartParameters);
            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create operand1 characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }
            RecieveFileNameAndStartCharacteristic = result.Characteristic;
            RecieveFileNameAndStartCharacteristic.WriteRequested += RecieveFileNameAndStart_WriteRequestedAsync;

            result = await provider.Service.CreateCharacteristicAsync(
                Constants.RecieveFileContentCharacteristicUuid, Constants.gattRecieveFileContentParameters);
            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create operand2 characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }

            RecieveFileContentCharacterictic = result.Characteristic;
            RecieveFileContentCharacterictic.WriteRequested += RecieveFileContent_WriteRequestedAsync;

            result = await provider.Service.CreateCharacteristicAsync(
                Constants.RecieveFileFinishedCharacteristicUuid, Constants.gattRecieveFileFinishedParameters);
            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create operator characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }

            RecieveFileFinishedCharacteristic = result.Characteristic;
            RecieveFileFinishedCharacteristic.WriteRequested += RecieveFileFinished_WriteRequestedAsync;

            result = await provider.Service.CreateCharacteristicAsync(Constants.ResultCharacteristicUuid, Constants.gattResultParameters);
            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create result characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }

            resultCharacteristic = result.Characteristic;
            resultCharacteristic.ReadRequested += ResultCharacteristic_ReadRequestedAsync;
            resultCharacteristic.SubscribedClientsChanged += ResultCharacteristic_SubscribedClientsChanged;

            // The advertising parameters were updated at various points in this class.
            // IsDiscoverable was set in the class constructor.
            // IsConnectable was set in OnNavigatedTo when we confirmed that the device supports peripheral role.
            // UseLowEnergyUncoded1MPhy/2MPhyAsSecondaryPhy was set when the user toggled the Publishing2MPHY button.

            // Last chance: Did the user navigate away while we were doing all this work?
            // If so, then abandon our work without starting the provider.
            // Must do this after the last await. (Could also do it after earlier awaits.)
            if (!navigatedTo)
            {
                return;
            }

            provider.AdvertisementStatusChanged += ServiceProvider_AdvertisementStatusChanged;
            provider.StartAdvertising(advertisingParameters);

            // Let the other methods know that we have a provider that is advertising.
            serviceProvider = provider;
        }

        private void ResultCharacteristic_SubscribedClientsChanged(GattLocalCharacteristic sender, object args)
        {
            rootPage.NotifyUser($"New device subscribed. New subscribed count: {sender.SubscribedClients.Count}", NotifyType.StatusMessage);
        }

        private void ServiceProvider_AdvertisementStatusChanged(GattServiceProvider sender, GattServiceProviderAdvertisementStatusChangedEventArgs args)
        {
            // Created - The default state of the advertisement, before the service is published for the first time.
            // Stopped - Indicates that the application has canceled the service publication and its advertisement.
            // Started - Indicates that the system was successfully able to issue the advertisement request.
            // Aborted - Indicates that the system was unable to submit the advertisement request, or it was canceled due to resource contention.

            rootPage.NotifyUser($"New Advertisement Status: {sender.AdvertisementStatus}", NotifyType.StatusMessage);
        }

        private async void ResultCharacteristic_ReadRequestedAsync(GattLocalCharacteristic sender, GattReadRequestedEventArgs args)
        {
            rootPage.NotifyUser("We got a READ Request!!! Yey!", NotifyType.StatusMessage);

            // BT_Code: Process a read request.
            using (args.GetDeferral())
            {
                // Get the request information.  This requires device access before an app can access the device's request.
                GattReadRequest request = await args.GetRequestAsync();
                if (request == null)
                {
                    // No access allowed to the device.  Application should indicate this to the user.
                    rootPage.NotifyUser("Access to device not allowed", NotifyType.ErrorMessage);
                    return;
                }

                // Can get details about the request such as the size and offset, as well as monitor the state to see if it has been completed/cancelled externally.
                // request.Offset
                // request.Length
                // request.State
                // request.StateChanged += <Handler>
                // Gatt code to handle the response
                rootPage.NotifyUser($"request.Offset: {request.Offset} request.Length: {request.Length} request.State: {request.State} ", NotifyType.StatusMessage);

                String responseText = "This is my answer";
                IBuffer respondTextBuffer = GattHelper.Converters.GattConvert.ToIBuffer(responseText);
                request.RespondWithValue(respondTextBuffer);
            }
        }

        private void ComputeResult()
        {
            switch (operatorValue)
            {
                case CalculatorOperators.Add:
                    resultValue = operand1Value + operand2Value;
                    break;
                case CalculatorOperators.Subtract:
                    resultValue = operand1Value - operand2Value;
                    break;
                case CalculatorOperators.Multiply:
                    resultValue = operand1Value * operand2Value;
                    break;
                case CalculatorOperators.Divide:
                    if (operand2Value == 0 || (operand1Value == Int32.MinValue && operand2Value == -1))
                    {
                        rootPage.NotifyUser("Division overflow", NotifyType.ErrorMessage);
                    }
                    else
                    {
                        resultValue = operand1Value / operand2Value;
                    }
                    break;
                default:
                    rootPage.NotifyUser("Invalid Operator", NotifyType.ErrorMessage);
                    break;
            }
            NotifyClientDevices(resultValue);
        }

        private async void NotifyClientDevices(int computedValue)
        {
            // BT_Code: Returns a collection of all clients that the notification was attempted and the result.
            IReadOnlyList<GattClientNotificationResult> results = await resultCharacteristic.NotifyValueAsync(BufferHelpers.BufferFromInt32(computedValue));

            rootPage.NotifyUser($"Sent value {computedValue} to clients.", NotifyType.StatusMessage);
            foreach (var result in results)
            {
                // An application can iterate through each registered client that was notified and retrieve the results:
                //
                // result.SubscribedClient: The details on the remote client.
                // result.Status: The GattCommunicationStatus
                // result.ProtocolError: iff Status == GattCommunicationStatus.ProtocolError
            }
        }

        private async void RecieveFileNameAndStart_WriteRequestedAsync(GattLocalCharacteristic sender, GattWriteRequestedEventArgs args)
        {
            // BT_Code: Processing a write request.
            using (args.GetDeferral())
            {
                // Get the request information.  This requires device access before an app can access the device's request.
                GattWriteRequest request = await args.GetRequestAsync();
                if (request == null)
                {
                    // No access allowed to the device.  Application should indicate this to the user.
                    return;
                }
                ProcessWriteCharacteristic(request, RecieveCharacteristics.RecieveFileNameAndStart);
            }
        }

        private async void RecieveFileContent_WriteRequestedAsync(GattLocalCharacteristic sender, GattWriteRequestedEventArgs args)
        {
            using (args.GetDeferral())
            {
                // Get the request information.  This requires device access before an app can access the device's request.
                GattWriteRequest request = await args.GetRequestAsync();
                if (request == null)
                {
                    // No access allowed to the device.  Application should indicate this to the user.
                    return;
                }
                ProcessWriteCharacteristic(request, RecieveCharacteristics.RecieveFileContent);
            }
        }

        private async void RecieveFileFinished_WriteRequestedAsync(GattLocalCharacteristic sender, GattWriteRequestedEventArgs args)
        {
            using (args.GetDeferral())
            {
                // Get the request information.  This requires device access before an app can access the device's request.
                GattWriteRequest request = await args.GetRequestAsync();
                if (request == null)
                {
                    // No access allowed to the device.  Application should indicate this to the user.
                }
                else
                {
                    ProcessWriteCharacteristic(request, RecieveCharacteristics.RecieveFileFinished);
                }
            }
        }

        /// <summary>
        /// BT_Code: Processing a write request.Takes in a GATT Write request and updates UX based on opcode.
        /// </summary>
        /// <param name="request"></param>
        /// <param name="opCode">Operand (1 or 2) and Operator (3)</param>
        private async void ProcessWriteCharacteristic(GattWriteRequest request, RecieveCharacteristics opCode)
        {
            IBuffer val = request.Value;
            System.Diagnostics.Debug.WriteLine($"Recievied an write of type: {opCode} ");



            if (val == null)
            {
                // Input is the wrong length. Respond with a protocol error if requested.
                if (request.Option == GattWriteOption.WriteWithResponse)
                {
                    request.RespondWithProtocolError(GattProtocolError.InvalidAttributeValueLength);
                }
                return;
            }

            switch (opCode)
            {
                case RecieveCharacteristics.RecieveFileNameAndStart:
                    // BT_Code: Recieve the file name and start the file transfer.
                    fileName = GattHelper.Converters.GattConvert.ToUTF8String(val);
                    System.Diagnostics.Debug.WriteLine($"Starting a new File. File Name: {fileName} ");

                    stream = new Windows.Storage.Streams.InMemoryRandomAccessStream();
                    memoryWriter =  new Windows.Storage.Streams.DataWriter(stream);
                    memoryWriter.ByteOrder = Windows.Storage.Streams.ByteOrder.LittleEndian;
                    System.Diagnostics.Debug.WriteLine($"Writer all setup!: {fileName} ");

                    break;
                case RecieveCharacteristics.RecieveFileContent:
                    System.Diagnostics.Debug.WriteLine($"Appending to file: {fileName} / Recieved: {FormatBytes(loop*512)}. ");
                    loop += 1;

                    memoryWriter.WriteBuffer(val);
                    break;
                case RecieveCharacteristics.RecieveFileFinished:
                    System.Diagnostics.Debug.WriteLine($"Finished the file: {fileName} ");

                    System.Diagnostics.Debug.WriteLine($"Stored file in DOWNLOAD Folder: {fileName} ");

                    rootPage.NotifyUser($"Finished the file: {fileName} ", NotifyType.StatusMessage);

                    await memoryWriter.StoreAsync();

                    StorageFile canvasFile = await DownloadsFolder.CreateFileAsync(fileName, CreationCollisionOption.GenerateUniqueName);
                        using (var reader = new DataReader(stream.GetInputStreamAt(0)))
                        {
                            await reader.LoadAsync((uint)stream.Size);
                            var buffer = new byte[(int)stream.Size];
                            reader.ReadBytes(buffer);
                            await Windows.Storage.FileIO.WriteBytesAsync(canvasFile, buffer);
                        }
                 
                    fileName = null;
                    memoryWriter.DetachStream();
                    
                    break;
            }
            // Complete the request if needed
            if (request.Option == GattWriteOption.WriteWithResponse)
            {
                request.Respond();
            }

            ComputeResult();

            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, UpdateUI);
        }
    }
}
