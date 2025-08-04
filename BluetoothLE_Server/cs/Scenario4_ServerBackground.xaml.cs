using System;
using System.Threading.Tasks;
using Windows.ApplicationModel.Background;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Background;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Foundation.Collections;
using Windows.Storage;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Navigation;

namespace SDKTemplate
{
    public sealed partial class Scenario4_ServerBackground : Page
    {
        private MainPage rootPage = MainPage.Current;

        // The background task registration for the background advertisement publisher
        private IBackgroundTaskRegistration taskRegistration;
        private const string taskName = "GattServerBackgroundTask";
        private const string triggerId = "backgroundGattCalculatorService";

        private GattServiceProviderTrigger serviceProviderTrigger;

        private IPropertySet calculatorValues;

        private bool navigatedTo = false;
        private bool startingService = false; // reentrancy protection

        private enum CalculatorOperators
        {
            Invalid = 0,
            Add = 1,
            Subtract = 2,
            Multiply = 3,
            Divide = 4
        }

        public Scenario4_ServerBackground()
        {
            InitializeComponent();

            // Create (or obtain existing) settings container for the calculator service.
            calculatorValues = ApplicationData.Current.LocalSettings.
                CreateContainer(Constants.CalculatorContainerName, ApplicationDataCreateDisposition.Always).Values;

            ServiceIdRun.Text = Constants.BackgroundCalculatorServiceUuid.ToString();
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            navigatedTo = true;

            // Find the task if we previously registered it
            foreach (var task in BackgroundTaskRegistration.AllTasks.Values)
            {
                //In a real scenario, if the app detects that a background task is currently running,
                // it should signal the background task and retrieve the necessary information to update the UI.
                if (task.Name == taskName)
                {
                    taskRegistration = task;
                    rootPage.NotifyUser("Background publisher already registered.", NotifyType.StatusMessage);
                    PublishButton.Content = "Stop service";
                    break;
                }
            }

            // BT_Code: New for Creator's Update - Bluetooth adapter has properties of the local BT radio.
            BluetoothAdapter adapter = await BluetoothAdapter.GetDefaultAsync();

            // Check whether the default Bluetooth adapter can act as a server.
            if (adapter != null && adapter.IsPeripheralRoleSupported)
            {
                ServerPanel.Visibility = Visibility.Visible;

                // Check whether the local Bluetooth adapter and Windows support 2M and Coded PHY.
                if (!FeatureDetection.AreExtendedAdvertisingPhysAndScanParametersSupported)
                {
                    Publishing2MPHYReasonRun.Text = "(Not supported by this version of Windows)";
                }
                else if (adapter.IsLowEnergyUncoded2MPhySupported)
                {
                    Publishing2MPHY.IsEnabled = true;
                }
                else
                {
                    Publishing2MPHYReasonRun.Text = "(Not supported by default Bluetooth adapter)";
                }
            }
            else
            {
                PeripheralWarning.Visibility = Visibility.Visible;
            }

            // Register handlers to stop updating UI when suspending and restart when resuming.
            Application.Current.Suspending += App_Suspending;
            Application.Current.Resuming += App_Resuming;

            StartUpdatingUI();
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            navigatedTo = false;

            Application.Current.Suspending -= App_Suspending;
            Application.Current.Resuming -= App_Resuming;

            StopUpdatingUI();
        }

        // Update the UI with the latest values reported by the calculator server.
        private void UpdateUI()
        {
            if (calculatorValues.TryGetValue("Operand1", out object arg1))
            {
                Operand1TextBox.Text = arg1.ToString();
            }
            else
            {
                Operand1TextBox.Text = "N/A";
            }

            string operationText = "N/A";
            if (calculatorValues.TryGetValue("Operator", out object o) && o is int op)
            {
                switch ((CalculatorOperators)op)
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
            }
            OperationTextBox.Text = operationText;

            if (calculatorValues.TryGetValue("Operand2", out object arg2))
            {
                Operand2TextBox.Text = arg2.ToString();
            }
            else
            {
                Operand2TextBox.Text = "N/A";
            }

            if (calculatorValues.TryGetValue("Result", out object result))
            {
                ResultTextBox.Text = result.ToString();
            }
            else
            {
                ResultTextBox.Text = "N/A";
            }

        }

        private async void OnCalculatorValuesChanged()
        {
            // This event is raised from the background task, so dispatch to the UI thread.
            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, UpdateUI);
        }

        private void StartUpdatingUI()
        {
            // Start listening for changes from the calculator service.
            CalculatorServerBackgroundTask.ValuesChanged += OnCalculatorValuesChanged;

            // Force an immediate update to show the latest values.
            UpdateUI();
        }

        private void StopUpdatingUI()
        {
            // Stop listening for changes from the calculator service.
            CalculatorServerBackgroundTask.ValuesChanged -= OnCalculatorValuesChanged;
        }

        /// <summary>
        /// Invoked when application execution is being suspended.
        /// </summary>
        /// <param name="sender">The source of the suspend request.</param>
        /// <param name="e">Details about the suspend request.</param>
        private void App_Suspending(object sender, Windows.ApplicationModel.SuspendingEventArgs e)
        {
            StopUpdatingUI();
        }

        /// <summary>
        /// Invoked when application execution is being resumed.
        /// </summary>
        /// <param name="sender">The source of the resume request.</param>
        /// <param name="e"></param>
        private void App_Resuming(object sender, object e)
        {
            StartUpdatingUI();
        }

