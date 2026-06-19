// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub header for non-ChromeOS builds. The real chromeos/ directory has been
// removed from PureCloak since this browser will never run on ChromeOS.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_

#include "base/feature_list.h"

namespace chromeos::features {

// Feature flag declarations (all disabled on non-ChromeOS builds).
BASE_DECLARE_FEATURE(kCachedLocationProvider);
BASE_DECLARE_FEATURE(kCloudGamingDevice);
BASE_DECLARE_FEATURE(kCrosComponents);
BASE_DECLARE_FEATURE(kCrosIsolatedWebAppSetShape);
BASE_DECLARE_FEATURE(kFeatureManagementDisableChromeCompose);
BASE_DECLARE_FEATURE(kFeatureManagementGlic);
BASE_DECLARE_FEATURE(kFeatureManagementHistoryEmbedding);
BASE_DECLARE_FEATURE(kFeatureManagementMahi);
BASE_DECLARE_FEATURE(kFeatureManagementOrca);
BASE_DECLARE_FEATURE(kFeatureManagementPassageEmbedder);
BASE_DECLARE_FEATURE(kFeatureManagementRoundedWindows);
BASE_DECLARE_FEATURE(kFileSystemProviderCloudFileSystem);
BASE_DECLARE_FEATURE(kFileSystemProviderContentCache);
BASE_DECLARE_FEATURE(kGeminiAppPreinstall);
BASE_DECLARE_FEATURE(kMagicBoostRevamp);
BASE_DECLARE_FEATURE(kMagicBoostRevampForQuickAnswers);
BASE_DECLARE_FEATURE(kMahiPanelResizable);
BASE_DECLARE_FEATURE(kMahiSummarizeSelected);
BASE_DECLARE_FEATURE(kMicrosoft365ManifestOverride);
BASE_DECLARE_FEATURE(kMicrosoft365ManifestUrls);
BASE_DECLARE_FEATURE(kMicrosoft365ScopeExtensions);
BASE_DECLARE_FEATURE(kMicrosoft365ScopeExtensionsDomains);
BASE_DECLARE_FEATURE(kMicrosoft365ScopeExtensionsURLs);
BASE_DECLARE_FEATURE(kNewGuestProfile);
BASE_DECLARE_FEATURE(kOfficeNavigationCapturingReimpl);
BASE_DECLARE_FEATURE(kOrca);
BASE_DECLARE_FEATURE(kOrcaDogfood);
BASE_DECLARE_FEATURE(kPlatformKeysChangesWave1);
BASE_DECLARE_FEATURE(kQuickAnswersMaterialNextUI);
BASE_DECLARE_FEATURE(kQuickAnswersRichCard);
BASE_DECLARE_FEATURE(kQuickShareV2);
BASE_DECLARE_FEATURE(kSystemFeaturesDisableListHidden);
BASE_DECLARE_FEATURE(kUploadOfficeToCloud);
BASE_DECLARE_FEATURE(kUseSearchClickForRightClick);
BASE_DECLARE_FEATURE(kVidsAppConsumerPreinstall);
BASE_DECLARE_FEATURE(kVidsAppPreinstall);

// Functions.
bool ShouldDisableChromeComposeOnChromeOS();

}  // namespace chromeos::features

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
