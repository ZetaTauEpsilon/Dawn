#pragma once
#include "../coo/population_readiness_request.h"
#include "frame.h"
#include "scan_playback.h"
namespace dawn::state::activity::hijacked {
void observe_objective_readiness(coo::Generation,std::uint32_t,std::uintptr_t,std::uintptr_t,bool,std::uint32_t logicalRevision=UINT32_MAX,std::uint32_t visibleRow=UINT32_MAX) noexcept;
[[nodiscard]] coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept;
bool prepare(std::uint64_t,bool) noexcept;
Frame snapshot(std::uint64_t,std::uint64_t,bool) noexcept;
Request request() noexcept;
std::uint64_t native_run() noexcept;
bool publication_due(std::uint64_t) noexcept;
void observe_position(float,float,float) noexcept;
void observe_submission(std::uint64_t,std::uint32_t,std::int64_t,std::uint32_t,std::uint8_t,std::uint32_t) noexcept;
void observe_prepared(coo::Generation,coo::Asset) noexcept;
void observe_object(const coo::ObjectReceipt&) noexcept;
bool observe_admission(const EnemyReceipt&) noexcept;
bool observe_death(const EnemyReceipt&) noexcept;
bool suspend_exterior(coo::Generation,std::uint16_t,std::span<const EnemyReceipt>) noexcept;
bool resume_exterior(coo::Generation,std::uint16_t) noexcept;
BossRequest boss_request() noexcept;
bool observe_boss_position(const EnemyReceipt&,std::uint8_t,std::uint32_t) noexcept;
bool observe_health(const EnemyReceipt&,float) noexcept;
void observe_readiness(const EnemyReceipt&,coo::EnemyReadiness) noexcept;
struct LivingEnemies {coo::Generation owner{};std::array<EnemyReceipt,256> actors{};std::size_t count{};};
LivingEnemies living_enemies() noexcept;
LivingEnemies retirement_enemies() noexcept;
PlateRequest plate_request(std::size_t) noexcept;
void observe_plate_binding(const PlateReceipt&) noexcept;
void observe_plate_pose(const PlateReceipt&,server::runtime::activity::mission_device_pose::Sample) noexcept;
void observe_plate(const PlateReceipt&,std::uint32_t,float,bool) noexcept;
void observe_contested_positions(const PlateReceipt&,std::span<const EnemyPosition>,bool complete) noexcept;
ScanRequest scan_request(std::size_t) noexcept;
void observe_scan_binding(const ScanReceipt&) noexcept;
void observe_scan_playback(const ScanReceipt&,ScanPlayback,bool participant) noexcept;
}
