// Copyright 2026 The PureCloak Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURECLOAK_BROWSER_PURECLOAK_BROWSER_MAIN_PARTS_H_
#define PURECLOAK_BROWSER_PURECLOAK_BROWSER_MAIN_PARTS_H_

class Profile;

namespace purecloak {

// Starts the PureCloak REST API server on the initial profile.
// Called from ChromeBrowserMainParts::PostProfileInit with the initial Profile.
// Only starts once; subsequent calls for additional profiles are no-ops.
void StartPureCloakApiServer(Profile* profile);

// Stops the PureCloak REST API server. Called during browser shutdown.
void StopPureCloakApiServer();

}  // namespace purecloak

#endif  // PURECLOAK_BROWSER_PURECLOAK_BROWSER_MAIN_PARTS_H_
