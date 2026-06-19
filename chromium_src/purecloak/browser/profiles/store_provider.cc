// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/browser/profiles/store_provider.h"

#include "chrome/browser/profiles/profile.h"
#include "purecloak/browser/profiles/workspace_store.h"

namespace purecloak {

// static
StoreProvider& StoreProvider::GetInstance() {
  static StoreProvider* instance = new StoreProvider();
  return *instance;
}

StoreProvider::StoreProvider() = default;
StoreProvider::~StoreProvider() = default;

WorkspaceStore* StoreProvider::GetWorkspaceStore(::Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile);

  base::FilePath dir = profile->GetPath().AppendASCII("PureCloak");
  auto it = workspace_stores_.find(dir);
  if (it != workspace_stores_.end()) {
    return it->second.get();
  }

  auto store = std::make_unique<WorkspaceStore>(dir);
  store->Init();
  auto* ptr = store.get();
  workspace_stores_[dir] = std::move(store);
  return ptr;
}

}  // namespace purecloak
