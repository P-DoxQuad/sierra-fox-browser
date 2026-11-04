// Firefox-Legacy: definitions of Apple media constants missing on the 10.13 SDK.
// These are only referenced inside `@available(macOS 11+, *)` runtime guards,
// which never evaluate true on a 10.13 host — the symbols must exist for the
// linker but their values are never read at runtime on this port.

#include "HighSierraShims.h"

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 110000

#include <CoreFoundation/CoreFoundation.h>

const CFStringRef kCGColorSpaceITUR_2100_PQ = CFSTR("kCGColorSpaceITUR_2100_PQ");
const CFStringRef kCGColorSpaceITUR_2100_HLG = CFSTR("kCGColorSpaceITUR_2100_HLG");

const CFStringRef kVTVideoEncoderSpecification_EnableLowLatencyRateControl =
    CFSTR("EnableLowLatencyRateControl");
const CFStringRef kVTCompressionPropertyKey_ConstantBitRate =
    CFSTR("ConstantBitRate");
const CFStringRef kVTCompressionPropertyKey_PrioritizeEncodingSpeedOverQuality =
    CFSTR("PrioritizeEncodingSpeedOverQuality");
const CFStringRef kVTCompressionPropertyKey_BaseLayerFrameRateFraction =
    CFSTR("BaseLayerFrameRateFraction");

const CFStringRef kCVImageBufferTransferFunction_Linear = CFSTR("Linear");

#endif  // __MAC_OS_X_VERSION_MAX_ALLOWED < 110000
