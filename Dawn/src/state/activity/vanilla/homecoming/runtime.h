#pragma once
#include "controller.h"
namespace dawn::state::activity::vanilla::homecoming {
bool prepare(std::uint64_t,bool) noexcept;
Frame snapshot(std::uint64_t,std::uint64_t,bool) noexcept;
Request request() noexcept;
bool opening_mask(std::uint64_t now) noexcept;
void observe_fly_in_complete() noexcept;
std::uint64_t native_run() noexcept;
bool publication_due(std::uint64_t) noexcept;
void observe_arrival(coo::Generation,std::uint8_t) noexcept;
bool observe_cinematic(const cinematics::Incident&) noexcept;
void observe_ghost(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::ghost_sense::Output&) noexcept;
void observe_position(float,float,float) noexcept;
void observe_mounted(const MountedPosition&) noexcept;
void observe_submission(std::uint64_t,std::uint32_t,std::int64_t,std::uint32_t,std::uint8_t,std::uint32_t) noexcept;
void observe_prepared(coo::Generation,coo::Asset) noexcept;
void observe_object(const coo::ObjectReceipt&) noexcept;
void observe_device(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::device_sense::Output&) noexcept;
void observe_source(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::source_sense::Output&) noexcept;
void observe_use(std::uint32_t,std::uint8_t,std::uint16_t,const middleware::bap::activity_message::object_sense::Output&) noexcept;
bool observe_admission(const EnemyReceipt&) noexcept;
bool observe_death(const EnemyReceipt&) noexcept;
void observe_readiness(const EnemyReceipt&,coo::EnemyReadiness) noexcept;
void observe_door_handled() noexcept;
// Called from the existing shared engine integration; no detour registration.
void observe_native_object(void*) noexcept;
void poll_bazaar_door() noexcept;
}
