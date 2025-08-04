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
using System.Threading.Tasks;
using Windows.ApplicationModel.Background;
using Windows.Devices.Bluetooth.Background;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Foundation.Collections;
using Windows.Storage;

namespace SDKTemplate
{
    public sealed class CalculatorServerBackgroundTask : IBackgroundTask
    {
        // Background
        private BackgroundTaskDeferral deferral = null;
        private IBackgroundTaskInstance taskInstance = null;

        // Gatt
        private GattServiceProviderConnection serviceConnection = null;

        private GattLocalCharacteristic op1Characteristic;
        private GattLocalCharacteristic op2Characteristic;
        private GattLocalCharacteristic operatorCharacteristic;
        private GattLocalCharacteristic resultCharacteristic;

        private int operand1Received = 0;
        private int operand2Received = 0;
        CalculatorOperators operatorReceived = 0;
        private int resultVal = 0;

        private IPropertySet calculatorValues;

        public static event Action ValuesChanged;

        private enum CalculatorCharacteristics
        {
            Operand1 = 1,
            Operand2 = 2,
            Operator = 3
        }

        private enum CalculatorOperators
        {
            Add = 1,
            Subtract = 2,
            Multiply = 3,
            Divide = 4
        }

        public void Run(IBackgroundTaskInstance taskInstance)
        {
            deferral = taskInstance.GetDeferral();
            // Setup our onCanceled callback and progress
            this.taskInstance = taskInstance;
            this.taskInstance.Canceled += CompleteTask;

            var triggerDetails = taskInstance.TriggerDetails as GattServiceProviderTriggerDetails;
            serviceConnection = triggerDetails.Connection;

            InitializeService();
        }

        // The app sets up GATT operation event delegates and indicates it is ready to receive requests.
        private void InitializeService()
        {
            // Create (or obtain existing) settings container for the calculator service.
            calculatorValues = ApplicationData.Current.LocalSettings.
                CreateContainer(Constants.CalculatorContainerName, ApplicationDataCreateDisposition.Always).Values;

            // Start with a blank slate.
            calculatorValues.Clear();

            // Let the UI know that the values have bbeen reset, so it can update.
            ValuesChanged?.Invoke();

            // Bind our characteristic handlers
            foreach (var characteristic in serviceConnection.Service.Characteristics)
            {
                if (characteristic.Uuid == Constants.BackgroundOperand1Uuid)
                {
                    op1Characteristic = characteristic;
                    op1Characteristic.WriteRequested += Op1Characteristic_WriteRequestedAsync;

                }
                else if (characteristic.Uuid == Constants.BackgroundOperand2Uuid)
                {
                    op2Characteristic = characteristic;
                    op2Characteristic.WriteRequested += Op2Characteristic_WriteRequestedAsync;

                }
                else if (characteristic.Uuid == Constants.BackgroundOperatorUuid)
                {
                    operatorCharacteristic = characteristic;
                    operatorCharacteristic.WriteRequested += OperatorCharacteristic_WriteRequestedAsync;
                }
                else if (characteristic.Uuid == Constants.BackgroundResultUuid)
                {
                    resultCharacteristic = characteristic;
                    resultCharacteristic.ReadRequested += ResultCharacteristic_ReadRequestedAsync;
                    resultCharacteristic.SubscribedClientsChanged += ResultCharacteristic_SubscribedClientsChanged;
                }
            }

            if (op1Characteristic == null)
            {
                throw new Exception("Could not restore operand1 characteristic");
            }
            if (op2Characteristic == null)
            {
                throw new Exception("Could not restore operand2 characteristic");
            }
            if (operatorCharacteristic == null)
            {
                throw new Exception("Could not restore operator characteristic");
            }
            if (resultCharacteristic == null)
            {
                throw new Exception("Could not restore result characteristic");
            }

            // Report that we are ready to begin receiving events.
            serviceConnection.Start();
        }

        // Upon task completion, cancel all callbacks and remove the associated handler for the background task.
        private void CompleteTask(IBackgroundTaskInstance taskInstance, BackgroundTaskCancellationReason reason)
        {
            if (op1Characteristic != null)
            {
                op1Characteristic.WriteRequested -= Op1Characteristic_WriteRequestedAsync;
                op1Characteristic = null;
            }
            if (op2Characteristic != null)
            {
                op2Characteristic.WriteRequested -= Op2Characteristic_WriteRequestedAsync;
                op2Characteristic = null;
            }
            if (operatorCharacteristic != null)
            {
                operatorCharacteristic.WriteRequested -= OperatorCharacteristic_WriteRequestedAsync;
                operatorCharacteristic = null;
            }
            if (resultCharacteristic != null)
            {
                resultCharacteristic.ReadRequested -= ResultCharacteristic_ReadRequestedAsync;
                resultCharacteristic.SubscribedClientsChanged -= ResultCharacteristic_SubscribedClientsChanged;
                resultCharacteristic = null;
            }

            // Inform the task as completed
            if (deferral != null)
            {
                deferral.Complete();
            }
        }

