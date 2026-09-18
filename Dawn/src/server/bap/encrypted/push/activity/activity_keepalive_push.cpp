#include "../../../../../state/activity/Newlight/launchpad/transit.h"
#include "../../../../../state/activity/vanilla/one_au/transit.h"
#include "../../../../../state/activity/vanilla/one_au/runtime.h"
#include "activity_keepalive_push.h"
#include "../../../../../state/activity/gateway/runtime.h"
#include "../../../../../state/activity/deadly_trial/runtime.h"
#include "../../../../../state/activity/beyond_infinity/runtime.h"
#include "../../../../../state/activity/deep_storage/runtime.h"
#include "../../../../../state/activity/hijacked/runtime.h"
#include "../../../../../state/activity/Newlight/launchpad/runtime.h"
#include "../../../../../state/activity/beyond_infinity/transit.h"
#include "../../../../../state/activity/strike_pact/runtime.h"
#include "../../../../../state/activity/strike_bond/runtime.h"
#include "../../../../../state/activity/coo/omega_opening_projection.h"
#include "../../../../../state/activity/runtime.h"
#include "../../../../../state/activity/native_population_events.h"
#include "../../../../../state/activity/vendors/presentation.h"
#include "../../../../../state/activity/vendors/lifetime.h"
#include "../../../../../client/player/player_position.h"
#include "../../../../runtime/activity/adventure_native_bridge.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

#include "../../../../../core/logging/log.h"
#include "../../../../../core/settings/settings.h"
#include "../../../../../state/activity/definition.h"
#include "../../../../../state/activity/omega_presentation.h"
#include "../../../../../state/activity/omega_first_lair_runtime.h"
#include "../../../../../state/activity/omega_ending.h"
#include "../../../../../state/activity/forced/activity_forced_destination.h"
#include "../../../../../state/activity/membership/activity_membership_query.h"
#include "../../../../../state/build_data/runtime.h"
#include "../../../../../state/runtime/runtime.h"
#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"
#include "../../../../gameplay/gameplay_advertisement.h"
#include "../../activity_message/definition.h"
#include "../../activity_transaction/activity_transaction_notifications.h"
#include "activity_arrival.h"
#include "activity_global_state_push.h"
#include "activity_clock_push.h"
#include "activity_membership_push.h"
#include "activity_region_snapshot.h"
#include "activity_roster_push.h"
#include "internal.h"
#include "../../../../runtime/activity/native_activity_transit.h"
#include "../../../../runtime/activity/native_activity_runtime.h"

