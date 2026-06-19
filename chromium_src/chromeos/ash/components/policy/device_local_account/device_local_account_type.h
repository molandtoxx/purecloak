// Stub header for non-ChromeOS builds.
#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_DEVICE_LOCAL_ACCOUNT_DEVICE_LOCAL_ACCOUNT_TYPE_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_DEVICE_LOCAL_ACCOUNT_DEVICE_LOCAL_ACCOUNT_TYPE_H_

#include <string>

namespace policy {

enum class DeviceLocalAccountType {
  kWebKioskApp,
  kArcKioskApp,
  kSamlPublicSession,
};

// Returns the display name for the given device local account type.
std::string GetDeviceLocalAccountTypeDisplayName(
    DeviceLocalAccountType type);

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_DEVICE_LOCAL_ACCOUNT_DEVICE_LOCAL_ACCOUNT_TYPE_H_
