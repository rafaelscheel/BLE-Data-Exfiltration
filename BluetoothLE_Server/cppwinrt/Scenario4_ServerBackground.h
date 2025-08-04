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

#pragma once

#include "Scenario4_ServerBackground.g.h"
#include "MainPage.h"

namespace winrt::SDKTemplate::implementation
{
    struct Scenario4_ServerBackground : Scenario4_ServerBackgroundT<Scenario4_ServerBackground>
    {
        enum class CalculatorOperators
        {
            Invalid = 0,
            Add = 1,
            Subtract = 2,
            Multiply = 3,
            Divide = 4
        };

        Scenario4_ServerBackground();
        fire_and_forget OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs const&);
        void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs const&);
        fire_and_forget PublishOrStopButton_Click(IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);
        void Publishing2MPHY_Click(IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&);

    private:
        SDKTemplate::MainPage rootPage{ MainPage::Current() };

        Windows::ApplicationModel::Background::IBackgroundTaskRegistration taskRegistration;
        static constexpr wchar_t taskName[] = L"GattServerBackgroundTask";
        static constexpr wchar_t triggerId[] = L"BackgroundGattCalculatorService";

        Windows::ApplicationModel::Background::GattServiceProviderTrigger serviceProviderTrigger{ nullptr };
        Windows::Foundation::Collections::IPropertySet calculatorValues;

        bool navigatedTo = false;
        bool startingService = false; // reentrancy protection

        event_token appSuspendingToken{};
        event_token appResumingToken{};
        event_token calculatorValuesChangedToken{};

        void UpdateUI();
        fire_and_forget OnCalculatorValuesChanged();
        void StartUpdatingUI();
        void StopUpdatingUI();

        void App_Suspending(IInspectable const& sender, Windows::ApplicationModel::SuspendingEventArgs const& e);
        void App_Resuming(IInspectable const& sender, IInspectable const& e);

        Windows::Foundation::IAsyncAction CreateBackgroundServiceAsync();

    };
}

namespace winrt::SDKTemplate::factory_implementation
{
    struct Scenario4_ServerBackground : Scenario4_ServerBackgroundT<Scenario4_ServerBackground, implementation::Scenario4_ServerBackground>
    {
    };
}
