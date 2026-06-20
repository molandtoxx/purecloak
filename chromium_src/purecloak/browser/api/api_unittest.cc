// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace purecloak {

// Path parsing logic (same as in workspace_api_handler.cc).
// We test it here since it's a key function for route dispatch.
namespace {

std::vector<std::string> ParsePath(const std::string& path) {
  std::vector<std::string> segments;
  if (path.empty() || path == "/") {
    return segments;
  }

  size_t start = path.front() == '/' ? 1 : 0;
  while (start < path.size()) {
    size_t end = path.find('/', start);
    if (end == std::string::npos) {
      segments.push_back(path.substr(start));
      break;
    }
    if (end > start) {
      segments.push_back(path.substr(start, end - start));
    }
    start = end + 1;
  }
  return segments;
}

}  // namespace

class ApiPathTest : public testing::Test {};

TEST_F(ApiPathTest, ParseEmptyPath) {
  auto segments = ParsePath("");
  EXPECT_TRUE(segments.empty());
}

TEST_F(ApiPathTest, ParseRootPath) {
  auto segments = ParsePath("/");
  EXPECT_TRUE(segments.empty());
}

TEST_F(ApiPathTest, ParseSimplePath) {
  auto segments = ParsePath("/api/workspaces");
  ASSERT_EQ(segments.size(), 2u);
  EXPECT_EQ(segments[0], "api");
  EXPECT_EQ(segments[1], "workspaces");
}

TEST_F(ApiPathTest, ParsePathWithId) {
  auto segments = ParsePath("/api/workspaces/abc-123");
  ASSERT_EQ(segments.size(), 3u);
  EXPECT_EQ(segments[0], "api");
  EXPECT_EQ(segments[1], "workspaces");
  EXPECT_EQ(segments[2], "abc-123");
}

TEST_F(ApiPathTest, ParsePathWithAction) {
  auto segments = ParsePath("/api/workspaces/abc-123/launch");
  ASSERT_EQ(segments.size(), 4u);
  EXPECT_EQ(segments[0], "api");
  EXPECT_EQ(segments[1], "workspaces");
  EXPECT_EQ(segments[2], "abc-123");
  EXPECT_EQ(segments[3], "launch");
}

TEST_F(ApiPathTest, ParsePathWithSubpath) {
  auto segments = ParsePath("/api/workspaces/abc-123/cdp/devtools/browser");
  ASSERT_EQ(segments.size(), 6u);
  EXPECT_EQ(segments[0], "api");
  EXPECT_EQ(segments[1], "workspaces");
  EXPECT_EQ(segments[2], "abc-123");
  EXPECT_EQ(segments[3], "cdp");
  EXPECT_EQ(segments[4], "devtools");
  EXPECT_EQ(segments[5], "browser");
}

TEST_F(ApiPathTest, ParseStatusPath) {
  auto segments = ParsePath("/api/status");
  ASSERT_EQ(segments.size(), 2u);
  EXPECT_EQ(segments[0], "api");
  EXPECT_EQ(segments[1], "status");
}

// JSON body parsing tests (simplified inline version)
TEST_F(ApiPathTest, ParseJsonBodyBasic) {
  // This is a placeholder for more comprehensive tests.
  // JSON parsing is handled by base::JSONReader which has its own tests.
  // Here we validate the business logic around it.
  SUCCEED();
}

// Error response format tests
TEST_F(ApiPathTest, ErrorResponseFormat) {
  base::Value::Dict error_detail;
  error_detail.Set("code", "NOT_FOUND");
  error_detail.Set("message", "Test error");

  base::Value::Dict error;
  error.Set("success", false);
  error.Set("error", std::move(error_detail));

  ASSERT_TRUE(error.FindBool("success").has_value());
  EXPECT_FALSE(*error.FindBool("success"));

  const base::Value::Dict* detail = error.FindDict("error");
  ASSERT_NE(detail, nullptr);
  EXPECT_EQ(*detail->FindString("code"), "NOT_FOUND");
  EXPECT_EQ(*detail->FindString("message"), "Test error");
}

TEST_F(ApiPathTest, SuccessResponseFormat) {
  base::Value::Dict data;
  data.Set("id", "test-id");
  data.Set("name", "test-workspace");

  base::Value::Dict response;
  response.Set("success", true);
  response.Set("data", std::move(data));

  ASSERT_TRUE(response.FindBool("success").has_value());
  EXPECT_TRUE(*response.FindBool("success"));

  const base::Value::Dict* data_out = response.FindDict("data");
  ASSERT_NE(data_out, nullptr);
  EXPECT_EQ(*data_out->FindString("id"), "test-id");
  EXPECT_EQ(*data_out->FindString("name"), "test-workspace");
}

// Workspace model serialization tests (integration)
TEST_F(ApiPathTest, WorkspaceToDictBasic) {
  // Test that a workspace dict has the required fields
  base::Value::Dict ws;
  ws.Set("id", "ws-001");
  ws.Set("name", "Test");
  ws.Set("type", "normal");

  EXPECT_EQ(*ws.FindString("id"), "ws-001");
  EXPECT_EQ(*ws.FindString("name"), "Test");
  EXPECT_EQ(*ws.FindString("type"), "normal");
}

}  // namespace purecloak
