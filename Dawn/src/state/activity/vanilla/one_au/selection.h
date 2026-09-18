#pragma once

#include <string_view>

#include "../../destination/definition.h"
#include "../../forced/prelaunch_profile.h"

namespace dawn::state::activity::vanilla::one_au {

/** Restricts 1AU's per-record transition guards to its exact native activity. */
[[nodiscard]] inline bool selected(const destination::DestinationSelection& value) noexcept {
    return value.activityIndex == forced::prelaunch::kOneAu.activity
        && value.packageNameLength <= value.packageName.size()
        && std::string_view(reinterpret_cast<const char*>(value.packageName.data()),
                            value.packageNameLength) == forced::prelaunch::kOneAu.package;
}

} // namespace dawn::state::activity::vanilla::one_au
