#pragma once

#include <cstdint>
#include <string_view>

#include "../destination/definition.h"
#include "definition.h"

namespace dawn::state::activity::forced {

/**
 * Replaces the forced destination.
 * @param value Candidate selection, complete or partial.
 * @return True when every named field is inside its wire range and the value was stored.
 */
[[nodiscard]] bool publish(const ForcedDestination& value) noexcept;
/**
 * Apply opening coordinates only to this activity's own native selection, or to the selection
 * retail chains into it from `chainSource` (carrying that source or no previous activity);
 * preserve its descriptor.
 */
[[nodiscard]] bool publish_direct(const ForcedDestination& value, std::int16_t activity,
                                  std::int16_t chainSource = destination::kAbsentActivityIndex) noexcept;
/** Atomically copies the direct destination and returns its exact native activity. */
[[nodiscard]] std::int16_t direct_snapshot(ForcedDestination& value) noexcept;

/**
 * Copies the effective operator configuration. It is empty while the completed
 * Omega run has released its override for the native free-roam handoff.
 * @param value Receives the effective selection.
 */
void snapshot(ForcedDestination& value) noexcept;
/** Called by the native launch driver only after claiming a qualified completed
 * ending and validating its destination. Preserves the raw panel configuration
 * but pauses forcing across teardown/idle until that configuration changes.
 * Returns false if the run is stale or the operator has selected another override. */
[[nodiscard]] bool suspend_omega_for_completed_run(std::uint64_t run) noexcept;
[[nodiscard]] bool omega_completion_suspended() noexcept;
[[nodiscard]] bool suspend_launchpad_for_completed_run(std::uint64_t run) noexcept;

/**
 * Copies the operator's raw stored panel selection, exactly as set in the activity-override panel
 * and empty until the operator sets one. Completion suspension does not edit it.
 * @param value Receives the raw stored selection.
 */
void stored(ForcedDestination& value) noexcept;

/** Drops the selection and the switch, the same as the interface's clear action. */
void clear() noexcept;

/** An explicit native Forest selection supersedes only a matching Forest debug
 * override. Keep the panel's fields, but release its switch through return to orbit. */
[[nodiscard]] bool release_haunted_forest_for_native_selection(
    std::int16_t source,std::int16_t destination,std::string_view package) noexcept;

/** Commits a supported hidden mission from the exact Chosen donor tuple.
 * Returns the same configuration committed under the state lock. */
[[nodiscard]] bool commit_prelaunch_authored_selection(
    std::int16_t sourceActivityIndex, std::int16_t destinationActivityIndex,
    ForcedDestination& committed) noexcept;

/**
 * Commits a staged Homecoming override at the native launch boundary after Chosen has supplied
 * its authored activity selection.
 * @param sourceActivityIndex Authored launch selection source activity.
 * @param destinationActivityIndex Authored launch selection destination activity.
 * @return True when an enabled Homecoming configuration is committed for this selection.
 */
[[nodiscard]] bool commit_homecoming_authored_selection(
    std::int16_t sourceActivityIndex,
    std::int16_t destinationActivityIndex) noexcept;

/**
 * @return True while the stored selection is complete and operationally committed.
 * Homecoming and Gateway remain staged until Chosen supplies its authored activity-282 descriptor.
 */
[[nodiscard]] bool override_active() noexcept;

/**
 * @return True while the committed override names an authored private mission whose activity-host
 * re-establishment and roster reconciliation path is implemented by the embedded server.
 */
[[nodiscard]] bool mission_host_reestablishment_enabled() noexcept;

/**
 * Latches the authored mission host's final peer-reestablishment acknowledgement.
 * This is the first host-owned boundary after the authored manager has been activated.
 * @return True only for the acknowledgement that changed the latch from clear to set.
 */
[[nodiscard]] bool mark_opening_host_ready() noexcept;

/**
 * Latches Towerfall's native in-world boundary when the archived migration alias cannot exist.
 * The caller must first prove initial-slice completion plus native world, player, script, and
 * director readiness. Omega remains restricted to mark_opening_host_ready().
 * @return True only when Towerfall changed the shared opening-ready latch from clear to set.
 */
[[nodiscard]] bool mark_towerfall_native_ready() noexcept;

/**
 * @return True after the current authored mission reaches its package-specific authority boundary:
 * the final migrated host acknowledgement, or Towerfall's validated native in-world substitute.
 * A changed or disabled override clears the latch before its next manager is created.
 */
[[nodiscard]] bool opening_host_ready() noexcept;

/**
 * Overwrites one committed destination with the forced one. The descriptor bits are dropped:
 * they carry the client's chosen name, and one outbound message replays them as-is. The
 * activity index goes too, because many package names map to several definitions.
 * @param selection Destination built from the client's request, replaced in place.
 * Homecoming is committed by the first authored Chosen selection and deliberately ignores blank
 * fallback allocations before that selection arrives.
 * @return True when a complete, committed forced destination was applied.
 */
[[nodiscard]] bool apply(destination::DestinationSelection& selection) noexcept;

} // namespace dawn::state::activity::forced
