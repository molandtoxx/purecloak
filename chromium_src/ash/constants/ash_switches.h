// Stub header for non-ChromeOS builds.
#ifndef ASH_CONSTANTS_ASH_SWITCHES_H_
#define ASH_CONSTANTS_ASH_SWITCHES_H_

namespace ash::switches {

// Command line switches for ChromeOS/Ash.
inline constexpr char kAllowFailedPolicyFetchForTest[] = "allow-failed-policy-fetch-for-test";
inline constexpr char kAlmanacApiUrl[] = "almanac-api-url";
inline constexpr char kAlwaysEnableHdcp[] = "always-enable-hdcp";
inline constexpr char kArcAvailability[] = "arc-availability";
inline constexpr char kArcDisableAppSync[] = "arc-disable-app-sync";
inline constexpr char kAshDebugShortcuts[] = "ash-debug-shortcuts";
inline constexpr char kAshEnableTabletMode[] = "ash-enable-tablet-mode";
inline constexpr char kAshEnableWaylandServer[] = "ash-enable-wayland-server";
inline constexpr char kAshTouchHud[] = "ash-touch-hud";
inline constexpr char kCampbellKey[] = "campbell-key";
inline constexpr char kDisableGaiaServices[] = "disable-gaia-services";
inline constexpr char kDisableHIDDetectionOnOOBEForTesting[] = "disable-hid-detection-on-oobe";
inline constexpr char kEnableBackgroundBlur[] = "enable-background-blur";
inline constexpr char kHostPairingOobe[] = "host-pairing-oobe";
inline constexpr char kKioskNextHomeUrl[] = "kiosk-next-home-url";
inline constexpr char kLoginManager[] = "login-manager";
inline constexpr char kLoginUser[] = "login-user";
inline constexpr char kNaturalScrollDefault[] = "natural-scroll-default";
inline constexpr char kOobeSkipPostLogin[] = "oobe-skip-post-login";
inline constexpr char kPowerSoundsForExtension[] = "power-sounds-for-extension";
inline constexpr char kPowerUiForExtension[] = "power-ui-for-extension";
inline constexpr char kRevenBranding[] = "reven-branding";
inline constexpr char kShelfHoverPreviews[] = "shelf-hover-previews";
inline constexpr char kShowNumericKeyboardForPassword[] = "show-numeric-keyboard-for-password";
inline constexpr char kShowTaps[] = "show-taps";
inline constexpr char kSkipHIDDetectionOnOOBEForTesting[] = "skip-hid-detection-on-oobe";
inline constexpr char kUiMode[] = "ui-mode";
inline constexpr char kWaitForInitialPolicyFetchForTest[] = "wait-for-initial-policy-fetch-for-test";

}  // namespace ash::switches

#endif  // ASH_CONSTANTS_ASH_SWITCHES_H_
