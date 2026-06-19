// Stub header for non-ChromeOS builds.
#ifndef ASH_PUBLIC_CPP_SHELF_TYPES_H_
#define ASH_PUBLIC_CPP_SHELF_TYPES_H_

#include <string>
#include "ui/gfx/image/image_skia.h"

namespace ash {

// The direction the shelf is in. Identifies where the shelf resides.
enum class ShelfAlignment {
  kBottom,
  kLeft,
  kRight,
};

// The auto-hide behavior of the shelf.
enum class ShelfAutoHideBehavior {
  kAlways,
  kNever,
  kAlwaysHidden,
};

// The type of shelf item.
enum class ShelfItemType {
  kPinnedApp,
  kApp,
  kDialog,
  kBrowserShortcut,
  kUndefined,
};

// The status of a shelf item.
enum class ShelfItemStatus {
  kClosed,
  kRunning,
  kAttention,
};

// The current visibility state of the shelf.
enum class ShelfVisibilityState {
  kHidden,
  kVisible,
  kAutoHide,
};

// Shelf item data.
struct ShelfID {
  std::string app_id;
  std::string launch_id;

  ShelfID() = default;
  explicit ShelfID(const std::string& app_id) : app_id(app_id) {}
  ShelfID(const std::string& app_id, const std::string& launch_id)
      : app_id(app_id), launch_id(launch_id) {}

  bool operator==(const ShelfID& other) const {
    return app_id == other.app_id && launch_id == other.launch_id;
  }
  bool operator!=(const ShelfID& other) const { return !(*this == other); }
  bool operator<(const ShelfID& other) const {
    return app_id < other.app_id ||
           (app_id == other.app_id && launch_id < other.launch_id);
  }
};

struct ShelfItem {
  ShelfItemType type = ShelfItemType::kUndefined;
  ShelfItemStatus status = ShelfItemStatus::kClosed;
  ShelfID id;
  std::string title;
  gfx::ImageSkia image;
  bool pinned_by_policy = false;
  bool has_notification = false;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_TYPES_H_
