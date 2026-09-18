// Generic object initialization receipts at the existing source create/sense boundary.
void observe_gateway_object(void* raw) noexcept {
    lost_sector_native_object::observe_source(raw);
    namespace gateway=state::activity::gateway;namespace coo=state::activity::coo;namespace gn=gateway_native;
    gn::Read read{g_image};const auto source=reinterpret_cast<std::uintptr_t>(raw);std::array<std::byte,16> header{};
    if(!read.copy(source,header)) { return; }
    std::size_t index{};for(;index<std::size(gateway::kEndingObjects);++index) {
        if(prefix(header.data(),gateway::kEndingObjects[index].source.definition,0x80809928U,0x4C8U)) { break; }
    }
    if(index==std::size(gateway::kEndingObjects)) { return; }
    const auto request=gateway::object_request();if(!request.enabled) { return; }
    const auto& desired=request.states[index];std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gn::Weak entity{},after{};
    if(desired.phase<coo::ObjectPhase::create || desired.phase==coo::ObjectPhase::ready || desired.phase==coo::ObjectPhase::retired
        || !read.value(source+0x180,generation) || generation!=desired.generation || !read.value(source+0x2F0,committed) || committed!=generation
        || !read.value(source+0x188,active) || active!=1 || !read.value(source+0x440,entity)) { return; }
    std::uintptr_t row{},controller{};coo::ObjectReceipt receipt{{request.owner.run,generation},gateway::kEndingObjects[index].source,entity.handle,entity.serial};
    if(!read.entity_row(entity,row) || !read.value(row+0x4C,bundle)) { return; }
    std::int32_t revision{-1};
    if(coo_native::component(read,bundle,entity.handle,0x80803910U,controller)) {
        if(!read.value(controller+0x24,receipt.controller) || !read.value(controller+0x960,revision)) { return; }
    }
    std::uint32_t again{};
    if(!read.value(source+0x440,after) || after!=entity || !read.weak(after)
        || !read.value(source+0x180,again) || again!=generation || !read.value(source+0x2F0,again) || again!=generation) { return; }
    // +960 is the controller's acknowledged position revision. The matching
    // revision proves consumption of this desired position, not animation end.
    gateway::observe_object(index,receipt,desired.apply && revision==desired.revision,desired.position,
        revision>=INT16_MIN && revision<=INT16_MAX?static_cast<std::int16_t>(revision):std::int16_t{-1});
}
// Included inside the existing guarded object-source hook translation unit.
// 9F0750 is the native source sense writer: void(source, output_ref).
using SourceSense=void(__fastcall*)(void*,void*) noexcept;
std::atomic<SourceSense> g_gatewaySense{};
std::atomic<std::uint64_t> g_gatewayModuleState{UINT64_MAX};
void observe_gateway_module(void* raw) noexcept {
    observe_beyond_object(raw);
    observe_deep_storage_object(raw);
    observe_hijacked_object(raw);
    state::activity::newlight::launchpad::observe_native_object(raw);
    observe_strike_bond_object(raw);
    state::activity::vanilla::one_au::observe_native_object(raw);
    observe_gateway_object(raw);
    observe_trial_object(raw);
    namespace gateway=state::activity::gateway;
    namespace gn=gateway_native;
    gn::Read read{g_image};const auto source=reinterpret_cast<std::uintptr_t>(raw);
    std::array<std::byte,16> header{};
    if(!read.copy(source,header) || !prefix(header.data(),0x80F46F23U,0x80809928U,0x4C8U)) { return; }
    const auto request=gateway::ending_request();
    if(!request.enabled || request.moduleDestroyed || request.run==0) { return; }
    std::uint32_t generation{},committed{};std::uint8_t active{};gn::Weak entity{};
    if(!read.value(source+0x180,generation) || !read.value(source+0x2F0,committed)
        || !read.value(source+0x188,active) || !read.value(source+0x440,entity)) { return; }
    const auto state=(static_cast<std::uint64_t>(generation)<<32)^entity.handle
        ^(static_cast<std::uint64_t>(committed)<<16)^request.run^(static_cast<std::uint64_t>(active)<<63);
    if(g_gatewayModuleState.exchange(state,std::memory_order_relaxed)!=state) {
        report_capped(g_gatewayModuleLines,24U,"ev=gateway stage=module_source run=%llu requested=%u expected=%u committed=%u active=%u source=%016llX entity=%08X serial=%08X",
            static_cast<unsigned long long>(request.run),generation,request.generation,committed,static_cast<unsigned>(active),
            static_cast<unsigned long long>(source),entity.handle,entity.serial);
    }
    if(generation!=request.generation || committed!=generation || active!=1) { return; }
    std::uintptr_t row{};std::uint32_t bundle{};
    if(!read.entity_row(entity,row) || !read.value(row+0x4C,bundle)) { return; }
    gateway_module_native_path::Probe probe{};
    if(!gateway_module_native_path::find(read,bundle,entity.handle,probe)) {
        report_capped(g_gatewayModuleLines,16U,"ev=gateway stage=module_health_unresolved run=%llu source=%016llX entity=%08X resources=%u mutation=observe_only",
            static_cast<unsigned long long>(request.run),static_cast<unsigned long long>(source),entity.handle,probe.resources);return;
    }
    gn::Weak after{};std::uint32_t afterGeneration{};std::array<std::byte,16> afterHeader{};
    if(!read.value(source+0x440,after) || after!=entity || !read.weak(after)
        || !read.value(source+0x180,afterGeneration) || afterGeneration!=generation
        || !read.copy(source,afterHeader) || afterHeader!=header) { return; }
    const gateway::ModuleReceipt receipt{request.run,generation,source,entity.handle,entity.serial,probe.health};
    // An initial sample already marked dead cannot invent the live owner receipt.
    gateway::observe_module(receipt,probe.dead);
}
__declspec(noinline) void __fastcall gateway_sense_hook(void* source,void* output) noexcept {
    const hooking::CallGate::Scope gate{g_gate};
    if(gate.accepts_side_effects()) { observe_gateway_module(source); }
    hooking::await_original(g_gatewaySense)(source,output);
}
