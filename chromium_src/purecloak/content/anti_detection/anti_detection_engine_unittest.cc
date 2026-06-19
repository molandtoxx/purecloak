// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/content/anti_detection/anti_detection_engine.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "purecloak/browser/profiles/workspace.h"

namespace purecloak {
namespace {

// Helper: create a workspace with a given seed and optional GPU info.
Workspace MakeWorkspace(int seed,
                        const std::string& gpu_vendor = "",
                        const std::string& gpu_renderer = "") {
  Workspace p;
  p.fingerprint_seed = seed;
  p.gpu_vendor = gpu_vendor;
  p.gpu_renderer = gpu_renderer;
  return p;
}

}  // namespace

// ─── GeneratePRNGFunction ───────────────────────────────────────────────────

TEST(AntiDetectionEngineTest, PRNGContainsSeed) {
  std::string js = AntiDetectionEngine::GeneratePRNGFunction(12345);
  EXPECT_NE(std::string::npos, js.find("12345"))
      << "Seed value must appear in the PRNG function";
}

TEST(AntiDetectionEngineTest, PRNGIsFunctionDefinition) {
  std::string js = AntiDetectionEngine::GeneratePRNGFunction(42);
  EXPECT_NE(std::string::npos, js.find("function _pcNoise"))
      << "Must define _pcNoise function";
}

TEST(AntiDetectionEngineTest, PRNGDeterministic) {
  std::string a = AntiDetectionEngine::GeneratePRNGFunction(9999);
  std::string b = AntiDetectionEngine::GeneratePRNGFunction(9999);
  EXPECT_EQ(a, b) << "Same seed must produce identical PRNG function";
}

TEST(AntiDetectionEngineTest, PRNGDifferentSeedsDiffer) {
  std::string a = AntiDetectionEngine::GeneratePRNGFunction(100);
  std::string b = AntiDetectionEngine::GeneratePRNGFunction(200);
  EXPECT_NE(a, b) << "Different seeds must produce different PRNG functions";
}

// ─── GenerateCanvasProtector ────────────────────────────────────────────────

TEST(AntiDetectionEngineTest, CanvasProtectorOverridesGetImageData) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateCanvasProtector(5555);
  EXPECT_NE(std::string::npos, js.find("getImageData"));
  EXPECT_NE(std::string::npos, js.find("CanvasRenderingContext2D"));
}

TEST(AntiDetectionEngineTest, CanvasProtectorOverridesToDataURL) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateCanvasProtector(5555);
  EXPECT_NE(std::string::npos, js.find("toDataURL"));
  EXPECT_NE(std::string::npos, js.find("HTMLCanvasElement"));
}

TEST(AntiDetectionEngineTest, CanvasProtectorContainsSeed) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateCanvasProtector(7777);
  EXPECT_NE(std::string::npos, js.find("7777"))
      << "Seed must be embedded for deterministic noise";
}

TEST(AntiDetectionEngineTest, CanvasProtectorDeterministic) {
  AntiDetectionEngine engine;
  std::string a = engine.GenerateCanvasProtector(333);
  std::string b = engine.GenerateCanvasProtector(333);
  EXPECT_EQ(a, b);
}

// ─── GenerateWebGLProtector ─────────────────────────────────────────────────

TEST(AntiDetectionEngineTest, WebGLProtectorWithGPUInfo) {
  AntiDetectionEngine engine;
  Workspace p = MakeWorkspace(100, "Google Inc. (NVIDIA)", "ANGLE (NVIDIA)");
  std::string js = engine.GenerateWebGLProtector(p);
  EXPECT_NE(std::string::npos, js.find("Google Inc. (NVIDIA)"));
  EXPECT_NE(std::string::npos, js.find("ANGLE (NVIDIA)"));
  EXPECT_NE(std::string::npos, js.find("getParameter"));
}

TEST(AntiDetectionEngineTest, WebGLProtectorWithoutGPUInfo) {
  AntiDetectionEngine engine;
  Workspace p = MakeWorkspace(100);  // empty GPU
  std::string js = engine.GenerateWebGLProtector(p);
  EXPECT_NE(std::string::npos, js.find("getSupportedExtensions"));
  EXPECT_NE(std::string::npos, js.find("WEBGL_debug_renderer"));
}

TEST(AntiDetectionEngineTest, WebGLProtectorEscapesSingleQuotes) {
  AntiDetectionEngine engine;
  Workspace p = MakeWorkspace(100, "Vendor's 'GPU'", "");
  std::string js = engine.GenerateWebGLProtector(p);
  // The single quote should be escaped with backslash.
  EXPECT_NE(std::string::npos, js.find("\\'"));
}

TEST(AntiDetectionEngineTest, WebGLProtectorOverridesWebGL2) {
  AntiDetectionEngine engine;
  Workspace p = MakeWorkspace(100, "TestVendor", "TestRenderer");
  std::string js = engine.GenerateWebGLProtector(p);
  EXPECT_NE(std::string::npos, js.find("WebGL2RenderingContext"));
}

// ─── GenerateAudioProtector ─────────────────────────────────────────────────

TEST(AntiDetectionEngineTest, AudioProtectorOverridesAnalyserNode) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateAudioProtector(4242);
  EXPECT_NE(std::string::npos, js.find("AnalyserNode"));
  EXPECT_NE(std::string::npos, js.find("getFloatFrequencyData"));
  EXPECT_NE(std::string::npos, js.find("getByteFrequencyData"));
}

