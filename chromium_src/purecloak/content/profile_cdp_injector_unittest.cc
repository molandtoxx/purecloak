// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/content/profile_cdp_injector.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "purecloak/browser/profiles/workspace.h"

namespace purecloak {
namespace {

// Helper: create a basic workspace for testing.
Workspace MakeTestWorkspace() {
  Workspace p;
  p.id = "test-workspace-id";
  p.name = "Test Workspace";
  p.fingerprint_seed = 12345;
  p.screen_width = 1920;
  p.screen_height = 1080;
  p.timezone = "America/New_York";
  p.locale = "en-US";
  p.platform = "windows";
  p.color_scheme = "dark";
  p.gpu_vendor = "Google Inc. (NVIDIA)";
  p.gpu_renderer = "ANGLE (NVIDIA, RTX 3080)";
  p.hardware_concurrency = 8;
  p.user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)";
  return p;
}

}  // namespace

// ─── MakeCommand ────────────────────────────────────────────────────────────

TEST(ProfileCDPInjectorTest, MakeCommandStructure) {
  base::DictValue params;
  params.Set("foo", "bar");
  base::DictValue cmd =
      ProfileCDPInjector::MakeCommand("Test.method", std::move(params));

  EXPECT_EQ("Test.method", *cmd.FindString("method"));
  ASSERT_TRUE(cmd.FindDict("params"));
  EXPECT_EQ("bar", *cmd.FindDict("params")->FindString("foo"));
}

// ─── GenerateScreenOverrides ─────────────────────────────────────────────────

TEST(ProfileCDPInjectorTest, ScreenOverridesContainWidthHeight) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  std::string js = injector.GenerateScreenOverrides(p);

  EXPECT_NE(std::string::npos, js.find("1920"));
  EXPECT_NE(std::string::npos, js.find("1080"));
  EXPECT_NE(std::string::npos, js.find("window.screen"));
  EXPECT_NE(std::string::npos, js.find("Proxy"));
}

TEST(ProfileCDPInjectorTest, ScreenOverridesDefaultsForZero) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.screen_width = 0;
  p.screen_height = 0;
  std::string js = injector.GenerateScreenOverrides(p);

  EXPECT_NE(std::string::npos, js.find("1366")) << "Default width should be 1366";
  EXPECT_NE(std::string::npos, js.find("768")) << "Default height should be 768";
}

TEST(ProfileCDPInjectorTest, ScreenOverridesCustomValues) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.screen_width = 2560;
  p.screen_height = 1440;
  std::string js = injector.GenerateScreenOverrides(p);

  EXPECT_NE(std::string::npos, js.find("2560"));
  EXPECT_NE(std::string::npos, js.find("1440"));
}

// ─── MakeScreenOverride ─────────────────────────────────────────────────────

TEST(ProfileCDPInjectorTest, MakeScreenOverride) {
  base::DictValue cmd = ProfileCDPInjector::MakeScreenOverride(1920, 1080);

  EXPECT_EQ("Emulation.setDeviceMetricsOverride",
            *cmd.FindString("method"));
  base::DictValue* params = cmd.FindDict("params");
  ASSERT_TRUE(params);
  EXPECT_EQ(1920, *params->FindInt("width"));
  EXPECT_EQ(1080, *params->FindInt("height"));
  EXPECT_EQ(1, *params->FindInt("deviceScaleFactor"));
  EXPECT_FALSE(*params->FindBool("mobile"));
}

TEST(ProfileCDPInjectorTest, MakeScreenOverrideCustom) {
  base::DictValue cmd = ProfileCDPInjector::MakeScreenOverride(2560, 1440);
  base::DictValue* params = cmd.FindDict("params");
  ASSERT_TRUE(params);
  EXPECT_EQ(2560, *params->FindInt("width"));
  EXPECT_EQ(1440, *params->FindInt("height"));
}

// ─── MakeTimezoneOverride ───────────────────────────────────────────────────

TEST(ProfileCDPInjectorTest, MakeTimezoneOverride) {
  base::DictValue cmd =
      ProfileCDPInjector::MakeTimezoneOverride("Europe/London");

  EXPECT_EQ("Emulation.setTimezoneOverride", *cmd.FindString("method"));
  base::DictValue* params = cmd.FindDict("params");
  ASSERT_TRUE(params);
  EXPECT_EQ("Europe/London", *params->FindString("timezoneId"));
}

// ─── MakeDarkModeOverride ───────────────────────────────────────────────────

