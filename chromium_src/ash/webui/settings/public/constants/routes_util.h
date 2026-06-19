// Stub header for non-ChromeOS builds.
#ifndef ASH_WEBUI_SETTINGS_PUBLIC_CONSTANTS_ROUTES_UTIL_H_
#define ASH_WEBUI_SETTINGS_PUBLIC_CONSTANTS_ROUTES_UTIL_H_

#include <string>

namespace ash::settings {

bool IsOSSettingsPath(const std::string& url_path);

}  // namespace ash::settings

#endif  // ASH_WEBUI_SETTINGS_PUBLIC_CONSTANTS_ROUTES_UTIL_H_