TEST(AntiDetectionEngineTest, AudioProtectorOverridesAudioBuffer) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateAudioProtector(4242);
  EXPECT_NE(std::string::npos, js.find("AudioBuffer"));
  EXPECT_NE(std::string::npos, js.find("getChannelData"));
}

TEST(AntiDetectionEngineTest, AudioProtectorContainsSeed) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateAudioProtector(8888);
  EXPECT_NE(std::string::npos, js.find("8888"));
}

// ─── GenerateFontProtector ──────────────────────────────────────────────────

TEST(AntiDetectionEngineTest, FontProtectorOverridesMeasureText) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateFontProtector(6666);
  EXPECT_NE(std::string::npos, js.find("measureText"));
  EXPECT_NE(std::string::npos, js.find("CanvasRenderingContext2D"));
}

TEST(AntiDetectionEngineTest, FontProtectorOverridesQueryLocalFonts) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateFontProtector(6666);
  EXPECT_NE(std::string::npos, js.find("queryLocalFonts"));
}

TEST(AntiDetectionEngineTest, FontProtectorContainsSeed) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateFontProtector(6666);
  EXPECT_NE(std::string::npos, js.find("6666"));
}

// ─── GenerateWebRTCProtector ────────────────────────────────────────────────

TEST(AntiDetectionEngineTest, WebRTCProtectorWrapsRTCPeerConnection) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateWebRTCProtector();
  EXPECT_NE(std::string::npos, js.find("RTCPeerConnection"));
  EXPECT_NE(std::string::npos, js.find("addIceCandidate"));
}

TEST(AntiDetectionEngineTest, WebRTCProtectorFiltersSrflxCandidates) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateWebRTCProtector();
  EXPECT_NE(std::string::npos, js.find("srflx"))
      << "Server reflexive candidates that leak real IP must be filtered";
}

// ─── GenerateWebdriverRemover ───────────────────────────────────────────────

TEST(AntiDetectionEngineTest, WebdriverRemoverRemovesNavigatorWebdriver) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateWebdriverRemover();
  EXPECT_NE(std::string::npos, js.find("webdriver"));
  EXPECT_NE(std::string::npos, js.find("navigator"));
}

TEST(AntiDetectionEngineTest, WebdriverRemoverOverridesPlugins) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateWebdriverRemover();
  EXPECT_NE(std::string::npos, js.find("plugins"));
  EXPECT_NE(std::string::npos, js.find("PDF Viewer"))
      << "Plugins list should include PDF Viewer to look natural";
}

TEST(AntiDetectionEngineTest, WebdriverRemoverOverridesLanguages) {
  AntiDetectionEngine engine;
  std::string js = engine.GenerateWebdriverRemover();
  EXPECT_NE(std::string::npos, js.find("languages"));
}

// ─── GenerateProtectionScript ───────────────────────────────────────────────

TEST(AntiDetectionEngineTest, ProtectionScriptEmptyForZeroSeed) {
  AntiDetectionEngine engine;
  Workspace p = MakeWorkspace(0);
  std::string js = engine.GenerateProtectionScript(p);
  EXPECT_TRUE(js.empty()) << "Seed 0 means no protection";
}

TEST(AntiDetectionEngineTest, ProtectionScriptNonEmptyForNonZeroSeed) {
  AntiDetectionEngine engine;
  Workspace p = MakeWorkspace(12345);
  std::string js = engine.GenerateProtectionScript(p);
  EXPECT_FALSE(js.empty());
  EXPECT_GT(js.size(), 100u) << "Full protection script should be substantial";
}

TEST(AntiDetectionEngineTest, ProtectionScriptContainsAllProtectors) {
  AntiDetectionEngine engine;
  Workspace p = MakeWorkspace(12345, "TestVendor", "TestRenderer");
  std::string js = engine.GenerateProtectionScript(p);

  EXPECT_NE(std::string::npos, js.find("_pcNoise"))    << "PRNG";
  EXPECT_NE(std::string::npos, js.find("getImageData")) << "Canvas";
  EXPECT_NE(std::string::npos, js.find("getParameter")) << "WebGL";
  EXPECT_NE(std::string::npos, js.find("AnalyserNode")) << "Audio";
  EXPECT_NE(std::string::npos, js.find("measureText"))  << "Font";
  EXPECT_NE(std::string::npos, js.find("RTCPeerConnection")) << "WebRTC";
  EXPECT_NE(std::string::npos, js.find("webdriver"))    << "Webdriver";
}

TEST(AntiDetectionEngineTest, ProtectionScriptWrappedInIIFE) {
  AntiDetectionEngine engine;
  Workspace p = MakeWorkspace(12345);
  std::string js = engine.GenerateProtectionScript(p);
  EXPECT_NE(std::string::npos, js.find("(function(){"))
      << "Script should be wrapped in an IIFE";
  EXPECT_NE(std::string::npos, js.find("})();"))
      << "IIFE should be closed";
}

TEST(AntiDetectionEngineTest, ProtectionScriptDeterministic) {
  AntiDetectionEngine engine;
  Workspace p = MakeWorkspace(54321, "Vendor", "Renderer");
  std::string a = engine.GenerateProtectionScript(p);
  std::string b = engine.GenerateProtectionScript(p);
  EXPECT_EQ(a, b) << "Same profile must produce identical script";
}

}  // namespace purecloak
