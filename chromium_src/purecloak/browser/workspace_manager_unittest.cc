// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/workspace_launcher.h"

#include <string>
#include <vector>

#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "purecloak/browser/profiles/workspace.h"

namespace purecloak {

namespace {

class TestWorkspaceObserver : public RunningWorkspaceObserver {
 public:
  size_t status_changed_count = 0;
  size_t launched_count = 0;
  size_t stopped_count = 0;
  std::string last_workspace_id;
  RunningWorkspace::Status last_status = RunningWorkspace::Status::kStopped;

  void OnWorkspaceStatusChanged(const std::string& workspace_id,
                                RunningWorkspace::Status status,
                                const base::DictValue& details) override {
    ++status_changed_count;
    last_workspace_id = workspace_id;
    last_status = status;
  }

  void OnWorkspaceLaunched(const RunningWorkspace& ws) override {
    ++launched_count;
    last_workspace_id = ws.workspace_id;
  }

  void OnWorkspaceStopped(const std::string& workspace_id) override {
    ++stopped_count;
    last_workspace_id = workspace_id;
  }
};

}  // namespace

class WorkspaceManagerTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

// ─── GetOrCreate ────────────────────────────────────────────────────────────

TEST_F(WorkspaceManagerTest, GetOrCreateReturnsNonNull) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  EXPECT_NE(nullptr, mgr);
}

TEST_F(WorkspaceManagerTest, GetOrCreateReturnsSameInstance) {
  RunningWorkspaceManager* mgr1 =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  RunningWorkspaceManager* mgr2 =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  EXPECT_EQ(mgr1, mgr2)
      << "GetOrCreate must return the same manager for the same context";
}

TEST_F(WorkspaceManagerTest, GetOrCreateNullContextReturnsNull) {
  RunningWorkspaceManager* mgr = RunningWorkspaceManager::GetOrCreate(nullptr);
  EXPECT_EQ(nullptr, mgr);
}

TEST_F(WorkspaceManagerTest, SeparateContextsGetSeparateManagers) {
  content::TestBrowserContext other_context;
  RunningWorkspaceManager* mgr1 =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  RunningWorkspaceManager* mgr2 =
      RunningWorkspaceManager::GetOrCreate(&other_context);
  EXPECT_NE(mgr1, mgr2)
      << "Different BrowserContexts must have separate managers";
}

// ─── Empty manager queries ──────────────────────────────────────────────────

TEST_F(WorkspaceManagerTest, RunningCountZeroOnEmpty) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  EXPECT_EQ(0u, mgr->RunningCount());
}

TEST_F(WorkspaceManagerTest, IsRunningFalseForNonExistent) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  EXPECT_FALSE(mgr->IsRunning("nonexistent-id"));
}

TEST_F(WorkspaceManagerTest, GetReturnsNullForNonExistent) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  EXPECT_EQ(nullptr, mgr->Get("nonexistent-id"));
}

TEST_F(WorkspaceManagerTest, GetAllReturnsEmptyOnEmpty) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  std::vector<RunningWorkspace*> all = mgr->GetAll();
  EXPECT_TRUE(all.empty());
}

// ─── Observers ──────────────────────────────────────────────────────────────

TEST_F(WorkspaceManagerTest, AddObserverDoesNotCrash) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  TestWorkspaceObserver observer;
  mgr->AddObserver(&observer);
  mgr->RemoveObserver(&observer);
}

TEST_F(WorkspaceManagerTest, NotifyStatusChangedFiresObserver) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  TestWorkspaceObserver observer;
  mgr->AddObserver(&observer);

  mgr->NotifyStatusChanged("test-ws", RunningWorkspace::Status::kRunning);

  EXPECT_GE(observer.status_changed_count, 1u);
  EXPECT_EQ("test-ws", observer.last_workspace_id);
  EXPECT_EQ(RunningWorkspace::Status::kRunning, observer.last_status);

  mgr->RemoveObserver(&observer);
}

TEST_F(WorkspaceManagerTest, RemovedObserverNotNotified) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  TestWorkspaceObserver observer;
  mgr->AddObserver(&observer);

  mgr->NotifyStatusChanged("ws1", RunningWorkspace::Status::kRunning);
  EXPECT_EQ(1u, observer.status_changed_count);

  mgr->RemoveObserver(&observer);
  mgr->NotifyStatusChanged("ws2", RunningWorkspace::Status::kCrashed);
  EXPECT_EQ(1u, observer.status_changed_count)
      << "Removed observer should not receive further notifications";
}

// ─── GetStatusDict ──────────────────────────────────────────────────────────

TEST_F(WorkspaceManagerTest, GetStatusDictForNonExistentWorkspace) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  base::DictValue dict = mgr->GetStatusDict("nonexistent-id");
  EXPECT_EQ("nonexistent-id", *dict.FindString("workspace_id"));
  EXPECT_EQ("stopped", *dict.FindString("status"));
}

// ─── StopAll on empty ───────────────────────────────────────────────────────

TEST_F(WorkspaceManagerTest, StopAllOnEmptyDoesNotCrash) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  mgr->StopAll();
  EXPECT_EQ(0u, mgr->RunningCount());
}

TEST_F(WorkspaceManagerTest, StopNonExistentDoesNotCrash) {
  RunningWorkspaceManager* mgr =
      RunningWorkspaceManager::GetOrCreate(&browser_context_);
  mgr->Stop("nonexistent-id");
  EXPECT_EQ(0u, mgr->RunningCount());
}

}  // namespace purecloak
