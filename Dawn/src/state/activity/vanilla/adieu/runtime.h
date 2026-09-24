#pragma once
#include "controller.h"
namespace dawn::state::activity::vanilla::adieu {
bool prepare(std::uint64_t,bool) noexcept;
// Game-thread poll. True means a new loadout was committed and all account
// peers must be refreshed after this function has released its locks.
bool prepare_starting_loadout() noexcept;
Frame snapshot(std::uint64_t,std::uint64_t,bool) noexcept;
Request request() noexcept;
std::uint64_t native_run() noexcept;
bool publication_due(std::uint64_t) noexcept;
void observe_arrival(coo::Generation,std::uint8_t) noexcept;
bool observe_cinematic(const cinematics::Incident&) noexcept;
void observe_position(float,float,float) noexcept;
void observe_scene(std::uint64_t,std::uint32_t,std::uint16_t,const scene_wire::Output&) noexcept;
PlaybackRequest playback_request(std::uint32_t definition) noexcept;
void observe_playback(const PlaybackReceipt&) noexcept;
void observe_submission(std::uint64_t,std::uint32_t,std::int64_t,std::uint32_t,std::uint8_t,std::uint32_t) noexcept;
void observe_prepared(coo::Generation,coo::Asset) noexcept;
void observe_object(const coo::ObjectReceipt&) noexcept;
void observe_use(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::object_sense::Output&) noexcept;
void observe_source(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::source_sense::Output&) noexcept;
bool observe_admission(const EnemyReceipt&) noexcept;
bool observe_death(const EnemyReceipt&) noexcept;
void observe_readiness(const EnemyReceipt&,coo::EnemyReadiness) noexcept;
void observe_native_object(void*) noexcept;
GrantRequest grant_request() noexcept;
bool observe_granted(const GrantRequest&,std::uint64_t) noexcept;
bool commit_grant(const GrantRequest&,std::uint64_t,bool,void*,bool(*)(void*) noexcept) noexcept;
}
