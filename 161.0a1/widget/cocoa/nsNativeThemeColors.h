/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeColors_h_
#define nsNativeThemeColors_h_

#import <Cocoa/Cocoa.h>

#include "mozilla/ColorScheme.h"

// Firefox-Legacy: NSAppearanceNameDarkAqua is macOS 10.14+. On older SDKs,
// provide a weak-imported extern so the file links; at runtime on 10.13 the
// dark-mode lookup returns nil and the light Aqua appearance is used.
#include <AvailabilityMacros.h>
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101400
extern NSAppearanceName const NSAppearanceNameDarkAqua
    __attribute__((weak_import));
@interface NSColor (HighSierraShims)
@property(class, readonly) NSColor* controlAccentColor;
@property(class, readonly) NSColor* selectedContentBackgroundColor;
@property(class, readonly) NSColor* unemphasizedSelectedContentBackgroundColor;
@property(class, readonly) NSColor* linkColor;
@end
@interface NSApplication (HighSierraShims)
@property(readonly) NSAppearance* effectiveAppearance;
@end
#endif

inline NSAppearance* NSAppearanceForColorScheme(mozilla::ColorScheme aScheme) {
  // Firefox-Legacy: on 10.13 NSAppearanceNameDarkAqua is weak-imported and
  // resolves to nil; -[NSAppearance appearanceNamed:nil] aborts. Fall back to
  // Aqua. Any dark-scheme request on 10.13 renders as light.
  if (@available(macOS 10.14, *)) {
    NSAppearanceName appearanceName = aScheme == mozilla::ColorScheme::Light
                                          ? NSAppearanceNameAqua
                                          : NSAppearanceNameDarkAqua;
    return [NSAppearance appearanceNamed:appearanceName];
  }
  return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
}

#endif  // nsNativeThemeColors_h_
