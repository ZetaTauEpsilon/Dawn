#include <cstdint>
#include <cstring>
#include <iostream>

#include "state/activity/membership/transactions/internal.h"
#include "state/activity/transactions/internal.h"
#include "state/activity/vanilla/one_au/selection.h"
#include "server/bap/encrypted/activity_message/membership/activity_membership_route.h"

namespace {

using namespace dawn::state::activity;

int g_failureCount = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

membership::PendingMutation region_move(const ActivityState& state,
                                        const SessionRecord& record,
                                        std::int32_t to) {
    membership::PendingMutation mutation{};
    mutation.authoritativeInput.hasRegion = true;
    mutation.authoritativeInput.region.index = to;
    mutation.authoritativeInput.region.hash = 0xA5A50000U | static_cast<std::uint32_t>(to);
    mutation.authoritativeGuard = mutation.authoritativeInput;
    mutation.movesRegion = record.membership.region.index != to;
    mutation.instanceKey = transactions::instance_key(record);
    mutation.expectedHostRegion = transactions::host_region_key(record);
    mutation.expectedStateRevision = state.stateRevision;
    mutation.expectedRecordRevision = record.recordRevision;
    mutation.hasSnapshot = record.membership.hasIdentity && mutation.movesRegion;

    membership::PreparedRegionTransition transition{};
    transition.activity = mutation.instanceKey;
    transition.expectedHostRegion = mutation.expectedHostRegion;
    transition.nextHostRegion = mutation.expectedHostRegion;
    transition.before = record.membership;
    transition.after = membership::transactions::merge(record.membership,
                                                        mutation.authoritativeInput);
    transition.destination = record.destination;
    transition.grantBefore = record.bubbleAuthority;
    transition.expectedStateRevision = state.stateRevision;
    transition.expectedRecordRevision = record.recordRevision;
    transition.effectiveRegion = transition.after.region.index;
    transition.movesRegion = mutation.movesRegion;
    transition.publishesMembership = mutation.hasSnapshot;
    if (transition.movesRegion) {
        HostRegionGeneration next = transition.expectedHostRegion.generation;
        bool exhausted = false;
        if (advance(next, exhausted)) {
            transition.nextHostRegion = {transition.activity, next};
        }
    }
    if (transition.publishesMembership) {
        ++transition.after.revision;
        transition.after.acknowledgedRevision = membership::kAbsentRevision;
    }
    mutation.regionTransition = transition;
    mutation.regionTransitionGuard = transition;
    return mutation;
}

SessionRecord fresh_record(std::int32_t regionIndex) {
    SessionRecord record{};
    record.sessionId = 42;
    record.occupied = true;
    record.joined = true;
    record.lifecycle = transactions::fresh_lifecycle();
    record.membership.region.index = regionIndex;
    record.membership.revision = 4;
    record.membership.hasIdentity = true;
    record.recordRevision = kInitialStateRevision;
    return record;
}

void authoritative_guard_includes_region_fields() {
    membership::AuthoritativeUpdate first{};
    first.hasRegion = true;
    first.region.index = 120;
    first.region.hash = 0x11111111;
    membership::AuthoritativeUpdate second = first;

    CHECK(membership::transactions::equal(first, second));
    second.region.index = 88;
    CHECK(!membership::transactions::equal(first, second));
    second = first;
    second.region.hash ^= 1;
    CHECK(!membership::transactions::equal(first, second));
    second = first;
    second.hasRegion = false;
    CHECK(!membership::transactions::equal(first, second));
    namespace route=dawn::server::bap::encrypted::activity_message::membership;
    namespace client=dawn::middleware::bap::activity_message::client_authoritative_data;
    client::ClientAuthoritativeData parsed{};
    parsed.currentLeg={8,0x12345678,64,1,2,true};parsed.pendingLeg={7,0xFEDCBA98,56,0,1,true};
    ActivityState state{};auto record=fresh_record(64);auto mutation=region_move(state,record,56);
    const auto mapped=route::make_authoritative(parsed,"mission_ember");
    mutation.authoritativeInput.currentLeg=mapped.currentLeg;mutation.authoritativeInput.pendingLeg=mapped.pendingLeg;
    mutation.authoritativeGuard=mutation.authoritativeInput;
    mutation.regionTransition.after=membership::transactions::merge(mutation.regionTransition.after,mutation.authoritativeInput);
    mutation.regionTransitionGuard=mutation.regionTransition;
    CHECK(membership::transactions::commit_authoritative(state,record,mutation));
    CHECK(record.membership.currentLeg==mapped.currentLeg && record.membership.pendingLeg==mapped.pendingLeg);
}

void accepted_region_move_advances_before_publication() {
    ActivityState state{};
    SessionRecord record = fresh_record(120);
    const ActivityInstanceKey activityBefore = transactions::instance_key(record);
    membership::PendingMutation mutation = region_move(state, record, 88);

    CHECK(membership::transactions::commit_authoritative(state, record, mutation));
    CHECK(record.membership.region.index == 88);
    CHECK(record.membership.revision == 5);
    CHECK(transactions::instance_key(record) == activityBefore);
    CHECK(record.lifecycle.hostRegion.value == kFirstGeneration + 1);
    CHECK(!record.lifecycle.hostRegionExhausted);
    CHECK(state.stateRevision == kInitialStateRevision + 1);
    CHECK(record.recordRevision == state.stateRevision);
}

void stale_or_mutated_region_guard_is_rejected() {
    ActivityState state{};
    SessionRecord record = fresh_record(120);
    membership::PendingMutation mutation = region_move(state, record, 88);
    mutation.authoritativeGuard.region.index = 89;

    CHECK(!membership::transactions::commit_authoritative(state, record, mutation));
    CHECK(record.membership.region.index == 120);
    CHECK(record.lifecycle.hostRegion.value == kFirstGeneration);
    CHECK(state.stateRevision == kInitialStateRevision);
}

void exhausted_region_generation_rejects_the_whole_move() {
    ActivityState state{};
    SessionRecord record = fresh_record(120);
    record.lifecycle.hostRegion.value = kMaximumGeneration;
    membership::PendingMutation mutation = region_move(state, record, 88);

    CHECK(!membership::transactions::commit_authoritative(state, record, mutation));
    CHECK(!record.lifecycle.hostRegionExhausted);
    CHECK(record.lifecycle.hostRegion.value == kMaximumGeneration);
    CHECK(record.membership.region.index == 120);
    CHECK(state.stateRevision == kInitialStateRevision);
    CHECK(record.recordRevision == kInitialStateRevision);
}

void mutated_after_image_is_rejected_without_partial_commit() {
    ActivityState state{};
    SessionRecord record = fresh_record(120);
    const SessionRecord before = record;
    membership::PendingMutation mutation = region_move(state, record, 88);
    mutation.regionTransition.after.region.index = 89;

    CHECK(!membership::transactions::commit_authoritative(state, record, mutation));
    CHECK(record.membership.region.index == before.membership.region.index);
    CHECK(record.membership.revision == before.membership.revision);
    CHECK(record.lifecycle.hostRegion == before.lifecycle.hostRegion);
    CHECK(state.stateRevision == kInitialStateRevision);
}

void sibling_changes_are_allowed_only_for_one_au() {
    for (int variant = 0; variant < 5; ++variant) {
        ActivityState state{};
        SessionRecord record = fresh_record(120);
        record.destination.activityIndex = forced::prelaunch::kOneAu.activity;
        const auto package = forced::prelaunch::kOneAu.package;
        std::memcpy(record.destination.packageName.data(), package.data(), package.size());
        record.destination.packageNameLength = static_cast<std::uint8_t>(package.size());
        if (variant == 0) record.destination.activityIndex = 0;
        if (variant == 1) record.destination.packageName[0] ^= 1;
        CHECK(vanilla::one_au::selected(record.destination) == (variant >= 2));
        membership::PendingMutation mutation = region_move(state, record, 88);
        ++state.stateRevision; // A cinematic sibling host changes the global revision.
        if (variant == 3) ++record.recordRevision;
        if (variant == 4) mutation.authoritativeInput.region.hash ^= 1;
        const bool expected = variant == 2;
        CHECK(membership::transactions::commit_authoritative(state, record, mutation) == expected);
        CHECK(record.membership.region.index == (expected ? 88 : 120));
        CHECK(record.membership.revision == (expected ? 5U : 4U));
        CHECK(state.stateRevision == kInitialStateRevision + (expected ? 2U : 1U));
    }
}

} // namespace

int main() {
    authoritative_guard_includes_region_fields();
    accepted_region_move_advances_before_publication();
    stale_or_mutated_region_guard_is_rejected();
    exhausted_region_generation_rejects_the_whole_move();
    mutated_after_image_is_rejected_without_partial_commit();
    sibling_changes_are_allowed_only_for_one_au();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " activity host lifecycle check(s) failed\n";
        return 1;
    }
    std::cout << "all activity host lifecycle checks passed\n";
    return 0;
}
