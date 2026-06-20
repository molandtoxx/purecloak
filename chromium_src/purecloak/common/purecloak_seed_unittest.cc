// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/common/purecloak_seed.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace purecloak {

class PureCloakSeedTest : public testing::Test {
 protected:
  void SetUp() override {
    // Reset state by creating a fresh instance from a minimal command line.
    PureCloakSeed::GetInstance()->InitializeFromCommandLine(
        *base::CommandLine::ForCurrentProcess());
  }
};

TEST_F(PureCloakSeedTest, DefaultValues) {
  auto* seed = PureCloakSeed::GetInstance();
  EXPECT_GE(seed->hardware_concurrency(), 1);
  EXPECT_GE(seed->device_memory(), 1);
  EXPECT_GE(seed->screen_width(), 800);
  EXPECT_GE(seed->screen_height(), 600);
  EXPECT_FALSE(seed->platform().empty());
}

TEST_F(PureCloakSeedTest, CommandLineOverride) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "42");
  cmd.AppendSwitchASCII("fingerprint-platform", "macos");
  cmd.AppendSwitchASCII("fingerprint-hardware-concurrency", "16");
  cmd.AppendSwitchASCII("fingerprint-device-memory", "4");
  cmd.AppendSwitchASCII("fingerprint-screen-width", "1440");
  cmd.AppendSwitchASCII("fingerprint-screen-height", "900");
  cmd.AppendSwitchASCII("fingerprint-taskbar-height", "95");
  cmd.AppendSwitchASCII("fingerprint-storage-quota", "1000");
  cmd.AppendSwitchASCII("fingerprint-noise", "false");

  PureCloakSeed::GetInstance()->InitializeFromCommandLine(cmd);

  EXPECT_EQ(PureCloakSeed::GetInstance()->seed(), 42);
  EXPECT_EQ(PureCloakSeed::GetInstance()->platform(), "macos");
  EXPECT_EQ(PureCloakSeed::GetInstance()->hardware_concurrency(), 16);
  EXPECT_EQ(PureCloakSeed::GetInstance()->device_memory(), 4);
  EXPECT_EQ(PureCloakSeed::GetInstance()->screen_width(), 1440);
  EXPECT_EQ(PureCloakSeed::GetInstance()->screen_height(), 900);
  EXPECT_EQ(PureCloakSeed::GetInstance()->taskbar_height(), 95);
  EXPECT_EQ(PureCloakSeed::GetInstance()->storage_quota_mb(), 1000);
  EXPECT_FALSE(PureCloakSeed::GetInstance()->noise_enabled());
}

TEST_F(PureCloakSeedTest, ThreadSafeAccess) {
  auto* seed = PureCloakSeed::GetInstance();
  // Verify that repeated access from a single thread doesn't crash.
  for (int i = 0; i < 100; ++i) {
    EXPECT_GE(seed->screen_width(), 800);
    EXPECT_GE(seed->screen_height(), 600);
    EXPECT_FALSE(seed->platform().empty());
  }
}

TEST_F(PureCloakSeedTest, PlatformDefaults) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);

  cmd.AppendSwitchASCII("fingerprint-platform", "windows");
  PureCloakSeed::GetInstance()->InitializeFromCommandLine(cmd);
  EXPECT_EQ(PureCloakSeed::GetInstance()->taskbar_height(), 48);

  cmd.AppendSwitchASCII("fingerprint-platform", "macos");
  PureCloakSeed::GetInstance()->InitializeFromCommandLine(cmd);
  EXPECT_EQ(PureCloakSeed::GetInstance()->screen_width(), 1440);
  EXPECT_EQ(PureCloakSeed::GetInstance()->screen_height(), 900);
  EXPECT_EQ(PureCloakSeed::GetInstance()->taskbar_height(), 95);
}

}  // namespace purecloak