        // Event handlers for the advertise parameters change button.
        private void Publishing2MPHY_Click(object sender, RoutedEventArgs e)
        {
            if (taskRegistration == null)
            {
                // The service is not registered. We will set the parameters when we register the service.
                return;
            }

            // Look for the service so we can update its advertising parameters on the fly.
            if (GattServiceProviderConnection.AllServices.TryGetValue(triggerId, out GattServiceProviderConnection connection))
            {
                // Found it. Update the advertising parameters.
                GattServiceProviderAdvertisingParameters parameters = serviceProviderTrigger.AdvertisingParameters;

                bool shouldAdvertise2MPHY = Publishing2MPHY.IsChecked.Value;

                parameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy = !shouldAdvertise2MPHY;
                parameters.UseLowEnergyUncoded2MPhyAsSecondaryPhy = shouldAdvertise2MPHY;
                connection.UpdateAdvertisingParameters(parameters);
            }
        }

        private async void PublishOrStopButton_Click(object sender, RoutedEventArgs e)
        {
            // Register or stop a background publisherTrigger. It will start background advertising if register successfully.
            // First get the existing tasks to see if we already registered for it, if that is the case, stop and unregister the existing task.
            if (taskRegistration == null)
            {
                // Don't try to start if already starting.
                if (startingService)
                {
                    return;
                }

                PublishButton.Content = "Starting...";
                startingService = true;
                await CreateBackgroundServiceAsync();
                startingService = false;
            }
            else
            {
                taskRegistration.Unregister(true);
                taskRegistration = null;
            }
            PublishButton.Content = taskRegistration == null ? "Start Service" : "Stop Service";
        }

        private async Task CreateBackgroundServiceAsync()
        {
            GattServiceProviderTriggerResult serviceResult =
                await GattServiceProviderTrigger.CreateAsync(triggerId, Constants.BackgroundCalculatorServiceUuid);

            if (serviceResult.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create background calculator service: {serviceResult.Error}", NotifyType.ErrorMessage);
                return;
            }

            serviceProviderTrigger = serviceResult.Trigger;

            // Create the operand1 characteristic.
            GattLocalCharacteristicResult result =
                await serviceProviderTrigger.Service.CreateCharacteristicAsync(
                    Constants.BackgroundOperand1Uuid, Constants.gattRecieveFileNameAndStartParameters);

            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create operand1 characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }

            // Create the operand2 characteristic.
            result = await serviceProviderTrigger.Service.CreateCharacteristicAsync(
                Constants.BackgroundOperand2Uuid, Constants.gattRecieveFileContentParameters);
            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create operand2 characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }

            // Create the operator characteristic.
            result = await serviceProviderTrigger.Service.CreateCharacteristicAsync(
                Constants.BackgroundOperatorUuid, Constants.gattRecieveFileFinishedParameters);

            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create operator characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }

            // Create the result characteristic.
            result = await serviceProviderTrigger.Service.CreateCharacteristicAsync(
                Constants.BackgroundResultUuid, Constants.gattResultParameters);
            if (result.Error != BluetoothError.Success)
            {
                rootPage.NotifyUser($"Could not create result characteristic: {result.Error}", NotifyType.ErrorMessage);
                return;
            }

            // Configure the advertising parameters.
            GattServiceProviderAdvertisingParameters parameters = serviceProviderTrigger.AdvertisingParameters;
            parameters.IsConnectable = true;
            parameters.IsDiscoverable = true;

            if (Publishing2MPHY.IsChecked.Value)
            {
                parameters.UseLowEnergyUncoded1MPhyAsSecondaryPhy = false;
                parameters.UseLowEnergyUncoded2MPhyAsSecondaryPhy = true;
            }

            // Applications registering for background publisherTrigger must request for permission.
            BackgroundAccessStatus backgroundAccessStatus = await BackgroundExecutionManager.RequestAccessAsync();
            // Here, we do not fail the registration even if the access is not granted. Instead, we allow
            // the publisherTrigger to be registered and when the access is granted for the Application at a later time,
            // the publisherTrigger will automatically start working again.

            // Last chance: Did the user navigate away while we were doing all this work?
            // If so, then abandon our work without starting the provider.
            // Must do this after the last await. (Could also do it after earlier awaits.)
            if (!navigatedTo)
            {
                return;
            }

            // At this point we assume we haven't found any existing tasks matching the one we want to register
            // First, configure the task trigger and name.
            // (Leaving the task entry point blank makes it an in-process background task. This trigger supports both
            // in-process and out-of-process background tasks, but in-process is simpler.)
            var builder = new BackgroundTaskBuilder();
            builder.Name = taskName;
            builder.SetTrigger(serviceProviderTrigger);

            // Now perform the registration.
            taskRegistration = builder.Register();
            // Even though the publisherTrigger is registered successfully, it might be blocked. Notify the user if that is the case.
            if ((backgroundAccessStatus == BackgroundAccessStatus.AlwaysAllowed) ||
                (backgroundAccessStatus == BackgroundAccessStatus.AllowedSubjectToSystemPolicy))
            {
                rootPage.NotifyUser("Background publisher registered.", NotifyType.StatusMessage);
            }
            else
            {
                rootPage.NotifyUser("Background tasks may be disabled for this app", NotifyType.ErrorMessage);
            }
        }
    }
}

