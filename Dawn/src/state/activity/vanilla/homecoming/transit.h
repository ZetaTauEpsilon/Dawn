#pragma once
#include "transit_rules.h"
#include "runtime.h"
#include "../../runtime.h"
#include "../../../../core/logging/log.h"
#include <cstdio>
#include <mutex>
namespace dawn::state::activity::vanilla::homecoming::transit {
inline std::mutex mutex;
inline Transaction transaction;
inline std::uint64_t nextRefresh{};
inline native::Authority project(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t member,
    bool exact,native::Observation observation) noexcept {
    const auto wanted=homecoming::request();native::Authority result{};Scope scope{};
    {
        const std::lock_guard lock(mutex);
        if(!exact || !activity || !run || run!=mission_run_generation()) {return {};}
        if(transaction.bound() && (transaction.scope().owner.run!=run || transaction.scope().activity!=activity
            || transaction.scope().member!=member || (wanted.owner.valid() && wanted.owner!=transaction.scope().owner))) {transaction={};nextRefresh=0;}
        if(transaction.bound()) {static_cast<void>(transaction.project(transaction.scope(),observation));}
        if(wanted.owner.valid() && wanted.owner.run==run) {
            Scope requested{wanted.owner,activity,member,wanted.frame.cinematic.route()};
            if(destination(requested).valid()) {static_cast<void>(transaction.next(requested,observation));}
        }
        if(!transaction.bound()) {return {};}
        scope=transaction.scope();result=transaction.project(scope,observation);
        static Scope lastScope{};
        static native::Teleport lastHost{},lastClient{};
        static std::uint8_t lastWanted{255};
        if(scope!=lastScope || result.host!=lastHost || observation.local!=lastClient
            || wanted.frame.cinematic.route()!=lastWanted) {
            lastScope=scope;lastHost=result.host;lastClient=observation.local;lastWanted=wanted.frame.cinematic.route();
            std::array<char,320> line{};
            std::snprintf(line.data(),line.size(),"ev=homecoming stage=transit run=%llu owner=%u wanted=%u route=%u host=%d/%u/%d client=%d/%u/%d region=%d receipt=%u complete=%u",
                static_cast<unsigned long long>(scope.owner.run),scope.owner.value,lastWanted,scope.route,
                result.host.state,result.host.token,result.host.sliceSetIndex,observation.local.state,
                observation.local.token,observation.local.sliceSetIndex,observation.currentRegion,
                observation.hasTeleport?1U:0U,result.complete?1U:0U);
            core::log::write(core::log::Channel::server,core::log::Level::info,line.data());
        }
    }
    if(result.arrived) {homecoming::observe_arrival(scope.owner,scope.route);}
    return result;
}
inline bool membership_due(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t now) noexcept {
    const auto wanted=homecoming::request();const std::lock_guard lock(mutex);
    const bool pending=transaction.bound() && transaction.scope().activity==activity && transaction.scope().owner.run==run && transaction.pending();
    const bool requested=wanted.owner.valid() && wanted.owner.run==run && destination(wanted.frame.cinematic.route()).valid()
        && (!transaction.bound() || transaction.scope().route!=wanted.frame.cinematic.route());
    if((!pending && !requested) || now<nextRefresh) {return false;}nextRefresh=now+100;return true;
}
}
