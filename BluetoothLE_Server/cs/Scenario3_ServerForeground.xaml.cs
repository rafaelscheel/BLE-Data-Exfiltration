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
using GattHelper.Converters;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection.Emit;
using System.Runtime.ConstrainedExecution;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
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


        private bool navigatedTo = false;
        private bool startingService = false; // reentrancy protection

        //General
        private readonly int junkSize = 508;

        //Recieve File;
        StorageFile outFile;
        private IRandomAccessStream stream;
        private DataWriter memoryWriter;
        private bool memoryWriterInitialized = false;
        private string fileName = "noNameRecieved.txt";
        private readonly string defaultFilename = "noNameRecieved.txt";
        private int loop = 0;
        private Dictionary<int, byte[]> fileContent = new Dictionary<int, byte[]>();
        private int expectedJunks = 0;

        // To Send a file
        private string send_FileName = "noName.blob";
        private int send_NumberOfJunks;
        private uint send_currentJunk = 1;
        private Dictionary<uint, byte[]> fileToSend = new Dictionary<uint, byte[]>();

    

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
        private async void FileSelectButtonn_Click(object sender, RoutedEventArgs e)
        {
            var picker = new Windows.Storage.Pickers.FileOpenPicker();
            picker.ViewMode = Windows.Storage.Pickers.PickerViewMode.Thumbnail;
            picker.SuggestedStartLocation = Windows.Storage.Pickers.PickerLocationId.PicturesLibrary;
            picker.FileTypeFilter.Add("*");
            Windows.Storage.StorageFile file = await picker.PickSingleFileAsync();
            if (file != null)
            {
                // File was picked
                await LoadFileToSendAsync(file);
                rootPage.NotifyUser($"Selected file: {send_FileName} with {send_NumberOfJunks} Junks.", NotifyType.StatusMessage);

            }
            else
            {
                // No file was picked
                rootPage.NotifyUser("No file selected", NotifyType.ErrorMessage);
            }
        }


        private void UnsubscribeServiceEvents()
        {
            if (RecieveFileNameAndStartCharacteristic != null)
            {
                RecieveFileNameAndStartCharacteristic.WriteRequested -= RecieveFileNameAndStart_WriteRequestedAsync;
                RecieveFileNameAndStartCharacteristic.ReadRequested -=  RecieveFileNameAndStart_ReadRequestedAsync;
            }
            if (RecieveFileContentCharacterictic != null)
            {
                RecieveFileContentCharacterictic.WriteRequested -= RecieveFileContent_WriteRequestedAsync;
                RecieveFileContentCharacterictic.ReadRequested -= RecieveFileContent_ReadRequestedAsync;
            }
            if (RecieveFileFinishedCharacteristic != null)
            {
                RecieveFileFinishedCharacteristic.WriteRequested -= RecieveFileFinished_WriteRequestedAsync;
                RecieveFileFinishedCharacteristic.ReadRequested -= RecieveFileFinished_ReadRequestedAsync;
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
            // TODO        
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
            RecieveFileNameAndStartCharacteristic.ReadRequested += RecieveFileNameAndStart_ReadRequestedAsync;

            result = await provider.Service.CreateCharacteristicAsync(
                Constants.RecieveFileContentCharacteristicUuid, Constants.gattRecieveFileContentParameters);
            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create operand2 characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }

            RecieveFileContentCharacterictic = result.Characteristic;
            RecieveFileContentCharacterictic.WriteRequested += RecieveFileContent_WriteRequestedAsync;
            RecieveFileContentCharacterictic.ReadRequested += RecieveFileContent_ReadRequestedAsync;

            result = await provider.Service.CreateCharacteristicAsync(
                Constants.RecieveFileFinishedCharacteristicUuid, Constants.gattRecieveFileFinishedParameters);
            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create operator characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }

            RecieveFileFinishedCharacteristic = result.Characteristic;
            RecieveFileFinishedCharacteristic.WriteRequested += RecieveFileFinished_WriteRequestedAsync;
            RecieveFileFinishedCharacteristic.ReadRequested += RecieveFileFinished_ReadRequestedAsync;

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

        private async void RecieveFileNameAndStart_ReadRequestedAsync(GattLocalCharacteristic sender, GattReadRequestedEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine($"Recievied a read");


            // BT_Code: Processing a write request.
            using (args.GetDeferral())
            {
                // Get the request information.  This requires device access before an app can access the device's request.
                GattReadRequest request = await args.GetRequestAsync();
                if (request == null)
                {
                    // No access allowed to the device.  Application should indicate this to the user.
                    return;
                }
                ProcessReadCharacteristic(request, RecieveCharacteristics.RecieveFileNameAndStart);
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

        private async void RecieveFileContent_ReadRequestedAsync(GattLocalCharacteristic sender, GattReadRequestedEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine($"Recievied a read");


            // BT_Code: Processing a write request.
            using (args.GetDeferral())
            {
                // Get the request information.  This requires device access before an app can access the device's request.
                GattReadRequest request = await args.GetRequestAsync();
                if (request == null)
                {
                    // No access allowed to the device.  Application should indicate this to the user.
                    return;
                }
                ProcessReadCharacteristic(request, RecieveCharacteristics.RecieveFileContent);
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
        private async void RecieveFileFinished_ReadRequestedAsync(GattLocalCharacteristic sender, GattReadRequestedEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine($"Recievied a read");


            // BT_Code: Processing a write request.
            using (args.GetDeferral())
            {
                // Get the request information.  This requires device access before an app can access the device's request.
                GattReadRequest request = await args.GetRequestAsync();
                if (request == null)
                {
                    // No access allowed to the device.  Application should indicate this to the user.
                    return;
                }
                ProcessReadCharacteristic(request, RecieveCharacteristics.RecieveFileFinished);
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
        private void InitializeMemoryWriter()
        {
            if (memoryWriterInitialized) { return; }
            // Probably not needed. Also not sure if we need to cover the case, where the stream is currently being written to while we try to initialize a new one.

          
                    // BT_Code: Initialize the stream and writer.
                    stream = new Windows.Storage.Streams.InMemoryRandomAccessStream();
                    memoryWriter = new Windows.Storage.Streams.DataWriter(stream);
                    memoryWriter.ByteOrder = Windows.Storage.Streams.ByteOrder.LittleEndian;
                    memoryWriterInitialized = true;

                
            
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
            byte[] source;


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
                // These might arrive out of order.
                // We currently can handle a clean client that sends the right commands even out of order. We can not handle a client that sends commands multiple times (e.g. sending RecieveFileFinished twice in short successon).

                case RecieveCharacteristics.RecieveFileNameAndStart:
                    // BT_Code: Recieve the file name and start the file transfer.
                    source = GattHelper.Converters.GattConvert.ToByteArray(val);
                    expectedJunks = BitConverter.ToInt32(source.Take(4).ToArray(), 0);
                    fileName = System.Text.Encoding.UTF8.GetString(source.Skip(4).ToArray()); 
                    System.Diagnostics.Debug.WriteLine($"Starting a new File. File Name: {fileName} ");
                    System.Diagnostics.Debug.WriteLine($"Expected Junks: { expectedJunks}");
                    break;
                case RecieveCharacteristics.RecieveFileContent:
                    // If we recieve them out of order, we store them out of order :(
                    loop += 1;
                    System.Diagnostics.Debug.WriteLine($"Appending to file: {fileName} / Recieved: {FormatBytes(loop*512)}. ");
                    source = GattHelper.Converters.GattConvert.ToByteArray(val);
                    int JunkNumber = BitConverter.ToInt32(source.Take(4).ToArray(), 0);
                    fileContent.Add(JunkNumber, source.Skip(4).ToArray());
                    if(JunkNumber % 200 == 0)
                    {
                        rootPage.NotifyUser($"Recieved Junk {JunkNumber} of {expectedJunks} (Read {Math.Floor((double)JunkNumber / expectedJunks * 100)}%). ", NotifyType.StatusMessage);
                    }
                    break;
                case RecieveCharacteristics.RecieveFileFinished:
                    // Complete the request if needed
                   
                    System.Diagnostics.Debug.WriteLine($"RecieveFileFinished for file: {fileName} ");
                    while (expectedJunks > 0 & loop < expectedJunks)
                    {
                        System.Diagnostics.Debug.WriteLine($"Not all Junks recieved. Loop: {loop} ; expected: {expectedJunks} ");
                        System.Threading.Thread.Sleep(100);
                    }
                    InitializeMemoryWriter();
                    for (int i = 1; i <= expectedJunks; i++)
                    {
                        if (fileContent.ContainsKey(i))
                        {
                            memoryWriter.WriteBytes(fileContent[i]);
                        }
                        else
                        {
                            System.Diagnostics.Debug.WriteLine($"Junk {i} not found in fileContent. Skipping.");
                        }
                    }
                    await memoryWriter.StoreAsync();

                    StorageFile canvasFile = await DownloadsFolder.CreateFileAsync(fileName, CreationCollisionOption.GenerateUniqueName);
                        using (var reader = new DataReader(stream.GetInputStreamAt(0)))
                        {
                            await reader.LoadAsync((uint)stream.Size);
                            var buffer = new byte[(int)stream.Size];
                            reader.ReadBytes(buffer);
                            await FileIO.WriteBytesAsync(canvasFile, buffer);
                    }
                    fileName = defaultFilename;
                    memoryWriter.DetachStream();
                    memoryWriterInitialized = false;   
                    memoryWriter = null;
                    stream.Dispose();
                    stream = null; 
                    fileContent.Clear();    
                    loop = 0; // Reset the loop counter for the next file.
                    expectedJunks = 0;
                    await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, UpdateUI); // In the example this was done for each characteristic write, but caused performance issues. Hence we only do it at the end atm. 
                    break;
            }
            // Probably crashes both side, if we wait too long in the last RecieveFileFinished step.
            if (request.Option == GattWriteOption.WriteWithResponse)
            {
                request.Respond();
            }

            //
        }
        private void ProcessReadCharacteristic(GattReadRequest request, RecieveCharacteristics opCode)
        {
            System.Diagnostics.Debug.WriteLine($"Recievied an read of type: {opCode} ");
            switch (opCode)
            {
                case RecieveCharacteristics.RecieveFileNameAndStart:
                    // BT_Code: Recieve the file name and start the file transfer.
                    
                    byte[] filenameToSendAsBytes = Encoding.ASCII.GetBytes(send_FileName);
                    byte[] numberOfJunksAsBytes = BitConverter.GetBytes(send_NumberOfJunks);
                    byte[] bufferToSend = numberOfJunksAsBytes.Concat(filenameToSendAsBytes).ToArray();
                    request.RespondWithValue (GattHelper.Converters.GattConvert.ToIBuffer(bufferToSend));
                    break;
                case RecieveCharacteristics.RecieveFileContent:
                    // BT_Code: Recieve the file content.
                    System.Diagnostics.Debug.WriteLine($"RecieveFileContent Read Request. Offset: {request.Offset}");
                    if (fileToSend.Count == 0)
                    {
                        request.RespondWithProtocolError(GattProtocolError.InvalidAttributeValueLength);
                        return;
                    }
                    if (fileToSend.ContainsKey(send_currentJunk))
                    {
                        byte[] junkToSend = fileToSend[send_currentJunk];
                        byte[] offsetBuffer = BitConverter.GetBytes(send_currentJunk);
                        request.RespondWithValue(GattHelper.Converters.GattConvert.ToIBuffer(offsetBuffer.Concat(junkToSend).ToArray()));
                        send_currentJunk += 1;
                    }
                    else
                    {
                        // No more junks to send.
                        request.RespondWithProtocolError(GattProtocolError.InvalidAttributeValueLength);
                    }
                    break;
            }

        }
        private async Task LoadFileToSendAsync(StorageFile file)
        {
            fileToSend.Clear();
            send_FileName = file.Name;
            using (var stream = await file.OpenReadAsync())
            {
                var buffer = new byte[junkSize];
                uint index = 1;
                using (var inputStream = stream.AsStreamForRead())
                {
                    int bytesRead;
                    while ((bytesRead = await inputStream.ReadAsync(buffer, 0, junkSize)) > 0)
                    {
                        byte[] chunk = new byte[bytesRead];
                        Array.Copy(buffer, 0, chunk, 0, bytesRead);
                        fileToSend.Add(index, chunk);
                        index++;
                    }
                }
                send_NumberOfJunks = fileToSend.Count;
                send_currentJunk = 1;
            }
        }
    }
}
