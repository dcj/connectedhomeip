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
#include <AppMain.h>
#include <EnergyManagementAppCmdLineOptions.h>
#include <device-energy-management-stub.h>
#include <electrical-alarm-stub.h>
#include <electrical-distribution-stub.h>
#include <electrical-protection-alarm-stub.h>
#include <power-topology-stub.h>

#include <app-common/zap-generated/ids/Clusters.h>
#include <app/util/attribute-storage.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/TypeTraits.h>

#include <cstdlib>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

namespace {

constexpr EndpointId kEnclosureEndpointId = 1; // Electrical Distribution Enclosure (0x0517)
constexpr EndpointId kBreakerEndpointId   = 2; // Electrical Circuit Breaker (0x0516)

// The enclosure device type requires each breaker child endpoint to carry a semantic tag from the
// Common Number namespace, so this one is breaker number one.
constexpr uint8_t kCommonNumberNamespaceId = 0x07;
constexpr uint8_t kNumberOneTag            = 0x01;

const Descriptor::Structs::SemanticTagStruct::Type kBreakerTagList[] = {
    { .namespaceID = kCommonNumberNamespaceId, .tag = kNumberOneTag },
};

constexpr uint16_t kOptionFeatureMap = 0xffd1;

// Device Energy Management features; --featureSet overrides.
BitMask<DeviceEnergyManagement::Feature> sDemFeatureMap(DeviceEnergyManagement::Feature::kPowerRangeAdjustment);

bool EnergyAppOptionHandler(const char * aProgram, ArgParser::OptionSet * aOptions, int aIdentifier, const char * aName,
                            const char * aValue)
{
    if (aIdentifier != kOptionFeatureMap)
    {
        ChipLogError(Support, "%s: INTERNAL ERROR: Unhandled option: %s", aProgram, aName);
        return false;
    }
    // Base 0 accepts both 0x-prefixed hex and decimal. The data model carries only the
    // PowerRangeAdjustment attributes, commands and events, so no other feature set is valid.
    char * end           = nullptr;
    unsigned long value  = strtoul(aValue, &end, 0);
    const uint32_t kOnly = to_underlying(DeviceEnergyManagement::Feature::kPowerRangeAdjustment);
    if (end == aValue || *end != '\0' || value != kOnly)
    {
        ChipLogError(Support, "%s: --featureSet must be 0x%x (PowerRangeAdjustment), got '%s'", aProgram,
                     static_cast<unsigned>(kOnly), aValue);
        return false;
    }
    sDemFeatureMap = BitMask<DeviceEnergyManagement::Feature>(static_cast<uint32_t>(value));
    ChipLogDetail(Support, "Using DEM FeatureMap 0x%04x", sDemFeatureMap.Raw());
    return true;
}

ArgParser::OptionDef sEnergyAppOptionDefs[] = { { "featureSet", ArgParser::kArgumentRequired, kOptionFeatureMap }, { nullptr } };

ArgParser::OptionSet sCmdLineOptions = {
    EnergyAppOptionHandler, sEnergyAppOptionDefs, "PROGRAM OPTIONS",
    "  --featureSet <value>\n"
    "       Device Energy Management FeatureMap; only 0x80 (PowerRangeAdjustment, the default) is accepted.\n"
};

} // namespace

namespace chip::app::Clusters::DeviceEnergyManagement {

BitMask<Feature> GetFeatureMap()
{
    return sDemFeatureMap;
}

} // namespace chip::app::Clusters::DeviceEnergyManagement

void ApplicationInit()
{
    // Electrical Distribution describes the enclosure's physical characteristics. Its generated
    // Init callback is a no-op, so the app owns the instance and registers it imperatively.
    VerifyOrDie(ElectricalDistribution::ElectricalDistributionInit(kEnclosureEndpointId) == CHIP_NO_ERROR);

    // Electrical Protection Alarm reports the enclosure's safety faults. Same imperative
    // registration: its generated Init callback is a no-op too.
    VerifyOrDie(ElectricalProtectionAlarm::ElectricalProtectionAlarmInit(kEnclosureEndpointId) == CHIP_NO_ERROR);

    // Power Topology is mandatory on both device types, but with different features. The
    // enclosure needs only a topology choice; the breaker device type additionally makes
    // ElectricalCircuit mandatory, which brings the ElectricalCircuitNodes attribute.
    VerifyOrDie(PowerTopology::PowerTopologyInit(
                    kEnclosureEndpointId, BitMask<PowerTopology::Feature>(PowerTopology::Feature::kTreeTopology)) == CHIP_NO_ERROR);
    VerifyOrDie(PowerTopology::PowerTopologyInit(kBreakerEndpointId,
                                                 BitMask<PowerTopology::Feature>(PowerTopology::Feature::kTreeTopology,
                                                                                 PowerTopology::Feature::kElectricalCircuit)) ==
                CHIP_NO_ERROR);

    VerifyOrDie(SetTagList(kBreakerEndpointId, Span<const Descriptor::Structs::SemanticTagStruct::Type>(kBreakerTagList)) ==
                CHIP_NO_ERROR);

    // Electrical Alarm reports measurement-threshold alarms on the breaker. The device type makes
    // OverCurrent mandatory and leaves the rest optional; this app offers every feature so that a
    // test script gating on any one of them has a DUT. Each alarm-class feature exposes its own
    // threshold attribute, and ADJUST exposes SetElectricalAlarmThresholds over all of them.
    VerifyOrDie(ElectricalAlarm::ElectricalAlarmInit(kBreakerEndpointId, ElectricalAlarm::kAllFeatures) == CHIP_NO_ERROR);

    // Device Energy Management lets an EMS bound the whole enclosure's power range.
    VerifyOrDie(DeviceEnergyManagement::DeviceEnergyManagementStubInit(kEnclosureEndpointId,
                                                                       DeviceEnergyManagement::GetFeatureMap()) == CHIP_NO_ERROR);
}

void ApplicationShutdown()
{
    DeviceEnergyManagement::DeviceEnergyManagementStubShutdown();
    ElectricalAlarm::ElectricalAlarmShutdown();
    ElectricalProtectionAlarm::ElectricalProtectionAlarmShutdown();
    PowerTopology::PowerTopologyShutdown();
    ElectricalDistribution::ElectricalDistributionShutdown();
}

int main(int argc, char * argv[])
{
    if (ChipLinuxAppInit(argc, argv, &sCmdLineOptions) != 0)
    {
        return -1;
    }

    ChipLinuxAppMainLoop();
    return 0;
}