TEST(ProfileCDPInjectorTest, MakeDarkModeOverrideEnabled) {
  base::DictValue cmd = ProfileCDPInjector::MakeDarkModeOverride(true);

  EXPECT_EQ("Emulation.setAutoDarkModeOverride",
            *cmd.FindString("method"));
  base::DictValue* params = cmd.FindDict("params");
  ASSERT_TRUE(params);
  EXPECT_TRUE(*params->FindBool("enabled"));
}

TEST(ProfileCDPInjectorTest, MakeDarkModeOverrideDisabled) {
  base::DictValue cmd = ProfileCDPInjector::MakeDarkModeOverride(false);

  base::DictValue* params = cmd.FindDict("params");
  ASSERT_TRUE(params);
  EXPECT_FALSE(*params->FindBool("enabled"));
}

// ─── MakeScriptInjection ────────────────────────────────────────────────────

TEST(ProfileCDPInjectorTest, MakeScriptInjection) {
  base::DictValue cmd =
      ProfileCDPInjector::MakeScriptInjection("console.log('test');");

  EXPECT_EQ("Page.addScriptToEvaluateOnNewDocument",
            *cmd.FindString("method"));
  base::DictValue* params = cmd.FindDict("params");
  ASSERT_TRUE(params);
  EXPECT_EQ("console.log('test');", *params->FindString("source"));
}

// ─── GenerateCDPCommands ────────────────────────────────────────────────────

TEST(ProfileCDPInjectorTest, GenerateCDPCommandsContainsScreenOverrides) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  auto commands = injector.GenerateCDPCommands(p);

  bool found_script = false;
  for (const auto& cmd : commands) {
    if (cmd.FindString("method") &&
        *cmd.FindString("method") ==
            "Page.addScriptToEvaluateOnNewDocument") {
      // Verify the script source contains screen overrides.
      const base::DictValue* params = cmd.FindDict("params");
      ASSERT_TRUE(params);
      const std::string* source = params->FindString("source");
      ASSERT_TRUE(source);
      if (source->find("window.screen") != std::string::npos &&
          source->find("1920") != std::string::npos) {
        found_script = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_script)
      << "Screen overrides should be in the injection script, not as a "
         "separate CDP command";
}

TEST(ProfileCDPInjectorTest, GenerateCDPCommandsContainsTimezone) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  auto commands = injector.GenerateCDPCommands(p);

  bool found_tz = false;
  for (const auto& cmd : commands) {
    if (cmd.FindString("method") &&
        *cmd.FindString("method") == "Emulation.setTimezoneOverride") {
      found_tz = true;
      break;
    }
  }
  EXPECT_TRUE(found_tz);
}

TEST(ProfileCDPInjectorTest, GenerateCDPCommandsContainsDarkMode) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.color_scheme = "dark";
  auto commands = injector.GenerateCDPCommands(p);

  bool found_dark = false;
  for (const auto& cmd : commands) {
    if (cmd.FindString("method") &&
        *cmd.FindString("method") == "Emulation.setAutoDarkModeOverride") {
      found_dark = true;
      break;
    }
  }
  EXPECT_TRUE(found_dark);
}

TEST(ProfileCDPInjectorTest, GenerateCDPCommandsLightModeDisablesDarkMode) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.color_scheme = "light";
  auto commands = injector.GenerateCDPCommands(p);

  bool found_dark = false;
  for (const auto& cmd : commands) {
    if (cmd.FindString("method") &&
        *cmd.FindString("method") == "Emulation.setAutoDarkModeOverride") {
      found_dark = true;
      const base::DictValue* params = cmd.FindDict("params");
      ASSERT_TRUE(params);
      EXPECT_FALSE(*params->FindBool("enabled"));
      break;
    }
  }
  EXPECT_TRUE(found_dark);
}

TEST(ProfileCDPInjectorTest, GenerateCDPCommandsContainsScriptInjection) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  auto commands = injector.GenerateCDPCommands(p);

  bool found_script = false;
  for (const auto& cmd : commands) {
    if (cmd.FindString("method") &&
        *cmd.FindString("method") ==
            "Page.addScriptToEvaluateOnNewDocument") {
      found_script = true;
      break;
    }
  }
  EXPECT_TRUE(found_script)
      << "Anti-detection script should be injected for non-zero seed";
}

