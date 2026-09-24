#pragma once
#include "frame.h"
namespace dawn::state::activity::beyond_infinity {
bool prepare(std::uint64_t run,bool selected) noexcept;
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept;
struct Request { coo::Generation owner{};Frame frame{}; };
Request request() noexcept;
void observe_forest_readiness(coo::Generation,std::uint8_t pass,bool ready) noexcept;
void observe_forest_terminal(coo::Generation,std::uint8_t pass) noexcept;
struct LensRequest { coo::Generation owner{};LensReceipt lens{};bool enabled{},vulnerable{},destroyed{};std::uint32_t objectGeneration{}; };
LensRequest lens_request() noexcept;
struct PlateRequest { coo::Generation owner{};PlateReceipt plate{};std::uint32_t revision{};bool enabled{},occupied{},destroyed{},charged{};server::runtime::activity::mission_capture::Publication capture{}; };
PlateRequest plate_request() noexcept;
void observe_plate_binding(const PlateReceipt&) noexcept;
void observe_plate_pose(const PlateReceipt&,server::runtime::activity::mission_device_pose::Sample) noexcept;
void observe_plate(const PlateReceipt&,std::uint32_t revision,float value,bool complete) noexcept;
bool publication_due(std::uint64_t now) noexcept;
void observe_position(float,float,float) noexcept;
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,
    std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept;
void observe_prepared(coo::Generation,coo::Asset) noexcept;
void observe_lens(const LensReceipt&,bool dead) noexcept;
void observe_scene(const SceneReceipt&,bool complete) noexcept;
void observe_transit(coo::Generation,std::uint8_t route) noexcept;
void observe_scene_cue(const SceneReceipt&,std::uint8_t id,std::uint32_t state,std::uint32_t starts) noexcept;
void observe_scene_speech(const SceneReceipt&,std::uint8_t row,std::uint32_t state) noexcept;
}
