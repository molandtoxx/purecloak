// Stub header for non-ChromeOS builds.
#ifndef ASH_CONSTANTS_ASH_PREF_NAMES_H_
#define ASH_CONSTANTS_ASH_PREF_NAMES_H_

namespace ash::prefs {

// Accessibility prefs
inline constexpr char kAccessibilityAutoclickEnabled[] = "ash.accessibility.autoclick.enabled";
inline constexpr char kAccessibilityChromeVoxEnabled[] = "ash.accessibility.chrome_vox.enabled";
inline constexpr char kAccessibilityHighContrastEnabled[] = "ash.accessibility.high_contrast.enabled";
inline constexpr char kAccessibilityLargeCursorEnabled[] = "ash.accessibility.large_cursor.enabled";
inline constexpr char kAccessibilityScreenMagnifierEnabled[] = "ash.accessibility.screen_magnifier.enabled";
inline constexpr char kAccessibilitySelectToSpeakEnabled[] = "ash.accessibility.select_to_speak.enabled";
inline constexpr char kAccessibilitySpokenFeedbackEnabled[] = "ash.accessibility.spoken_feedback.enabled";
inline constexpr char kAccessibilityStickyKeysEnabled[] = "ash.accessibility.sticky_keys.enabled";
inline constexpr char kAccessibilitySwitchAccessEnabled[] = "ash.accessibility.switch_access.enabled";
inline constexpr char kAccessibilityDictationEnabled[] = "ash.accessibility.dictation.enabled";
inline constexpr char kAccessibilityDictationLocale[] = "ash.accessibility.dictation.locale";
inline constexpr char kAccessibilityVirtualKeyboardEnabled[] = "ash.accessibility.virtual_keyboard.enabled";
inline constexpr char kShouldAlwaysShowAccessibilityMenu[] = "ash.should_always_show_accessibility_menu";

// Display prefs
inline constexpr char kDisplayRotationAcceleratorDialogHasBeenAccepted[] = "ash.display.rotation_accelerator_dialog_accepted";
inline constexpr char kDisplayMixedMirrorModeParams[] = "ash.display.mixed_mirror_mode_params";

// Night light prefs
inline constexpr char kNightLightEnabled[] = "ash.night_light.enabled";
inline constexpr char kNightLightTemperature[] = "ash.night_light.temperature";

// Dark mode prefs
inline constexpr char kDarkModeEnabled[] = "ash.dark_mode.enabled";

// Contextual nudges prefs
inline constexpr char kContextualNudgeEnabled[] = "ash.contextual_nudge.enabled";

// Docked magnifier prefs
inline constexpr char kDockedMagnifierEnabled[] = "ash.docked_magnifier.enabled";

// Accessibility focus highlight
inline constexpr char kAccessibilityFocusHighlightEnabled[] = "ash.accessibility.focus_highlight.enabled";

// Accessibility cursor color
inline constexpr char kAccessibilityCursorColorEnabled[] = "ash.accessibility.cursor_color.enabled";

// Accessibility caret highlight
inline constexpr char kAccessibilityCaretHighlightEnabled[] = "ash.accessibility.caret_highlight.enabled";

// Accessibility mono audio
inline constexpr char kAccessibilityMonoAudioEnabled[] = "ash.accessibility.mono_audio.enabled";

}  // namespace ash::prefs

#endif  // ASH_CONSTANTS_ASH_PREF_NAMES_H_