TEST(ProfileCDPInjectorTest, GenerateCDPCommandsNoScriptForZeroSeed) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.fingerprint_seed = 0;
  auto commands = injector.GenerateCDPCommands(p);

  // With seed 0, there's no anti-detection script, but navigator overrides
  // and screen overrides are still injected as a script.
  bool found_script = false;
  for (const auto& cmd : commands) {
    if (cmd.FindString("method") &&
        *cmd.FindString("method") == "Page.addScriptToEvaluateOnNewDocument") {
      found_script = true;
      break;
    }
  }
  EXPECT_TRUE(found_script) << "Navigator/overrides script should still be present";
}

// ─── GenerateNavigatorOverrides ─────────────────────────────────────────────

TEST(ProfileCDPInjectorTest, NavigatorOverridesWindowsPlatform) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.platform = "windows";
  std::string js = injector.GenerateNavigatorOverrides(p);

  EXPECT_NE(std::string::npos, js.find("Win32"));
}

TEST(ProfileCDPInjectorTest, NavigatorOverridesMacOSPlatform) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.platform = "macos";
  std::string js = injector.GenerateNavigatorOverrides(p);

  EXPECT_NE(std::string::npos, js.find("MacIntel"));
}

TEST(ProfileCDPInjectorTest, NavigatorOverridesLinuxPlatform) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.platform = "linux";
  std::string js = injector.GenerateNavigatorOverrides(p);

  EXPECT_NE(std::string::npos, js.find("Linux x86_64"));
}

TEST(ProfileCDPInjectorTest, NavigatorOverridesDefaultPlatform) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.platform = "";  // empty → defaults to Win32
  std::string js = injector.GenerateNavigatorOverrides(p);

  EXPECT_NE(std::string::npos, js.find("Win32"));
}

TEST(ProfileCDPInjectorTest, NavigatorOverridesHardwareConcurrency) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.hardware_concurrency = 16;
  std::string js = injector.GenerateNavigatorOverrides(p);

  EXPECT_NE(std::string::npos, js.find("16"));
}

TEST(ProfileCDPInjectorTest, NavigatorOverridesDefaultConcurrency) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.hardware_concurrency = 0;  // → defaults to 4
  std::string js = injector.GenerateNavigatorOverrides(p);

  EXPECT_NE(std::string::npos, js.find("return 4;"))
      << "Concurrency 0 should default to 4";
}

TEST(ProfileCDPInjectorTest, NavigatorOverridesUserAgent) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.user_agent = "MyCustomUA/1.0";
  std::string js = injector.GenerateNavigatorOverrides(p);

  EXPECT_NE(std::string::npos, js.find("MyCustomUA/1.0"));
}

TEST(ProfileCDPInjectorTest, NavigatorOverridesEscapesUserAgentQuotes) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.user_agent = "Mozilla'5.0";
  std::string js = injector.GenerateNavigatorOverrides(p);

  EXPECT_NE(std::string::npos, js.find("\\'"))
      << "Single quotes in UA must be escaped";
}

// ─── GenerateInjectionScript ────────────────────────────────────────────────

TEST(ProfileCDPInjectorTest, InjectionScriptWithoutAntiDetection) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  std::string js = injector.GenerateInjectionScript(p, false);

  // Should contain navigator overrides and screen overrides but NOT anti-detection.
  EXPECT_NE(std::string::npos, js.find("navigator"));
  EXPECT_NE(std::string::npos, js.find("window.screen"));
  EXPECT_NE(std::string::npos, js.find("1920"));
  EXPECT_EQ(std::string::npos, js.find("_pcNoise"))
      << "Anti-detection PRNG should not appear without flag";
}

TEST(ProfileCDPInjectorTest, InjectionScriptWithAntiDetection) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  std::string js = injector.GenerateInjectionScript(p, true);

  // Should contain both navigator overrides and anti-detection.
  EXPECT_NE(std::string::npos, js.find("navigator"));
  EXPECT_NE(std::string::npos, js.find("_pcNoise"))
      << "Anti-detection PRNG should appear with flag and non-zero seed";
}

TEST(ProfileCDPInjectorTest, InjectionScriptAntiDetectionNoSeed) {
  ProfileCDPInjector injector;
  Workspace p = MakeTestWorkspace();
  p.fingerprint_seed = 0;
  std::string js = injector.GenerateInjectionScript(p, true);

  // With seed 0, GenerateProtectionScript returns empty, so no anti-detection.
  EXPECT_NE(std::string::npos, js.find("navigator"))
      << "Navigator overrides should still be present";
  EXPECT_EQ(std::string::npos, js.find("_pcNoise"))
      << "No anti-detection for seed 0";
}

}  // namespace purecloak
