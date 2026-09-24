#pragma once
#include "controller.h"
namespace dawn::state::activity::vanilla::one_au {
bool prepare(std::uint64_t,bool) noexcept;
Frame snapshot(std::uint64_t,std::uint64_t,bool) noexcept;
Request request() noexcept;
bool opening_mask(std::uint64_t now) noexcept;
void observe_fly_in_complete() noexcept;
std::uint64_t native_run() noexcept;
bool publication_due(std::uint64_t) noexcept;
void observe_arrival(coo::Generation,std::uint8_t) noexcept;
bool observe_cinematic(const cinematics::Incident&) noexcept;
void observe_actor(std::uint32_t,std::uint8_t,std::uint16_t,const transport::Delta&) noexcept;
void observe_ghost(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::ghost_sense::Output&) noexcept;
void observe_position(float,float,float) noexcept;
void observe_mounted(const MountedPosition&) noexcept;
void observe_native_player(std::uint32_t) noexcept;
void observe_life(coo::Generation,std::uint32_t,bool) noexcept;
recovery::Spawn project_spawn(recovery::Spawn) noexcept;
void observe_submission(std::uint64_t,std::uint32_t,std::int64_t,std::uint32_t,std::uint8_t,std::uint32_t) noexcept;
void observe_prepared(coo::Generation,coo::Asset) noexcept;
void observe_lost(const ComponentReceipt&) noexcept;
void observe_object(const coo::ObjectReceipt&) noexcept;
void observe_interceptor_faction(const coo::ObjectReceipt&,std::int32_t,std::int32_t) noexcept;
void observe_device(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::device_sense::Output&) noexcept;
void observe_source(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::source_sense::Output&) noexcept;
void observe_binding(const ComponentReceipt&) noexcept;
void observe_carry(const ComponentReceipt&,std::uint32_t,bool) noexcept;
void observe_use(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::object_sense::Output&) noexcept;
void observe_destruction(const ComponentReceipt&,bool) noexcept;
bool observe_admission(const EnemyReceipt&) noexcept;
bool observe_death(const EnemyReceipt&) noexcept;
void observe_readiness(const EnemyReceipt&,coo::EnemyReadiness) noexcept;
// Called from the existing shared engine integration; no detour registration.
void observe_native_object(void*) noexcept;
void poll_native_objects() noexcept;
void poll_escape_ship() noexcept;
using Holder=std::uint32_t*(*)(const void*,std::uint32_t*) noexcept;
using Controlled=std::uint32_t*(*)(std::uint32_t*) noexcept;
void observe_native_carry(void*,Holder,Controlled) noexcept;
}
