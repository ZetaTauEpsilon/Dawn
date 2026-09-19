#pragma once

#include <string_view>
#include "../../../../../middleware/bap/activity_message/client_authoritative_data.h"

#include "../../../../../middleware/bap/activity_message/definition.h"
#include "../definition.h"

namespace dawn::server::bap::encrypted::activity_message::membership {

[[nodiscard]] constexpr bool retains_held_region(std::string_view destination) noexcept {
    return destination == "mission_bond" || destination == "mission_pact" || destination == "strike_bond" || destination == "strike_pact"
        || destination == "mercury_freeroam" || destination == "eden_freeroam"
        || destination == "fleet_freeroam" || destination == "polaris_freeroam"
        || destination == "planet_x_freeroam" || destination == "tangled_shore_freeroam"
        || destination == "dreaming_city_freeroam"
        || destination == "adventure_rumba" || destination == "mission_launchpad";
}

/** Maps parsed membership fields without changing legacy destination routing. */
[[nodiscard]] inline state::activity::membership::AuthoritativeUpdate make_authoritative(
    const middleware::bap::activity_message::client_authoritative_data::ClientAuthoritativeData& parsed,
    std::string_view destination) noexcept {
    state::activity::membership::AuthoritativeUpdate update{};
    update.transitionToken = parsed.transitionToken;
    update.hasTransitionToken = parsed.hasTransitionToken;
    update.synchronizationToken = parsed.synchronizationToken;
    update.hasSynchronizationToken = parsed.hasSynchronizationToken;
    update.spawn.state = parsed.spawn.state;
    update.spawn.opaqueByte = parsed.spawn.opaqueByte;
    update.spawn.opaqueValue = parsed.spawn.opaqueValue;
    update.hasSpawn = parsed.hasSpawn;
    update.teleport.state = parsed.teleport.state;
    update.teleport.token = parsed.teleport.token;
    update.teleport.sliceSetIndex = parsed.teleport.sliceSetIndex;
    update.teleport.sliceSetHash = parsed.teleport.sliceSetHash;
    update.hasTeleport = parsed.hasTeleport;
    // Keep the second leg for prefetch. These destinations also retain the first
    // (held) leg so a post-swap outgoing region cannot undo the actual arrival.
    update.region.index = parsed.region.index;
    update.region.hash = parsed.region.hash;
    update.hasRegion = parsed.hasRegion;
    if(retains_held_region(destination)) {
        update.currentRegion={parsed.currentRegion.index,parsed.currentRegion.hash};
        update.hasCurrentRegion=parsed.hasCurrentRegion;
    }
    if(destination=="mission_launchpad" || destination=="mission_ember") {
        const auto leg=[](const auto& v) -> state::activity::membership::RegionLeg {
            return {v.sliceSetIndex,v.sliceSetHash,v.regionIndex,v.publicState,v.auxState,v.present};
        };
        update.currentLeg=leg(parsed.currentLeg);update.pendingLeg=leg(parsed.pendingLeg);
    }
    return update;
}

/**
 * Stages a changed identity push or an unchanged transactional no-op.
 * @param request Validated owned svc8 envelope.
 * @param plan Cleared, then receives the membership transaction and optional delivery.
 * @return True when the exact identity is valid for the join-bound session.
 */
[[nodiscard]] bool prepare_identity(state::activity::ActivityInstanceKey key,
                                    const middleware::bap::activity_message::Request& request,
                                    ActivityPlan& plan) noexcept;

/**
 * Stages the kept host-state changes, and an updated snapshot only when needed.
 * @param request Validated owned svc8 envelope.
 * @param plan Cleared, then receives the authoritative membership transaction.
 * @return True when the exact sparse body is valid for the joined session.
 */
[[nodiscard]] bool prepare_authoritative(state::activity::ActivityInstanceKey key,
                                         const middleware::bap::activity_message::Request& request,
                                         ActivityPlan& plan) noexcept;

/**
 * Stages a stable membership resend guarded by the current State revisions.
 * @param request Validated owned svc8 envelope.
 * @param plan Cleared, then receives the refresh transaction and optional delivery.
 * @return True when the exact refresh request and joined session are valid.
 */
[[nodiscard]] bool prepare_refresh(state::activity::ActivityInstanceKey key,
                                   const middleware::bap::activity_message::Request& request,
                                   ActivityPlan& plan) noexcept;

/**
 * Stages a matching acknowledgement update or a transactional no-op.
 * @param request Validated owned svc8 envelope.
 * @param plan Cleared, then receives the acknowledgement transaction.
 * @return True when the exact acknowledgement and joined session are valid.
 */
[[nodiscard]] bool
prepare_acknowledgement(state::activity::ActivityInstanceKey key,
                        const middleware::bap::activity_message::Request& request,
                        ActivityPlan& plan) noexcept;

} // namespace dawn::server::bap::encrypted::activity_message::membership
