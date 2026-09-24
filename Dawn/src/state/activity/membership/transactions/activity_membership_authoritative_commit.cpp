#include "internal.h"

#include <cstring>

#include "../../transactions/internal.h"
#include "../../vanilla/one_au/selection.h"
#include "../../vanilla/homecoming/selection.h"

namespace dawn::state::activity::membership::transactions {

/** Merges and applies one sparse authoritative operation. */
bool commit_authoritative(ActivityState& state,
                          SessionRecord& record,
                          const PendingMutation& prepared) noexcept {
    const PreparedRegionTransition& transition = prepared.regionTransition;
    if (!equal(prepared.authoritativeInput, prepared.authoritativeGuard)
        || !transactions::equal(transition, prepared.regionTransitionGuard)
        || transition.activity != activity::transactions::instance_key(record)
        || transition.expectedHostRegion != activity::transactions::host_region_key(record)
        || (!vanilla::one_au::selected(record.destination) && !vanilla::homecoming::selected(record.destination)
            && transition.expectedStateRevision != state.stateRevision)
        || transition.expectedRecordRevision != record.recordRevision
        || !equal(transition.before, record.membership)) {
        return false;
    }

    MembershipState expectedAfter = merge(transition.before, prepared.authoritativeInput);
    const bool changed = !equal_authoritative(transition.before, expectedAfter);
    const bool movesRegion = moves_region(transition.before, expectedAfter);
    const bool publishes = changed || movesRegion;
    if (publishes && expectedAfter.hasIdentity) {
        if (expectedAfter.revision == kMaximumMembershipRevision) {
            return false;
        }
        ++expectedAfter.revision;
        expectedAfter.acknowledgedRevision = kAbsentRevision;
    }
    if (transition.movesRegion != movesRegion
        || transition.publishesMembership != (publishes && expectedAfter.hasIdentity)
        || transition.effectiveRegion != expectedAfter.region.index
        || !equal(transition.after, expectedAfter)
        || prepared.hasSnapshot != transition.publishesMembership) {
        return false;
    }
    if (!publishes) {
        return transition.nextHostRegion == transition.expectedHostRegion;
    }
    if (state.stateRevision == activity::kMaximumRevision) {
        return false;
    }
    if (movesRegion) {
        HostRegionGeneration expectedNext = transition.expectedHostRegion.generation;
        bool exhausted = false;
        if (!activity::advance(expectedNext, exhausted)
            || transition.nextHostRegion
                   != HostRegionKey{transition.activity, expectedNext}) {
            return false;
        }
    } else if (transition.nextHostRegion != transition.expectedHostRegion) {
        return false;
    }

    if (movesRegion) {
        record.lifecycle.hostRegion = transition.nextHostRegion.generation;
    }
    record.membership = transition.after;
    publish_change(state, record);
    return true;
}

} // namespace dawn::state::activity::membership::transactions
