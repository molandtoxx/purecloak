// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_COMMON_PURECLOAK_I18N_H_
#define PURECLOAK_COMMON_PURECLOAK_I18N_H_

#include <string>
#include <string_view>

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"

namespace purecloak {

// PureCloak-specific string IDs (declared in purecloak_resources.grd).
// Generated GRIT header provides the actual ID constants.
// For now we use inline locale detection as a minimal i18n layer.

// Returns the application's locale string (e.g., "zh-CN", "en-US").
inline std::string GetLocale() {
  return base::i18n::GetConfiguredLocale();
}

// Returns true if the current locale is Chinese (any variant).
inline bool IsChineseLocale() {
  std::string locale = GetLocale();
  return locale.substr(0, 2) == "zh";
}

// Returns a Chinese or English string based on locale.
inline std::u16string i18n(const std::u16string& chinese,
                           const std::u16string& english) {
  return IsChineseLocale() ? chinese : english;
}

// PureCloak UI string constants (Chinese / English pairs).
namespace strings {

// Side panel header
inline std::u16string kWorkspaces() {
  return i18n(u"工作区", u"Workspaces");
}

// New workspace button
inline std::u16string kNewWorkspace() {
  return i18n(u"+ 新建", u"+ New");
}

// Empty state
inline std::u16string kNoWorkspaces() {
  return i18n(u"暂无工作区。点击\"+ 新建\"创建一个。",
              u"No workspaces yet. Click \"+ New\" to create one.");
}

// Workspace type: Normal
inline std::u16string kNormal() {
  return i18n(u"📦 普通", u"📦 Normal");
}

// Workspace type: Fingerprint
inline std::u16string kFingerprint() {
  return i18n(u"🔒 指纹", u"🔒 Fingerprint");
}

// Dialog: Create Workspace
inline std::u16string kCreateWorkspace() {
  return i18n(u"创建工作区", u"Create Workspace");
}

// Dialog: Rename Workspace
inline std::u16string kRenameWorkspace() {
  return i18n(u"重命名工作区", u"Rename Workspace");
}

// Dialog: Delete Workspace
inline std::u16string kDeleteWorkspace() {
  return i18n(u"删除工作区", u"Delete Workspace");
}

// Workspace type combobox accessible name
inline std::u16string kWorkspaceType() {
  return i18n(u"工作区类型", u"Workspace type");
}

// Button: Create
inline std::u16string kCreate() {
  return i18n(u"创建", u"Create");
}

// Button: Rename
inline std::u16string kRename() {
  return i18n(u"重命名", u"Rename");
}

// Button: Delete
inline std::u16string kDelete() {
  return i18n(u"删除", u"Delete");
}

// Button: Cancel
inline std::u16string kCancel() {
  return i18n(u"取消", u"Cancel");
}

// Label: Name
inline std::u16string kName() {
  return i18n(u"名称", u"Name");
}

// Label: Type
inline std::u16string kType() {
  return i18n(u"类型", u"Type");
}

// Label: New name
inline std::u16string kNewName() {
  return i18n(u"新名称", u"New name");
}

// Label: Workspace name (placeholder)
inline std::u16string kWorkspaceName() {
  return i18n(u"工作区名称", u"Workspace name");
}

// Toolbar tooltip
inline std::u16string kToolbarTooltip() {
  return i18n(u"PureCloak 工作区模式", u"PureCloak workspace mode");
}

// Delete workspace confirmation
inline std::u16string kDeleteWorkspaceConfirm(const std::string& name) {
  if (IsChineseLocale()) {
    return base::UTF8ToUTF16("删除工作区 \"" + name + "\"？");
  }
  return base::UTF8ToUTF16("Delete \"" + name + "\"?");
}

}  // namespace strings
}  // namespace purecloak

#endif  // PURECLOAK_COMMON_PURECLOAK_I18N_H_
