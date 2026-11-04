// Firefox-Legacy: shims for Apple media/codec constants added after macOS 10.13.
// Each symbol here is runtime-gated at its call sites; this header only exists
// to let the code compile against the 10.13 SDK.

#ifndef FIREFOX_LEGACY_APPLE_HIGH_SIERRA_SHIMS_H_
#define FIREFOX_LEGACY_APPLE_HIGH_SIERRA_SHIMS_H_

#include <AvailabilityMacros.h>

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 110000

#include <CoreMedia/CMFormatDescription.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <CoreGraphics/CGColorSpace.h>
#include <VideoToolbox/VideoToolbox.h>

// CMVideoCodecType constants (CMFormatDescription.h, macOS 11+)
enum {
  kCMVideoCodecType_VP9 = 'vp09',
  kCMVideoCodecType_AV1 = 'av01',
};

// AudioFormatID constants (CoreAudioBaseTypes.h, macOS 11+)
enum {
  kAudioFormatMPEGD_USAC = 'usac',
};

// CVPixelFormat constants defined in gfx/2d/MacIOSurface.h to avoid duplicate.

// CGColorSpace names (CGColorSpace.h, macOS 11+)
extern const CFStringRef kCGColorSpaceITUR_2100_PQ;
extern const CFStringRef kCGColorSpaceITUR_2100_HLG;

// VideoToolbox encoder keys (VTCompressionProperties.h, macOS 11.3+ / 12+)
extern const CFStringRef kVTVideoEncoderSpecification_EnableLowLatencyRateControl;
extern const CFStringRef kVTCompressionPropertyKey_ConstantBitRate;
extern const CFStringRef kVTCompressionPropertyKey_PrioritizeEncodingSpeedOverQuality;
extern const CFStringRef kVTCompressionPropertyKey_BaseLayerFrameRateFraction;

// CVImageBuffer attachment keys (CVImageBuffer.h, macOS 12+)
extern const CFStringRef kCVImageBufferTransferFunction_Linear;

#endif  // __MAC_OS_X_VERSION_MAX_ALLOWED < 110000

#endif  // FIREFOX_LEGACY_APPLE_HIGH_SIERRA_SHIMS_H_
