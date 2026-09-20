#include "../../../../../state/activity/Newlight/launchpad/transit.h"
#include "../../../../../state/activity/vanilla/one_au/transit.h"
#include "../../../../../state/activity/vanilla/homecoming/transit.h"
#include "../../../../../state/activity/vanilla/homecoming/prologue.h"
#include "activity_membership_push.h"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <string_view>

#include "../../../../../core/logging/log.h"
#include "../../../../../middleware/bap/activity_message/replicate_membership.h"
#include "../../../../../middleware/secure_channel/runtime.h"
#include "../../../../../state/build_data/runtime.h"
#include "../../../../../state/activity/runtime.h"
#include "../../../../../state/activity/omega_ending.h"
#include "../../../../../state/activity/beyond_infinity/transit.h"
#include "../../../../gameplay/gameplay_advertisement.h"
#include "activity_arrival.h"
#include "activity_notification_frame.h"
#include "../../../../runtime/activity/native_activity_transit.h"

namespace dawn::server::bap::encrypted::push::activity {
namespace {

namespace message = middleware::bap::activity_message::replicate_membership;

/** The one published member always occupies slot zero of both top-level masks. */
constexpr std::uint8_t kLocalMemberSlot = 0;

/**
 * Maps a lock-consistent State snapshot into the fixed Middleware schema.
 * @param sessionId Joined activity session, used to resolve the advertised region.
 * @param mutation Prepared membership operation, whose region this body publishes.
 * @return Whole current membership encoder input.
 */
[[nodiscard]] bool
make_wire_snapshot(state::activity::ActivityInstanceKey activity,
                   const state::activity::membership::PendingMutation& mutation,
                   message::MembershipSnapshot& wire) noexcept {
    const state::activity::membership::Snapshot& snapshot = mutation.snapshot;
    wire = {};
    wire.identity.memberKey = snapshot.identity.memberKey;
    wire.identity.field1 = snapshot.identity.smallOpaque;
    wire.identity.field2 = snapshot.identity.signedOpaque;
    wire.identity.field3 = snapshot.identity.joinIdentity;
    wire.identity.accountSoid = snapshot.identity.accountSoid;
    wire.identity.field5 = snapshot.identity.opaqueSoid;
    wire.identity.field6 = snapshot.identity.secondaryOpaque;
    wire.spawn.state = snapshot.spawn.state;
    wire.spawn.opaqueByte = snapshot.spawn.opaqueByte;
    wire.spawn.opaqueValue = snapshot.spawn.opaqueValue;
    wire.teleport.state = snapshot.teleport.state;
    wire.teleport.token = snapshot.teleport.token;
    wire.teleport.sliceSetIndex = snapshot.teleport.sliceSetIndex;
    wire.teleport.sliceSetHash = snapshot.teleport.sliceSetHash;
    wire.revision = snapshot.revision;
    wire.epoch = snapshot.epoch;
    wire.transitionToken = snapshot.transitionToken;
    wire.hasSynchronizationToken=snapshot.hasSynchronizationToken;
    wire.synchronizationToken=snapshot.synchronizationToken;
    state::activity::membership::RegionSnapshotInputs copied{};
    if (!state::activity::membership::snapshot_region_inputs(
            activity, activity, {}, copied)) {
        return false;
    }
    const auto& transition = mutation.regionTransition;
    const std::int32_t reported =
        transition.activity == activity ? transition.after.region.index
                                        : copied.sourceMembership.region.index;
    state::activity::membership::SpawnState recovery{};
    if(runtime::activity::native_activity_transit::project_respawn(activity,
        snapshot.identity.memberKey,snapshot.spawn,reported,recovery))
        wire.spawn={recovery.state,recovery.opaqueByte,recovery.opaqueValue};
    const std::string_view name(
        reinterpret_cast<const char*>(copied.destination.packageName.data()),
        copied.destination.packageNameLength);
    state::build_data::scenarios::Definition layout{};
    static_cast<void>(state::build_data::find_scenario_layout(name, layout));
    const state::activity::omega_ending_transit::Observation nativeTransit{
        {snapshot.teleport.state,snapshot.teleport.token,snapshot.teleport.sliceSetIndex,
            snapshot.teleport.sliceSetHash},reported,snapshot.hasTeleportReceipt,reported>=0};
    auto terminal=state::activity::omega_ending::project_transit({activity,
        state::activity::mission_run_generation(),snapshot.identity.memberKey,
        name=="mission_scot" && layout.tag==0x80F47522U,
        nativeTransit});
    const auto beyond=state::activity::beyond_infinity::transit::project(activity,
        state::activity::mission_run_generation(),snapshot.identity.memberKey,
        name=="adventure_vod" && layout.tag==state::activity::beyond_infinity::kScenario,nativeTransit);
    if(beyond.publish) { terminal=beyond; }
    const auto launchpad=state::activity::newlight::launchpad::transit::project(activity,
        state::activity::mission_run_generation(),snapshot.identity.memberKey,
        name=="mission_launchpad" && layout.tag==state::activity::newlight::launchpad::kScenario,nativeTransit,
        state::activity::newlight::launchpad::transit::dock_arrived(snapshot.currentLeg,snapshot.pendingLeg),
        state::activity::newlight::launchpad::transit::divide_retained(snapshot.currentLeg,snapshot.pendingLeg),snapshot.transitionToken);
    if(name=="mission_launchpad" && layout.tag==state::activity::newlight::launchpad::kScenario) {
        state::activity::newlight::launchpad::transit::echo_regions(snapshot,wire);
    }
    if(launchpad.publish) {terminal=launchpad;}
    const auto approach=state::activity::newlight::launchpad::tower::project(activity,
        state::activity::mission_run_generation(),snapshot.identity.memberKey,
        name=="cine_110_twr" && layout.tag==state::activity::newlight::launchpad::tower::kApproachScenario,nativeTransit,GetTickCount64());
    if(approach.publish) {terminal=approach;}
    const auto briefing=state::activity::gateway_intro::project(activity,
        state::activity::mission_run_generation(),snapshot.identity.memberKey,
        name==state::activity::gateway_intro::kPackage && layout.tag==state::activity::gateway_intro::kScenario,nativeTransit,GetTickCount64());
    if(briefing.publish) {terminal=briefing;}
    const auto generic = runtime::activity::native_activity_transit::project(
        activity, snapshot.identity.memberKey, snapshot.hasTeleportReceipt,
        {snapshot.teleport.state, snapshot.teleport.token, snapshot.teleport.sliceSetIndex,
         snapshot.teleport.sliceSetHash}, reported);
    if (generic.present && !terminal.publish) {
        terminal = {{generic.host.state, generic.host.token, generic.host.sliceSetIndex,
                     generic.host.sliceSetHash}, generic.present, generic.arrived,
                    generic.released};
    }
    const auto oneAu=state::activity::vanilla::one_au::transit::project(activity,
        state::activity::mission_run_generation(),snapshot.identity.memberKey,
        name=="mission_ember" && layout.tag==state::activity::vanilla::one_au::kScenario,nativeTransit);
    if(oneAu.publish) {terminal=oneAu;}
    if(name=="mission_ember" && layout.tag==state::activity::vanilla::one_au::kScenario) {
        const auto leg=[](const auto& v) {
            return middleware::bap::activity_message::replicate_membership::RegionLeg{
                v.sliceSetIndex,v.sliceSetHash,v.regionIndex,v.publicState,v.auxState,v.present};
        };
        wire.currentLeg=leg(snapshot.currentLeg);wire.pendingLeg=leg(snapshot.pendingLeg);
        wire.localAmbassador=true;
        const auto spawn=state::activity::vanilla::one_au::project_spawn({snapshot.spawn.state,snapshot.spawn.opaqueByte,snapshot.spawn.opaqueValue});
        wire.spawn={spawn.state,spawn.token,spawn.value};
    }
    const auto homecomingTransit=state::activity::vanilla::homecoming::transit::project(activity,
        state::activity::mission_run_generation(),snapshot.identity.memberKey,
        name=="mission_towerfall" && layout.tag==state::activity::vanilla::homecoming::kScenario,nativeTransit);
    if(homecomingTransit.publish) {terminal=homecomingTransit;}
    if(name=="mission_towerfall" && layout.tag==state::activity::vanilla::homecoming::kScenario) {
        const auto leg=[](const auto& v) {
            return middleware::bap::activity_message::replicate_membership::RegionLeg{
                v.sliceSetIndex,v.sliceSetHash,v.regionIndex,v.publicState,v.auxState,v.present};
        };
        wire.currentLeg=leg(snapshot.currentLeg);wire.pendingLeg=leg(snapshot.pendingLeg);
        wire.localAmbassador=true;
    }
    if(terminal.publish) {
        wire.teleport={terminal.host.state,terminal.host.token,terminal.host.sliceSetIndex,
            terminal.host.sliceSetHash};
    }
    const EffectiveRegion region = resolve_region(copied.defaults.defaultDestination,
                                                  copied.destination,
                                                  reported,
                                                  true,
                                                  name,
                                                  layout);
    if (!region.valid || !message::select_active_region(wire, region.index)) {
        return false;
    }
    server::gameplay::build_advertisement(activity,
                                          region.index,
                                          region.reported ? server::gameplay::RegionSource::reported
                                                          : server::gameplay::RegionSource::arrival,
                                          kLocalMemberSlot,
                                          wire.citizen);
    wire.hasHostSynchronizationToken=state::activity::omega_ending_transit::host_synchronization_ready(
        terminal,nativeTransit,snapshot.hasSynchronizationToken,snapshot.synchronizationToken,
        wire.citizen.present,wire.citizen.regionIndex);
    if(wire.hasHostSynchronizationToken) { wire.hostSynchronizationToken=terminal.host.token; }
    return true;
}

} // namespace

/** Appends one current membership svc9 notification and advances its local nonce. */
bool append_membership_notification(Scratch& scratch,
                                    const activity_message::ActivityPlan& activity,
                                    std::span<const std::byte, state::kAesKeySize> key,
                                    std::array<std::byte, state::kBapNonceSize>& nonce,
                                    std::span<std::byte> response,
                                    std::size_t& written) noexcept {
    if (written > response.size() || !static_cast<bool>(activity.instanceKey)
        || activity.sessionId != activity.instanceKey.sessionId
        || activity.membershipMutation.instanceKey != activity.instanceKey
        || !activity.membershipMutation.hasSnapshot) {
        return false;
    }

    const std::size_t initialWritten = written;
    auto initialNonce = nonce;
    std::size_t messageSize = 0;
    message::MembershipSnapshot snapshot{};
    const bool hasSnapshot =
        make_wire_snapshot(activity.instanceKey, activity.membershipMutation, snapshot);
    const bool encoded =
        hasSnapshot
        && message::encode_replicate_membership(snapshot, scratch.responseBody, messageSize)
        && append_notification_frame(scratch,
                                     activity.sessionId,
                                     message::kMessageType,
                                     std::span(scratch.responseBody).first(messageSize),
                                     key,
                                     nonce,
                                     response,
                                     written);
    SecureZeroMemory(scratch.responseBody.data(), message::encoded_size(snapshot));
    if (encoded) {
        middleware::secure_channel::advance_nonce(nonce);
        std::array<char, core::log::kLineCapacity> line{};
        const int lineSize = std::snprintf(
            line.data(),
            line.size(),
            "ev=activity stage=membership_reflection result=sent message_type=%u bytes=%zu "
            "revision=%u transition_token=%u teleport_state=%d teleport_token=%u "
            "teleport_slice=%d teleport_hash=0x%08X advertised_region=%d",
            message::kMessageType,
            messageSize,
            snapshot.revision,
            static_cast<unsigned>(snapshot.transitionToken),
            static_cast<int>(snapshot.teleport.state),
            static_cast<unsigned>(snapshot.teleport.token),
            snapshot.teleport.sliceSetIndex,
            snapshot.teleport.sliceSetHash,
            snapshot.citizen.present ? snapshot.citizen.regionIndex : -1);
        if (lineSize > 0) {
            core::log::write(core::log::Channel::server,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(lineSize)});
        }
    } else {
        if (written > initialWritten) {
            SecureZeroMemory(response.data() + initialWritten, written - initialWritten);
        }
        written = initialWritten;
        nonce = initialNonce;
    }
    SecureZeroMemory(&initialNonce, sizeof initialNonce);
    return encoded;
}