namespace dawn::server::bap::encrypted::push::activity {
namespace {

namespace message = middleware::bap::activity_message::sensor_auth_update;

/**
 * Keepalive cadence. The client tears the session down after 20.5 seconds of server silence, so
 * 5 seconds leaves 3 writes of margin.
 */
constexpr std::uint64_t kKeepaliveIntervalMs = 5'000;
/**
 * Roster burst cadence for loading and pending native observations. Idle
 * sessions keep the ordinary keepalive cadence.
 */
constexpr std::uint64_t kRosterBurstIntervalMs = 100;
/** @return True while this implemented authored-mission override owns a host-ready launch. */
[[nodiscard]] bool opening_mission_host_ready() noexcept {
    return state::activity::forced::opening_host_ready();
}

/** Region index and name hash of the Omega Infinite-Forest bubble (bubble 11). The name hash is
 *  the slice-set hash the client teleports to (PRV88.88 == 0x47EA4CEA), verified from the mission
 *  scenario bubble table (region = bubble ordinal * 8). */
constexpr std::int32_t kOmegaForestRegionIndex = 88;
constexpr std::uint32_t kOmegaForestRegionHash = 0x47EA4CEAU;



/**
 * Copies one staged frame to the caller and publishes its nonce.
 * @param session Connection-owned send nonce.
 * @param scratch Lock-owned staging storage.
 * @param response Caller-owned complete-frame storage.
 * @param written Receives the published byte count.
 * @param framedSize Staged byte count.
 * @param nextSendNonce Nonce to publish once the copy finishes.
 * @param published True when at least one notification was staged.
 * @return True when the caller received a complete frame.
 */
[[nodiscard]] bool publish_frame(Session& session,
                                 Scratch& scratch,
                                 std::span<std::byte> response,
                                 std::size_t& written,
                                 std::size_t framedSize,
                                 const std::array<std::byte, state::kBapNonceSize>& nextSendNonce,
                                 bool published) noexcept {
    if (!published || framedSize == 0 || framedSize > response.size()) {
        // Nothing left, so a roster staged into the discarded body is offered again next push.
        discard_staged_roster(session);
        return false;
    }
    for (std::size_t index = 0; index < framedSize; ++index) {
        response[index] = scratch.framed[index];
    }
    written = framedSize;
    session.sendNonce = nextSendNonce;
    // Settled only here: the grant and the state byte may move only on a delivered frame.
    commit_staged_roster(session);
    return true;
}

/** Services one exact region debt before any ordinary keepalive can bypass it. */
[[nodiscard]] bool service_region_debt(Session& session,
                                       Scratch& scratch,
                                       std::span<std::byte> response,
                                       std::size_t& written) noexcept {
    RegionTransitionSnapshot snapshot{};
    gameplay::group::HostActivityLineageLease lease{};
    gameplay::group::HostActivityLineageLease boundLineageLease{};
    const RegionSnapshotBuildResult built = build_region_debt_snapshot(
        session,
        scratch,
        session.activity.regionDebt,
        snapshot,
        lease,
        boundLineageLease);
    if (built == RegionSnapshotBuildResult::stale) {
        session.activity.regionDebt = {};
        gameplay::group::release_host_activity_lineage(boundLineageLease);
        return false;
    }
    if (built != RegionSnapshotBuildResult::ready) {
        gameplay::group::release_host_activity_lineage(lease);
        gameplay::group::release_host_activity_lineage(boundLineageLease);
        return false;
    }
    auto nextNonce = session.sendNonce;
    std::size_t framedSize = 0;
    const auto& key = state::bap().sessionKey;
    bool complete = true;
    if (requires_notification(snapshot.required, RegionNotification::globalState)) {
        complete = append_global_state_notification(
            scratch, snapshot.activity, key, nextNonce, scratch.framed, framedSize);
    }
    if (complete && requires_notification(snapshot.required, RegionNotification::membership)) {
        complete = append_membership_notification(
            scratch, snapshot, key, nextNonce, scratch.framed, framedSize);
    }
    if (complete && requires_notification(snapshot.required, RegionNotification::roster)) {
        complete = append_roster_notification(
            session, scratch, snapshot, key, nextNonce, scratch.framed, framedSize);
    }
    state::activity::membership::RegionView currentSource{};
    complete = complete
               && (!lease.pinned
                   || gameplay::group::validate_host_activity_lineage(lease))
               && retained_region_lineage_is_current_locked(
                   session, session.activity.lineage, boundLineageLease)
               && state::activity::membership::snapshot_region_view(
                   snapshot.regionSource, snapshot.sourceHostRegion, currentSource)
               && framedSize != 0 && framedSize <= response.size();
    if (!complete) {
        if (framedSize != 0) {
            SecureZeroMemory(scratch.framed.data(), framedSize);
        }
        discard_staged_roster(session);
        gameplay::group::release_host_activity_lineage(lease);
        gameplay::group::release_host_activity_lineage(boundLineageLease);
        return false;
    }
    std::copy_n(scratch.framed.begin(), framedSize, response.begin());
    written = framedSize;
    session.sendNonce = nextNonce;
    commit_staged_roster(session);
    session.activity.advertisedRegion = snapshot.regionIndex;
    if(requires_notification(snapshot.required,RegionNotification::membership)) {
        state::activity::omega_ending::note_membership_published(snapshot.activity,
            state::activity::mission_run_generation(),GetTickCount64());
        runtime::activity::native_activity_transit::note_membership_published(
            snapshot.activity, GetTickCount64());
    }
    if (snapshot.publishesHud) {
        publish_hud_region_locked(session, snapshot.sourceHostRegion);
    }
    session.activity.regionDebt = {};
    gameplay::group::release_host_activity_lineage(lease);
    gameplay::group::release_host_activity_lineage(boundLineageLease);
    return true;
}

} // namespace

/** Writes the periodic activity-link keepalive when one is due. */
bool consume_activity_keepalive(Session& session,
                                Scratch& scratch,
                                std::span<std::byte> response,
                                std::size_t& written,
                                bool& touchesScratch) noexcept {
    written = 0;
    if (!lifecycle::activity_binding_is_current(session)
        || !state::activity::contains(session.activity.instance)) {
        return false;
    }
    // Drain into the persistent binding before any detached retry snapshot is
    // copied. Delivery failure must not replay or forget native observations.
    if (!session.activity.joinedForeignSession
        && session.activity.sensorObservation.omegaOpeningExecutor.queued() != 0) {
        namespace opening = state::activity::coo::omega::opening;
        auto& observer = session.activity.sensorObservation;
        const auto before = observer.omegaOpeningExecutor.diagnostics();
        const auto changes = opening::update_observer(observer, session.activity.key.generation.value);
        const auto after = observer.omegaOpeningExecutor.diagnostics();
        if (changes.reset) { session.activity.omegaOpeningStage = message::kOmegaOpeningStageNone; }
        if (changes.reset || changes.failed || before.active != after.active || before.complete != after.complete) {
            const auto authority = observer.omegaOpeningExecutor.authority();
            std::array<char, 448> line{};
            const int size = std::snprintf(line.data(), line.size(),
                "ev=coo_opening binding=%llu incarnation=%llu packet=%llu phase=%u active=%08X complete=%08X "
                "ready=%u requested=%u released=%u entrance=%u scene_revision=%u reset=%u failed=%u",
                static_cast<unsigned long long>(session.activity.key.generation.value),
                static_cast<unsigned long long>(after.incarnation), static_cast<unsigned long long>(changes.packet),
                static_cast<unsigned>(after.phase), after.active, after.complete,
                authority.roster ? 1U : 0U, authority.requested ? 1U : 0U, authority.released ? 1U : 0U,
                authority.entrance ? 1U : 0U, changes.sceneRevision, changes.reset ? 1U : 0U, authority.failed ? 1U : 0U);
            if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
                core::log::write(core::log::Channel::server, core::log::Level::info,
                    {line.data(), static_cast<std::size_t>(size)});
            }
        }
    }
    if (session.activity.regionDebt.present) {
        touchesScratch = true;
        return service_region_debt(session, scratch, response, written);
    }
    const std::uint64_t now = GetTickCount64();
    // Loading and native observations open the scheduled burst window.
    const bool omegaOpeningDue = ((session.activity.omegaOpeningStage >= message::kOmegaOpeningStageBaseline
         && session.activity.omegaOpeningStage < message::kOmegaOpeningStageReady)
        || (session.activity.omegaOpeningStage == message::kOmegaOpeningStageReady
            && session.activity.sensorObservation.omegaSceneHandoffArmed)
        || (session.activity.omegaOpeningStage >= message::kOmegaOpeningStageSpawner
            && session.activity.omegaOpeningStage < message::kOmegaOpeningStageSettled)
        || (session.activity.omegaOpeningStage == message::kOmegaOpeningStageSettled
            && session.activity.sensorObservation.omegaForestEntranceTriggered
            && !session.activity.sensorObservation.omegaForestEntranceAuthorityPublished));
    const ActivitySensorObservation& cues = session.activity.sensorObservation;
    const bool towerWatchDue =
        cues.towerWatchRosterReady
        && (cues.towerWatchPublishedStage < 1U
            || (cues.towerWatchPublishedStage < 2U
                && (cues.towerWatchBreachSeen
                    || state::activity::tower_watch_opening_dialogue_processed()))
            || (cues.towerWatchPublishedStage < 3U && cues.towerWatchEncounterChanged));
    const bool nativePopulationDue = !session.activity.joinedForeignSession
        && state::activity::native_population::pending(session.activity.instance);
    const bool nativeCueDue = !session.activity.joinedForeignSession
        && runtime::activity::adventure::native_bridge::pending(session.activity.instance);
    const bool nativeActivityPublicationPending = !session.activity.joinedForeignSession
        && runtime::activity::native_activity::publication_pending(session.activity.instance);
    namespace welcome=state::activity::vendors::presentation;
    const auto& vendorOwner=session.activity.rosterLifetimes.identity;
    const auto player=client::player::position::snapshot();
    // Greeting/departure inputs follow proximity edges, including after loading
    // settles. They must not wait for the five-second idle keepalive.
    const bool vendorPresenceDue=vendorOwner.owner==session.activity.instance.sessionId
        && vendorOwner.incarnation==session.activity.instance.incarnation.value
        && !welcome::groups(vendorOwner.scenario).empty()
        && welcome::presence(session.activity.towerVendorPresence,0,player.position,player.present,vendorOwner.scenario)
            !=session.activity.towerVendorPresence;
    const auto vendorRevision=state::activity::vendors::lifetime::revision(session.activity.instance);
    const bool vendorPopulationDue=vendorRevision!=session.activity.vendorPopulationRevision;
    const bool scheduledBurst = now >= session.activity.rosterDueTick
        && (now < session.activity.transitionUntilTick || omegaOpeningDue || towerWatchDue
            || nativePopulationDue || nativeCueDue || nativeActivityPublicationPending);
    const bool burstDue = !session.activity.joinedForeignSession
        && (vendorPresenceDue || vendorPopulationDue || scheduledBurst);
    const bool endingMembershipDue=!session.activity.joinedForeignSession
        && (state::activity::strike_bond::ending_membership_due(now)
            || state::activity::newlight::launchpad::transit::membership_due(session.activity.instance,
                state::activity::mission_run_generation(),now)
            || state::activity::omega_ending::membership_due(session.activity.instance,
            state::activity::mission_run_generation(),now)
            || state::activity::beyond_infinity::transit::membership_due(session.activity.instance,
                state::activity::mission_run_generation(),now)
            || runtime::activity::native_activity_transit::membership_due(
                session.activity.instance, now)
            || state::activity::vanilla::one_au::transit::membership_due(session.activity.instance,
                state::activity::mission_run_generation(),now));
    const bool keepaliveDue = now >= session.activity.keepaliveDueTick
        || endingMembershipDue
        || (!session.activity.joinedForeignSession
            && (state::activity::omega_presentation::publication_due(now)
                || state::activity::omega_first_lair::publication_due(now)
                || state::activity::vanilla::one_au::publication_due(now)
                || state::activity::gateway::publication_due(now)
                || state::activity::deadly_trial::publication_due(now)
                || state::activity::beyond_infinity::publication_due(now)
                || state::activity::deep_storage::publication_due(now)
                || state::activity::strike_bond::publication_due(now)
                || state::activity::strike_pact::publication_due(now)
                || state::activity::hijacked::publication_due(now)
                || (state::activity::newlight::launchpad::publication_due(now) || state::activity::newlight::launchpad::tower::active() || state::activity::gateway_intro::active())));
    if (session.activity.joinedForeignSession) {
        // This link exists only so the client's second activity instance sees traffic. A roster or
        // membership push on it leaves the transition running with no world entered.
        if (!keepaliveDue) {
            return false;
        }
        touchesScratch = true;
        auto nextSendNonce = session.sendNonce;
        std::size_t framedSize = 0;
        BorrowedClockPublication clockPublication{};
        const bool staged = append_global_state_notification(scratch,
                                                              session.activity.instance,
                                                              state::bap().sessionKey,
                                                              nextSendNonce,
                                                              scratch.framed,
                                                              framedSize)
            && append_borrowed_clock_notifications(session,scratch,state::bap().sessionKey,
                nextSendNonce,scratch.framed,framedSize,clockPublication)
            && borrowed_clock_publication_is_current(session,clockPublication);
        const bool delivered = publish_frame(
            session, scratch, response, written, framedSize, nextSendNonce, staged);
        release_borrowed_clock_publication(clockPublication);
        if(!delivered && framedSize)SecureZeroMemory(scratch.framed.data(),framedSize);
        if (delivered) {
            session.activity.keepaliveDueTick = now + kKeepaliveIntervalMs;
        }
        return delivered;
    }

