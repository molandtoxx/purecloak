// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_PROFILES_STORE_PROVIDER_H_
#define PURECLOAK_BROWSER_PROFILES_STORE_PROVIDER_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"

class Profile;

namespace purecloak {

class WorkspaceStore;

// Singleton provider that creates and caches WorkspaceStore per browser
// profile path. Both the side panel coordinator and the WebUI handler use
// this to share the same store instances.
//
// Stores are created on first access and live for the process lifetime.
class StoreProvider {
 public:
  static StoreProvider& GetInstance();

  WorkspaceStore* GetWorkspaceStore(::Profile* profile);

 private:
  StoreProvider();
  ~StoreProvider();

  StoreProvider(const StoreProvider&) = delete;
  StoreProvider& operator=(const StoreProvider&) = delete;

  SEQUENCE_CHECKER(sequence_checker_);

  std::map<base::FilePath, std::unique_ptr<WorkspaceStore>> workspace_stores_;
};

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_PROFILES_STORE_PROVIDER_H_
