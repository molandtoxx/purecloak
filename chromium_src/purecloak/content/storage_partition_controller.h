// Copyright 2026 PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_CONTENT_STORAGE_PARTITION_CONTROLLER_H_
#define PURECLOAK_CONTENT_STORAGE_PARTITION_CONTROLLER_H_

#include <string>

#include "base/supports_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"

namespace purecloak {

// Associates workspace and profile identity with a BrowserContext and provides
// StoragePartitionConfig overrides for workspace-isolated / profile-isolated
// storage.
//
// Workspace types:
//   kNormal:      All tabs use the default (shared) storage partition.
//   kFingerprint: Tabs use per-profile isolated partitions. The partition
//                 domain is "purecloak" and the partition_name encodes both
//                 workspace and profile ("<workspace_id>#<profile_id>").
//                 Same-profile tabs share storage; different-profile tabs are
//                 fully isolated.
//
// Usage:
//   purecloak::StoragePartitionController::SetWorkspace(
//       bc, "ws_abc", StoragePartitionController::Type::kFingerprint);
//   purecloak::StoragePartitionController::SetProfile(wc, "profile_x");
//   auto config = purecloak::StoragePartitionController::
//       GetStoragePartitionConfig(bc, site, wc);
class CONTENT_EXPORT StoragePartitionController
    : public base::SupportsUserData::Data {
 public:
  enum class Type { kNormal, kFingerprint };

  static const char kPureCloakDomain[];
  static const char kProfilePartitionSeparator[];  // "#"

  ~StoragePartitionController() override = default;

  // Sets the active workspace for |browser_context|. Use kFingerprint for
  // profile-isolated workspaces, kNormal for plain browsing.
  static void SetWorkspace(content::BrowserContext* browser_context,
                           const std::string& workspace,
                           Type type = Type::kFingerprint);

  static std::string GetWorkspace(content::BrowserContext* browser_context);

  // Returns the workspace type for |browser_context|. Returns kNormal if
  // no workspace has been set.
  static Type GetWorkspaceType(content::BrowserContext* browser_context);

  // Associates a profile_id with a WebContents (tab).
  static void SetProfile(content::WebContents* web_contents,
                         const std::string& profile_id);

  static std::string GetProfile(content::WebContents* web_contents);

  // Sets the active profile_id at the BrowserContext level. This value is
  // consumed once by GetStoragePartitionConfigForSite() during SiteInstance
  // creation (which doesn't have access to WebContents). Call before creating
  // a tab for a specific PureCloak profile.
  static void SetActiveProfile(content::BrowserContext* browser_context,
                               const std::string& profile_id);

  // Returns the active profile_id and clears it (consume-once pattern).
  static std::string ConsumeActiveProfile(
      content::BrowserContext* browser_context);

  // Resolves the StoragePartitionConfig for a given site.
  // - kNormal workspaces: always returns the default config.
  // - kFingerprint workspaces: returns a per-profile partition when
  //   profile_id is set, otherwise a per-workspace partition.
  // This variant is called from ContentBrowserClient::GetStoragePartitionConfigForSite,
  // which does NOT have a WebContents.
  static content::StoragePartitionConfig GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site);

  // Resolves the StoragePartitionConfig with an optional WebContents for
  // per-tab profile lookups. Falls back to the BrowserContext-level
  // active_profile_id if no WebContents-level profile is found.
  static content::StoragePartitionConfig GetStoragePartitionConfig(
      content::BrowserContext* browser_context,
      const GURL& site,
      content::WebContents* web_contents = nullptr);

  StoragePartitionController(content::BrowserContext* browser_context,
                             const std::string& workspace,
                             Type type = Type::kFingerprint);
  StoragePartitionController(const StoragePartitionController&) = delete;
  StoragePartitionController& operator=(const StoragePartitionController&) =
      delete;

 private:
  static const void* kUserDataKey;
  static const void* kProfileUserDataKey;

  std::string workspace_;
  Type type_ = Type::kFingerprint;
};

}  // namespace purecloak

#endif  // PURECLOAK_CONTENT_STORAGE_PARTITION_CONTROLLER_H_