/** Appends one immutable coordinator-owned membership snapshot. */
bool append_membership_notification(
    Scratch& scratch,
    const RegionTransitionSnapshot& transition,
    std::span<const std::byte, state::kAesKeySize> key,
    std::array<std::byte, state::kBapNonceSize>& nonce,
    std::span<std::byte> response,
    std::size_t& written) noexcept {
    if (written > response.size() || !static_cast<bool>(transition.binding)
        || !static_cast<bool>(transition.activity) || transition.regionIndex < 0) {
        return false;
    }
    const std::size_t initialWritten = written;
    const auto initialNonce = nonce;
    std::size_t messageSize = 0;
    const bool encoded = message::encode_replicate_membership(
                             transition.membershipWire,
                             scratch.responseBody,
                             messageSize)
                         && append_notification_frame(
                             scratch,
                             transition.activity.sessionId,
                             message::kMessageType,
                             std::span(scratch.responseBody).first(messageSize),
                             key,
                             nonce,
                             response,
                             written);
    SecureZeroMemory(scratch.responseBody.data(),
                     message::encoded_size(transition.membershipWire));
    if (encoded) {
        middleware::secure_channel::advance_nonce(nonce);
    } else {
        if (written > initialWritten) {
            SecureZeroMemory(response.data() + initialWritten, written - initialWritten);
        }
        written = initialWritten;
        nonce = initialNonce;
    }
    return encoded;
}

} // namespace dawn::server::bap::encrypted::push::activity
