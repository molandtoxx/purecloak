// Copyright 2026 PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "purecloak/content/storage_partition_controller.h"

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "purecloak/common/buildflags.h"

using content::BrowserContext;
using content::BrowserThread;
using content::StoragePartitionConfig;

namespace purecloak {
namespace {

// Per-WebContents user data that stores the profile_id for a tab.
class ProfileWebContentsData : public base::SupportsUserData::Data {
 public:
  explicit ProfileWebContentsData(std::string profile_id)
      : profile_id_(std::move(profile_id)) {}
  const std::string& profile_id() const { return profile_id_; }

 private:
  std::string profile_id_;
};

// BrowserContext-level user data for the consume-once active profile_id.
class ActiveProfileData : public base::SupportsUserData::Data {
 public:
  explicit ActiveProfileData(std::string profile_id)
      : profile_id_(std::move(profile_id)) {}
  std::string Consume() { return std::exchange(profile_id_, std::string()); }
  bool has_value() const { return !profile_id_.empty(); }

 private:
  std::string profile_id_;
};

const void* kProfileWebContentsDataKey = &kProfileWebContentsDataKey;
const void* kActiveProfileDataKey = &kActiveProfileDataKey;

// Build the partition name. If |profile_id| is non-empty, appends
// "#<profile_id>" to |workspace| for per-profile isolation.
std::string BuildPartitionName(const std::string& workspace,
                               const std::string& profile_id) {
  if (profile_id.empty()) {
    return workspace;
  }
  return workspace + StoragePartitionController::kProfilePartitionSeparator +
         profile_id;
}

}  // namespace

const char StoragePartitionController::kPureCloakDomain[] = "purecloak";
const char StoragePartitionController::kProfilePartitionSeparator[] = "#";
const void* StoragePartitionController::kUserDataKey = &kUserDataKey;
const void* StoragePartitionController::kProfileUserDataKey =
    &kProfileUserDataKey;

StoragePartitionController::StoragePartitionController(
    BrowserContext* browser_context,
    const std::string& workspace,
    Type type)
    : workspace_(workspace), type_(type) {
  DCHECK(browser_context);
}

// static
void StoragePartitionController::SetWorkspace(
    BrowserContext* browser_context,
    const std::string& workspace,
    Type type) {
  DCHECK(browser_context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (browser_context->GetUserData(kUserDataKey)) {
    browser_context->RemoveUserData(kUserDataKey);
  }

  if (!workspace.empty()) {
    browser_context->SetUserData(
        kUserDataKey,
        std::make_unique<StoragePartitionController>(browser_context, workspace,
                                                     type));
  }
}

// static
std::string StoragePartitionController::GetWorkspace(
    BrowserContext* browser_context) {
  DCHECK(browser_context);
  auto* controller = static_cast<StoragePartitionController*>(
      browser_context->GetUserData(kUserDataKey));
  if (!controller) {
    return std::string();
  }
  return controller->workspace_;
}

// static
StoragePartitionController::Type
StoragePartitionController::GetWorkspaceType(
    BrowserContext* browser_context) {
  DCHECK(browser_context);
  auto* controller = static_cast<StoragePartitionController*>(
      browser_context->GetUserData(kUserDataKey));
  if (!controller) {
    return Type::kNormal;
  }
  return controller->type_;
}

// static
void StoragePartitionController::SetProfile(
    content::WebContents* web_contents,
    const std::string& profile_id) {
  DCHECK(web_contents);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (profile_id.empty()) {
    web_contents->RemoveUserData(kProfileUserDataKey);
  } else {
    web_contents->SetUserData(
        kProfileUserDataKey,
        std::make_unique<ProfileWebContentsData>(profile_id));
  }
}

// static
std::string StoragePartitionController::GetProfile(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  auto* data = static_cast<ProfileWebContentsData*>(
      web_contents->GetUserData(kProfileUserDataKey));
  if (!data) {
    return std::string();
  }
  return data->profile_id();
}

// static
void StoragePartitionController::SetActiveProfile(
    BrowserContext* browser_context,
    const std::string& profile_id) {
  DCHECK(browser_context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  browser_context->SetUserData(
      kActiveProfileDataKey,
      std::make_unique<ActiveProfileData>(profile_id));
}

// static
std::string StoragePartitionController::ConsumeActiveProfile(
    BrowserContext* browser_context) {
  DCHECK(browser_context);
  auto* data = static_cast<ActiveProfileData*>(
      browser_context->GetUserData(kActiveProfileDataKey));
  if (!data) {
    return std::string();
  }
  std::string result = data->Consume();
  // If consumed, remove the now-empty data.
  if (!data->has_value()) {
    browser_context->RemoveUserData(kActiveProfileDataKey);
  }
  return result;
}

// static
content::StoragePartitionConfig
StoragePartitionController::GetStoragePartitionConfigForSite(
    BrowserContext* browser_context,
    const GURL& site) {
#if BUILDFLAG(IS_PURECLOAK)
  std::string workspace = GetWorkspace(browser_context);
  if (workspace.empty()) {
    return StoragePartitionConfig::CreateDefault(browser_context);
  }

  Type type = GetWorkspaceType(browser_context);
  if (type == Type::kNormal) {
    return StoragePartitionConfig::CreateDefault(browser_context);
  }

  // Fingerprint workspace: consume active profile_id if set.
  std::string profile_id = ConsumeActiveProfile(browser_context);
  std::string partition_name = BuildPartitionName(workspace, profile_id);

  return StoragePartitionConfig::Create(
      browser_context, kPureCloakDomain, partition_name,
      /*in_memory=*/false);
#else
  return StoragePartitionConfig::CreateDefault(browser_context);
#endif
}

// static
content::StoragePartitionConfig
StoragePartitionController::GetStoragePartitionConfig(
    BrowserContext* browser_context,
    const GURL& site,
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_PURECLOAK)
  std::string workspace = GetWorkspace(browser_context);
  if (workspace.empty()) {
    return StoragePartitionConfig::CreateDefault(browser_context);
  }

  Type type = GetWorkspaceType(browser_context);
  if (type == Type::kNormal) {
    return StoragePartitionConfig::CreateDefault(browser_context);
  }

  // Fingerprint workspace: check WebContents-level profile first.
  std::string profile_id;
  if (web_contents) {
    profile_id = GetProfile(web_contents);
  }
  // Fall back to consume-once active profile from BrowserContext.
  if (profile_id.empty()) {
    profile_id = ConsumeActiveProfile(browser_context);
  }

  std::string partition_name = BuildPartitionName(workspace, profile_id);
  return StoragePartitionConfig::Create(
      browser_context, kPureCloakDomain, partition_name,
      /*in_memory=*/false);
#else
  return StoragePartitionConfig::CreateDefault(browser_context);
#endif
}

}  // namespace purecloak