    state::activity::membership::PendingMutation mutation{};
    state::activity::membership::PeriodicRegionRefresh refresh{};
    if (session.activity.lineage.kind != RegionLineageKind::ownedActivity
        || session.activity.lineage.bound != session.activity.lineage.source
        || !state::activity::membership::prepare_periodic_region_refresh(
            session.activity.instance,
            session.activity.advertisedRegion,
            mutation,
            refresh,
            endingMembershipDue)) {
        return false;
    }
    if (!burstDue && !keepaliveDue && !refresh.regionChanged) {
        SecureZeroMemory(&mutation, sizeof mutation);
        return false;
    }

    touchesScratch = true;
    const bool burst = !keepaliveDue && !refresh.regionChanged;
    const bool rearmMissionDirector = !burst && refresh.regionChanged
                                      && session.activity.missionDirectorActive
                                      && opening_mission_host_ready();
    auto nextSendNonce = session.sendNonce;
    std::size_t framedSize = 0;
    activity_transaction::NotificationStaging staging{};
    const activity_transaction::NotificationStageResult staged =
        activity_transaction::stage_periodic_notifications(session,
                                                           scratch,
                                                           refresh,
                                                           mutation,
                                                           burst,
                                                           rearmMissionDirector,
                                                           state::bap().sessionKey,
                                                           nextSendNonce,
                                                           scratch.framed,
                                                           framedSize,
                                                           staging);
    bool complete = staged == activity_transaction::NotificationStageResult::complete
                    && framedSize != 0 && framedSize <= response.size()
                    && (!staging.advertisementLease.pinned
                        || gameplay::group::validate_host_activity_lineage(
                            staging.advertisementLease))
                    && retained_region_lineage_is_current_locked(
                        session, session.activity.lineage, staging.boundLineageLease);
    if (complete) {
        complete = state::activity::membership::commit(mutation);
    }
    if (!complete) {
        if (framedSize != 0) {
            SecureZeroMemory(scratch.framed.data(), framedSize);
        }
        activity_transaction::discard_notification_staging(session, staging);
        SecureZeroMemory(&mutation, sizeof mutation);
        return false;
    }

    std::copy_n(scratch.framed.begin(), framedSize, response.begin());
    written = framedSize;
    session.sendNonce = nextSendNonce;
    commit_staged_roster(session);
    session.activity.vendorPopulationRevision=vendorRevision;
    if(refresh.publishesMembership) {
        state::activity::omega_ending::note_membership_published(session.activity.instance,
            state::activity::mission_run_generation(),now);
        runtime::activity::native_activity_transit::note_membership_published(
            session.activity.instance, now);
    }
    if (refresh.publishesMembership && refresh.inputs.sourceMembership.region.index >= 0) {
        session.activity.advertisedRegion = refresh.inputs.sourceMembership.region.index;
    }
    session.activity.rosterDueTick = now + kRosterBurstIntervalMs;
    if (!burst) {
        session.activity.keepaliveDueTick = now + kKeepaliveIntervalMs;
    }
    gameplay::group::release_host_activity_lineage(staging.advertisementLease);
    gameplay::group::release_host_activity_lineage(staging.boundLineageLease);
    SecureZeroMemory(&mutation, sizeof mutation);
    return true;
}

} // namespace dawn::server::bap::encrypted::push::activity
