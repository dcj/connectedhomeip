/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <device-energy-management-stub.h>

#include <DEMConfig.h>
#include <DeviceEnergyManagementManager.h>

#include <lib/support/CodeUtils.h>

#include <memory>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::DeviceEnergyManagement;

namespace {

// Representative whole-panel limits: 200 A at 240 V, consumption only.
constexpr int64_t kAbsMinPowerMw = 0;
constexpr int64_t kAbsMaxPowerMw = 48'000'000;

std::unique_ptr<DeviceEnergyManagementDelegate> gDelegate;
std::unique_ptr<DeviceEnergyManagementManager> gInstance;

} // namespace

// Used by the shared DEM test event triggers.
DeviceEnergyManagementDelegate * GetDEMDelegate()
{
    VerifyOrDieWithMsg(gDelegate != nullptr, AppServer, "DEM Delegate is null");
    return gDelegate.get();
}

namespace chip::app::Clusters::DeviceEnergyManagement {

CHIP_ERROR DeviceEnergyManagementStubInit(EndpointId endpointId, BitMask<Feature> features)
{
    ReturnErrorOnFailure(DeviceEnergyManagementInit(endpointId, gDelegate, gInstance, features));

    ReturnErrorOnFailure(gDelegate->SetESAType(ESATypeEnum::kOther));
    ReturnErrorOnFailure(gDelegate->SetESACanGenerate(false));
    ReturnErrorOnFailure(gDelegate->SetAbsMinPower(kAbsMinPowerMw));
    ReturnErrorOnFailure(gDelegate->SetAbsMaxPower(kAbsMaxPowerMw));
    return gDelegate->SetESAState(ESAStateEnum::kOnline);
}

void DeviceEnergyManagementStubShutdown()
{
    DeviceEnergyManagementShutdown(gInstance, gDelegate);
}

} // namespace chip::app::Clusters::DeviceEnergyManagement
