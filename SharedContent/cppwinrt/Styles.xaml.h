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

#include "Styles.g.h"

namespace winrt::SDKTemplate::implementation
{
    struct Styles : StylesT<Styles>
    {
        Styles()
        {
        }
    };
}

namespace winrt::SDKTemplate::factory_implementation
{
    struct Styles : StylesT<Styles, implementation::Styles>
    {
    };
}
