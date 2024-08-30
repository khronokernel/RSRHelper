//
//  kern_start.cpp
//  RSRHelper.kext
//
//  Copyright © 2023 Mykola Grymalyuk. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_user.hpp>
#include <IOKit/IOPlatformExpert.h>

#define MODULE_SHORT "rsr_help"

static mach_vm_address_t orig_validateKCUUID {};

typedef enum kc_kind {
    KCKindNone      = -1,
    KCKindUnknown   = 0,
    KCKindPrimary   = 1,
    KCKindPageable  = 2,
    KCKindAuxiliary = 3,
    KCNumKinds      = 4,
} kc_kind_t;


#pragma mark - Patched functions

int patched_validateKCUUID(uuid_t *loaded_kcuuid, kc_kind_t type, OSDictionary *infoDict, const char *uuid_key) {
    /*
     Source:
        13.1: https://github.com/apple-oss-distributions/xnu/blob/xnu-8792.61.2/libkern/c++/OSKext.cpp#L13955-L14013
     */
    
    int kc_result = FunctionCast(patched_validateKCUUID, orig_validateKCUUID)(loaded_kcuuid, type, infoDict, uuid_key);

    // Non-zero means there's a fault
    if (kc_result) {
        SYSLOG(MODULE_SHORT, "determined KC UUID mismatch occured, waiting 30s for userspace binary to finish repair");
        IOSleep(15000);
        SYSLOG(MODULE_SHORT, "determined KC UUID mismatch occured, waiting 15s for userspace binary to finish repair");
        IOSleep(15000);
        SYSLOG(MODULE_SHORT, "restarting now");
        PEHaltRestart(kPERestartCPU);
    }

    return kc_result;
}

#pragma mark - Patches on start/stop

static void pluginStart() {
    DBGLOG(MODULE_SHORT, "start");
    lilu.onPatcherLoadForce([](void *user, KernelPatcher &patcher) {
        KernelPatcher::RouteRequest csRoute = KernelPatcher::RouteRequest("__ZN6OSKext29validateKCUUIDfromPrelinkInfoEPA16_h7kc_kindP12OSDictionaryPKc", patched_validateKCUUID, orig_validateKCUUID);
        if (!patcher.routeMultipleLong(KernelPatcher::KernelID, &csRoute, 1))
            SYSLOG(MODULE_SHORT, "failed to route KC validation function");
    });
}

// Boot args.
static const char *bootargOff[] {
    "-rsrhelpoff"
};
static const char *bootargDebug[] {
    "-rsrhelpdbg"
};
static const char *bootargBeta[] {
    "-rsrhelpbeta"
};

// Plugin configuration.
PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal | LiluAPI::AllowSafeMode,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::BigSur,
    KernelVersion::Sequoia,
    pluginStart
};
