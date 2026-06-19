// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_PROFILES_WORKSPACE_STORE_H_
#define PURECLOAK_BROWSER_PROFILES_WORKSPACE_STORE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "purecloak/browser/profiles/workspace.h"

namespace purecloak {

class WorkspaceStoreObserver : public base::CheckedObserver {
 public:
  virtual void OnWorkspaceAdded(const Workspace& workspace) {}
  virtual void OnWorkspaceUpdated(const Workspace& workspace) {}
  virtual void OnWorkspaceRemoved(const std::string& workspace_id) {}
  virtual void OnWorkspacesChanged() {}

 protected:
  WorkspaceStoreObserver() = default;
  ~WorkspaceStoreObserver() override = default;
};

// Persistent storage for PureCloak workspaces.
//
// Stores workspace definitions (id, name, type) in a single JSON file:
//   <root_path>/workspaces.json
//
// Type locking: once a workspace is created with a type, SetType() is a no-op.
class WorkspaceStore {
 public:
  explicit WorkspaceStore(const base::FilePath& root_path);
  WorkspaceStore(const WorkspaceStore&) = delete;
  WorkspaceStore& operator=(const WorkspaceStore&) = delete;
  ~WorkspaceStore();

  // Initialize the store. Loads data from disk.
  void Init();

  // --- CRUD ---

  // Create a workspace. Returns the workspace with generated id.
  // Type cannot be changed after creation (type locking).
  Workspace CreateWorkspace(Workspace workspace);

  std::optional<Workspace> GetWorkspace(const std::string& id) const;
  std::vector<Workspace> GetAllWorkspaces() const;

  // Update name only. Type is immutable.
  bool UpdateWorkspace(const std::string& id, const std::string& new_name);

  // Full-field update. Updates all user-configurable fields.
  // Id and type are preserved (type is immutable after creation).
  bool UpdateWorkspace(const Workspace& workspace);

  // Delete a workspace and all its profiles (caller should also delete profiles).
  bool DeleteWorkspace(const std::string& id);

  // --- Type locking (returns false if type differs from stored) ---
  bool VerifyType(const std::string& id, Workspace::Type type) const;

  // --- Observers ---
  void AddObserver(WorkspaceStoreObserver* observer);
  void RemoveObserver(WorkspaceStoreObserver* observer);

  // --- Persistence ---
  void Flush();

 private:
  base::FilePath GetFilePath() const;
  void OnInitData(std::optional<base::DictValue> dict);
  void SaveToDisk();

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath root_path_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  std::map<std::string, Workspace> workspaces_;
  base::ObserverList<WorkspaceStoreObserver> observers_;
  bool initialized_ = false;

  base::WeakPtrFactory<WorkspaceStore> weak_factory_{this};
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_PROFILES_WORKSPACE_STORE_H_
