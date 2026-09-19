#pragma once
#include "../coo/population_readiness_request.h"
#include "frame.h"
#include "ending_receipts.h"
#include "traversal_catalog.h"
#include "../coo/population_service.h"
#include "../coo/object_service.h"
#include "cannon_gate.h"
namespace dawn::state::activity::gateway {
[[nodiscard]] cannon::Request cannon_request() noexcept;
[[nodiscard]] coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept;
void observe_capacity(std::uint64_t,coo::PopulationCapacity) noexcept;
struct ObjectRequest { coo::Generation owner{};std::array<coo::ObjectState,3> states{};bool enabled{}; };
[[nodiscard]] ObjectRequest object_request() noexcept;
void observe_object(std::size_t,const coo::ObjectReceipt&,bool,float,std::int16_t) noexcept;
void observe_readiness(const EnemyReceipt&,coo::EnemyReadiness) noexcept;
[[nodiscard]] EndingRequest ending_request() noexcept;
void observe_module(const ModuleReceipt&,bool dead) noexcept;
void observe_scene(const SceneReceipt&,bool completed) noexcept;
void observe_vance(const SceneReceipt&,VanceMilestone) noexcept;
[[nodiscard]] bool prepare(std::uint64_t run,bool selected) noexcept;
[[nodiscard]] Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept;
[[nodiscard]] std::uint64_t native_run() noexcept;
[[nodiscard]] EnemyReceipt marcher(std::uint32_t actor) noexcept;
[[nodiscard]] bool publication_due(std::uint64_t now) noexcept;
[[nodiscard]] bool observe_admission(const EnemyReceipt& receipt) noexcept;
[[nodiscard]] bool observe_death(const EnemyReceipt& receipt) noexcept;
void observe_prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t index) noexcept;
void observe_position(float x,float y,float z) noexcept;
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,
    std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept;
}
