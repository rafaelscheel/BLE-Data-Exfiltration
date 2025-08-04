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
using System.Runtime.InteropServices;
using Windows.ApplicationModel.Activation;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Foundation.Metadata;
using Windows.Security.Cryptography;
using Windows.Storage.Streams;
using Windows.UI.Xaml.Controls;

namespace SDKTemplate
{
    public partial class MainPage : Page
    {
        public const string FEATURE_NAME = "Bluetooth Low Energy C# Sample";

        List<Scenario> scenarios = new List<Scenario>
        {
            new Scenario() { Title="Client: Discover servers", ClassType=typeof(Scenario1_Discovery) },
            new Scenario() { Title="Client: Connect to a server", ClassType=typeof(Scenario2_Client) },
            new Scenario() { Title="Server: Publish foreground", ClassType=typeof(Scenario3_ServerForeground) },
            new Scenario() { Title="Server: Publish background", ClassType=typeof(Scenario4_ServerBackground) },
        };

        public string SelectedBleDeviceId;
        public string SelectedBleDeviceName = "No device selected";
    }

    public class Scenario
    {
        public string Title { get; set; }
        public Type ClassType { get; set; }
    }
}

namespace SDKTemplate
{
    sealed partial class App : Windows.UI.Xaml.Application
    {
        /// <summary>
        /// Override the Application.OnBackgroundActivated method to handle background activation in
        /// the main process. This entry point is used when BackgroundTaskBuilder.TaskEntryPoint is
        /// not set during background task registration.
        /// </summary>
        /// <param name="args"></param>
        protected override void OnBackgroundActivated(BackgroundActivatedEventArgs args)
        {
            // Use the args.TaskInstance.Name and/or args.TaskInstance.InstanceId to determine
            // which background task to run. This sample has only one background task,
            // so can just start that task without needing to check.
            var activity = new CalculatorServerBackgroundTask();
            activity.Run(args.TaskInstance);
        }
    }

    static class FeatureDetection
    {
        // Reports whether the extended advertising and scanning features are supported:
        //
        // BluetoothAdapter.IsLowEnergyUncoded2MPhySupported property
        // BluetoothAdapter.IsLowEnergyCodedPhySupported property
        // BluetoothLEAdvertisementReceivedEventArgs.PrimaryPhy property
        // BluetoothLEAdvertisementReceivedEventArgs.SecondaryPhy property
        // BluetoothLEAdvertisementPublisher.PrimaryPhy property
        // BluetoothLEAdvertisementPublisher.SecondaryPhy property
        // BluetoothLEAdvertisementPublisherTrigger.PrimaryPhy property
        // BluetoothLEAdvertisementPublisherTrigger.SecondaryPhy property
        // BluetoothLEAdvertisementScanParameters class
        // BluetoothLEAdvertisementWatcher.UseUncoded1MPhy property
        // BluetoothLEAdvertisementWatcher.UseCodedPhy property
        // BluetoothLEAdvertisementWatcher.ScanParameters property
        // BluetoothLEAdvertisementWatcher.UseHardwareFilter property
        // BluetoothLEAdvertisementWatcherTrigger.UseUncoded1MPhy property
        // BluetoothLEAdvertisementWatcherTrigger.UseCodedPhy property
        // BluetoothLEAdvertisementWatcherTrigger.ScanParameters property
        // GattServiceProvider.UpdateAdvertisingParameters method
        // GattServiceProviderConnection.UpdateAdvertisingParameters method
        // GattServiceProviderAdvertisingParameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy property
        // GattServiceProviderAdvertisingParameters.UseLowEnergyUncoded2MPhyAsSecondaryPhy property
        //
        // All of these features are available as a group, so testing one of them is sufficient to
        // check for the presence of all.
        public static bool AreExtendedAdvertisingPhysAndScanParametersSupported => LazyAreExtendedAdvertisingPhysAndScanParametersSupported.Value;

        private static Lazy<bool> LazyAreExtendedAdvertisingPhysAndScanParametersSupported = new Lazy<bool>(DetectExtendedAdvertisingPhysAndScanParameters);

        // Declare a dummy version of IGattServiceProviderAdvertisingParameters3 because all we need is the UUID.
        [ComImport]
        [System.Runtime.InteropServices.Guid("A23546B2-B216-5929-9055-F1313DD53E2A")]
        [InterfaceType(ComInterfaceType.InterfaceIsIInspectable)]
        private interface IGattServiceProviderAdvertisingParameters3
        {
        };
        private static bool DetectExtendedAdvertisingPhysAndScanParameters()
        {
            // We will use GattServiceProviderAdvertisingParameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy
            // to detect this feature group.
            bool isPresentInMetadata = ApiInformation.IsPropertyPresent(
                typeof(GattServiceProviderAdvertisingParameters).FullName,
                nameof(GattServiceProviderAdvertisingParameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy));
            if (!isPresentInMetadata)
            {
                return false;
            }

            // The feature is present in metadata. See if it is available at runtime.
            // During the initial rollout of the feature, it may be unavailable at runtime
            // despite being declared in metadata.
            return (object)(new GattServiceProviderAdvertisingParameters()) is IGattServiceProviderAdvertisingParameters3;
        }
    }

    static class BufferHelpers
    {
        public static int? Int32FromBuffer(IBuffer buffer)
        {
            CryptographicBuffer.CopyToByteArray(buffer, out byte[] data);
            if (data.Length != sizeof(int))
            {
                return null;
            }

            return BitConverter.ToInt32(data, 0);
        }

        public static IBuffer BufferFromInt32(int value)
        {
            return CryptographicBuffer.CreateFromByteArray(BitConverter.GetBytes(value));
        }
    }
}