        private async void Op1Characteristic_WriteRequestedAsync(GattLocalCharacteristic sender, GattWriteRequestedEventArgs args)
        {
            await ProcessWriteCharacteristicAndCalculation(args, CalculatorCharacteristics.Operand1);
        }

        private async void Op2Characteristic_WriteRequestedAsync(GattLocalCharacteristic sender, GattWriteRequestedEventArgs args)
        {
            await ProcessWriteCharacteristicAndCalculation(args, CalculatorCharacteristics.Operand2);
        }

        private async void OperatorCharacteristic_WriteRequestedAsync(GattLocalCharacteristic sender, GattWriteRequestedEventArgs args)
        {
            await ProcessWriteCharacteristicAndCalculation(args, CalculatorCharacteristics.Operator);
        }

        /// <summary>
        /// BT_Code: Processing a write request.Takes in a GATT Write request and updates UX based on opcode.
        /// </summary>
        /// <param name="request"></param>
        /// <param name="opCode">Operand (1 or 2) and Operator (3)</param>
        private async Task ProcessWriteCharacteristicAndCalculation(GattWriteRequestedEventArgs args, CalculatorCharacteristics opCode)
        {
            using (var deferral = args.GetDeferral())
            {
                var request = await args.GetRequestAsync();

                int? val = BufferHelpers.Int32FromBuffer(request.Value);
                if (val == null)
                {
                    // Input is the wrong length. Respond with a protocol error if requested.
                    if (request.Option == GattWriteOption.WriteWithResponse)
                    {
                        request.RespondWithProtocolError(GattProtocolError.InvalidAttributeValueLength);
                    }
                    return;
                }

                // Record the written value and also save it in a place the foreground UI can see.
                switch (opCode)
                {
                    case CalculatorCharacteristics.Operand1:
                        operand1Received = val.Value;
                        calculatorValues["Operand1"] = val;
                        break;
                    case CalculatorCharacteristics.Operand2:
                        operand2Received = val.Value;
                        calculatorValues["Operand2"] = val;
                        break;
                    case CalculatorCharacteristics.Operator:
                        if (Enum.IsDefined(typeof(CalculatorOperators), val))
                        {
                            operatorReceived = (CalculatorOperators)val.Value;
                            calculatorValues["Operator"] = val.Value;
                        }
                        else
                        {
                            if (request.Option == GattWriteOption.WriteWithResponse)
                            {
                                request.RespondWithProtocolError(GattProtocolError.InvalidPdu);
                            }
                            return;
                        }
                        break;
                }

                // Check if the operator is valid and compute the result.
                if (Enum.IsDefined(typeof(CalculatorOperators), operatorReceived))
                {
                    ComputeResult();
                }

                // Let the UI know that the values have changed, so it can update.
                ValuesChanged?.Invoke();

                // Complete the request if needed
                if (request.Option == GattWriteOption.WriteWithResponse)
                {
                    request.Respond();
                }
            }
        }

        private async void ResultCharacteristic_ReadRequestedAsync(GattLocalCharacteristic sender, GattReadRequestedEventArgs args)
        {
            var deferral = args.GetDeferral();

            // Get the request information.  This requires device access before an app can access the device's request.
            var request = await args.GetRequestAsync();
            if (request == null)
            {
                // No access allowed to the device.  Application should indicate this to the user.
                //rootPage.NotifyUser("Access to device not allowed", NotifyType.ErrorMessage);
                return;
            }

            // Can get details about the request such as the size and offset, as well as monitor the state to see if it has been completed/cancelled externally.
            // request.Offset
            // request.Length
            // request.State
            // request.StateChanged += <Handler>

            // Gatt code to handle the response
            request.RespondWithValue(BufferHelpers.BufferFromInt32(resultVal));
            deferral.Complete();
        }

        private void ResultCharacteristic_SubscribedClientsChanged(GattLocalCharacteristic sender, object args)
        {
            // This function could be used to monitor the clients that have subscribed to the characteristic and perform actions based on the clients that have subscribed.
        }

        private async void ComputeResult()
        {
            switch (operatorReceived)
            {
                case CalculatorOperators.Add:
                    resultVal = operand1Received + operand2Received;
                    break;
                case CalculatorOperators.Subtract:
                    resultVal = operand1Received - operand2Received;
                    break;
                case CalculatorOperators.Multiply:
                    resultVal = operand1Received * operand2Received;
                    break;
                case CalculatorOperators.Divide:
                    if (operand2Received == 0 || (operand1Received == -0x80000000 && operand2Received == -1))
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

            calculatorValues["Result"] = resultVal;

            // BT_Code: Returns a collection of all clients that the notification was attempted and the result.
            IReadOnlyList<GattClientNotificationResult> results = await resultCharacteristic.NotifyValueAsync(BufferHelpers.BufferFromInt32(resultVal));

            foreach (var result in results)
            {
                // An application can iterate through each registered client that was notified and retrieve the results:
                //
                // result.SubscribedClient: The details on the remote client.
                // result.Status: The GattCommunicationStatus
                // result.ProtocolError: iff Status == GattCommunicationStatus.ProtocolError
            }
        }
    }
}
