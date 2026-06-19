// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/profiles/workspace_store.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"

namespace purecloak {

namespace {

constexpr char kWorkspacesFileName[] = "workspaces.json";
constexpr char kVersionKey[] = "version";
constexpr char kWorkspacesKey[] = "workspaces";
constexpr int kFileVersion = 1;

// Reads and parses the workspaces JSON file. Must run on a background thread.
std::optional<base::DictValue> ReadWorkspacesFile(
    const base::FilePath& path) {
  if (!base::PathExists(path)) {
    return std::nullopt;
  }
  JSONFileValueDeserializer deserializer(path);
  std::string error_message;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(nullptr, &error_message);
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  return std::move(*value).TakeDict();
}

// Writes the workspaces JSON file. Must run on a background thread.
bool WriteWorkspacesFile(const base::FilePath& path,
                         const base::DictValue& dict) {
  base::FilePath dir = path.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    return false;
  }
  std::string json;
  if (!base::JSONWriter::WriteWithOptions(
           dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json)) {
    return false;
  }
  return base::WriteFile(path, json);
}

}  // namespace

WorkspaceStore::WorkspaceStore(const base::FilePath& root_path)
    : root_path_(root_path),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

WorkspaceStore::~WorkspaceStore() = default;

void WorkspaceStore::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadWorkspacesFile, GetFilePath()),
      base::BindOnce(&WorkspaceStore::OnInitData,
                     weak_factory_.GetWeakPtr()));
}

void WorkspaceStore::OnInitData(std::optional<base::DictValue> dict) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  initialized_ = true;

  if (!dict.has_value()) {
    return;
  }

  const base::ListValue* list = dict->FindList(kWorkspacesKey);
  if (!list) {
    return;
  }

  // Merge disk data without trampling in-memory changes.
  // If CRUD ops happened before Init() completed, the in-memory map has
  // newer data. Disk reads that posted before those CRUD writes return
  // stale data — we must NOT blindly replace the in-memory map.
  if (workspaces_.empty()) {
    // Fast path: no pre-Init activity, just load from disk.
    for (const auto& item : *list) {
      if (item.is_dict()) {
        Workspace ws = Workspace::FromDict(item.GetDict());
        workspaces_[ws.id] = std::move(ws);
      }
    }
  } else {
    // Merge: add disk entries that don't exist in memory (new from another
    // session), but keep in-memory entries (which are newer).
    for (const auto& item : *list) {
      if (item.is_dict()) {
        Workspace ws = Workspace::FromDict(item.GetDict());
        workspaces_.try_emplace(ws.id, std::move(ws));
      }
    }
  }
}

Workspace WorkspaceStore::CreateWorkspace(Workspace workspace) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (workspace.id.empty()) {
    workspace.id = Workspace::GenerateId();
  }
  if (workspace.fingerprint_seed == 0) {
    workspace.fingerprint_seed = Workspace::GenerateSeed();
  }
  base::Time now = base::Time::Now();
  workspace.created_at = now;
  workspace.updated_at = now;

  workspaces_[workspace.id] = workspace;
  SaveToDisk();

  for (auto& observer : observers_) {
    observer.OnWorkspaceAdded(workspace);
  }
  for (auto& observer : observers_) {
    observer.OnWorkspacesChanged();
  }
  return workspace;
}

std::optional<Workspace> WorkspaceStore::GetWorkspace(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = workspaces_.find(id);
  if (it == workspaces_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<Workspace> WorkspaceStore::GetAllWorkspaces() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<Workspace> result;
  result.reserve(workspaces_.size());
  for (const auto& [id, ws] : workspaces_) {
    result.push_back(ws);
  }
  return result;
}

bool WorkspaceStore::UpdateWorkspace(const std::string& id,
                                     const std::string& new_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = workspaces_.find(id);
  if (it == workspaces_.end()) {
    return false;
  }
  it->second.name = new_name;
  it->second.updated_at = base::Time::Now();
  SaveToDisk();

  for (auto& observer : observers_) {
    observer.OnWorkspaceUpdated(it->second);
  }
  for (auto& observer : observers_) {
    observer.OnWorkspacesChanged();
  }
  return true;
}

bool WorkspaceStore::DeleteWorkspace(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = workspaces_.find(id);
  if (it == workspaces_.end()) {
    return false;
  }
  workspaces_.erase(it);
  SaveToDisk();

  for (auto& observer : observers_) {
    observer.OnWorkspaceRemoved(id);
  }
  for (auto& observer : observers_) {
    observer.OnWorkspacesChanged();
  }
  return true;
}

bool WorkspaceStore::UpdateWorkspace(const Workspace& workspace) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = workspaces_.find(workspace.id);
  if (it == workspaces_.end()) {
    return false;
  }

  // Preserve immutable fields (id, type) and creation timestamp.
  Workspace::Type preserved_type = it->second.type;
  base::Time preserved_created = it->second.created_at;

  it->second = workspace;
  it->second.type = preserved_type;
  it->second.created_at = preserved_created;
  it->second.updated_at = base::Time::Now();

  SaveToDisk();

  for (auto& observer : observers_) {
    observer.OnWorkspaceUpdated(it->second);
  }
  for (auto& observer : observers_) {
    observer.OnWorkspacesChanged();
  }
  return true;
}

bool WorkspaceStore::VerifyType(const std::string& id,
                                Workspace::Type type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = workspaces_.find(id);
  if (it == workspaces_.end()) {
    return false;
  }
  return it->second.type == type;
}

void WorkspaceStore::AddObserver(WorkspaceStoreObserver* observer) {
  observers_.AddObserver(observer);
}

void WorkspaceStore::RemoveObserver(WorkspaceStoreObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WorkspaceStore::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SaveToDisk();
}

base::FilePath WorkspaceStore::GetFilePath() const {
  return root_path_.AppendASCII(kWorkspacesFileName);
}

void WorkspaceStore::SaveToDisk() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ListValue workspaces_list;
  for (const auto& [id, ws] : workspaces_) {
    workspaces_list.Append(ws.ToDict());
  }

  base::DictValue dict;
  dict.Set(kVersionKey, kFileVersion);
  dict.Set(kWorkspacesKey, std::move(workspaces_list));

  base::FilePath path = GetFilePath();
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path, base::DictValue data) {
            WriteWorkspacesFile(path, data);
          },
          path, std::move(dict)));
}

}  // namespace purecloak
