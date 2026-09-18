// Included in the installed object-source owner, under the same CallGate.
#include "hijacked_boss_damage.inl"
#include "strike_pact_boss_damage.inl"
#include "garden_lens_damage.inl"
#include "strike_bond_boss_damage.inl"
using ModuleDamage=void(__fastcall*)(const void*,const void*,std::byte*,bool,bool,const void*,std::int32_t) noexcept;
using ModuleDamageGate=bool(__fastcall*)(const void*) noexcept;
using ModuleDamageSummary=void(__fastcall*)(const void*,std::uint32_t,std::uint32_t,bool,bool,const void*,float) noexcept;
std::atomic<ModuleDamage> g_moduleDamage{};
std::atomic<ModuleDamageGate> g_moduleDamageGate{};
std::atomic<ModuleDamageSummary> g_moduleDamageSummary{};
__declspec(noinline) bool gateway_damage_blocked(const void* context) noexcept {
    if(lost_sector_native_object::blocked(context)) return true;
    if(garden_lens_damage::blocked(context)) return true;
    gateway_native::Read read{g_image};native_box_identity::Sample sample{};
    if(!native_box_identity::sample(read,reinterpret_cast<std::uintptr_t>(context),sample)) { return false; }
    if(gateway_module_damage::blocked(state::activity::gateway::ending_request())) { return true; }
    const auto request=state::activity::beyond_infinity::lens_request();
    AcquireSRWLockShared(&g_lock);const auto candidate=g_beyondLensCandidate;ReleaseSRWLockShared(&g_lock);
    const bool current=beyond_infinity_lens_damage::current(read,request,candidate,sample);
    if(beyond_infinity_lens_damage::blocked(request,current)) {return true;}
    const auto deep=state::activity::deep_storage::lens_request();
    AcquireSRWLockShared(&g_lock);const auto deepCandidate=g_deepLensCandidate;ReleaseSRWLockShared(&g_lock);
    gateway_native::Read deepRead{g_image};
    return deep_storage_lens_damage::blocked(deep,deep_storage_lens_damage::current(deepRead,deep,deepCandidate,sample));
}
__declspec(noinline) void gateway_damage_receipt(const void* context) noexcept {
    lost_sector_native_object::receipt(context);
    garden_lens_damage::receipt(context);
    gateway_native::Read read{g_image};native_box_identity::Sample sample{};
    if(!native_box_identity::sample(read,reinterpret_cast<std::uintptr_t>(context),sample) || !sample.dead) { return; }
    const auto request=state::activity::gateway::ending_request();
    if(gateway_module_damage::current(read,request,sample) && read.weak({request.owner.serial,request.owner.entity})) {
        state::activity::gateway::observe_module(request.owner,true);
    }
    const auto lens=state::activity::beyond_infinity::lens_request();
    // A dead candidate never invents the live receipt required for progression.
    if(lens.lens.valid() && lens.vulnerable && beyond_infinity_lens_damage::current(read,lens,{},sample)) {
        state::activity::beyond_infinity::observe_lens(lens.lens,true);
    }
    const auto deep=state::activity::deep_storage::lens_request();gateway_native::Read deepRead{g_image};
    if(deep.lens.valid() && deep.vulnerable && deep_storage_lens_damage::current(deepRead,deep,{},sample)) {
        state::activity::deep_storage::observe_lens(deep.lens,true);
    }
}
__declspec(noinline) bool __fastcall gateway_damage_gate_hook(const void* context) noexcept {
    const hooking::CallGate::Scope gate{g_gate};
    const bool nativeResult=hooking::await_original(g_moduleDamageGate)(context);
    const bool result=gate.accepts_side_effects()?lost_sector_native_object::allowed(context,
        garden_damage::allowed(context,garden_lens_damage::allowed(context,nativeResult))):nativeResult;
    if(!gate.accepts_side_effects()) { return result; }
    if(strike_pact_damage::immune(context) || hijacked_damage::immune(context) || garden_damage::immune(context)) {return false;}
    gateway_native::Read read{g_image};native_box_identity::Sample sample{};
    if(!native_box_identity::sample(read,reinterpret_cast<std::uintptr_t>(context),sample)) { return result; }
    const auto request=state::activity::gateway::ending_request();
    const bool current=gateway_module_damage::current(read,request,sample)
        && read.weak({request.owner.serial,request.owner.entity});
    const bool gatewayResult=gateway_module_damage::allowed(request,current,result);
    const auto lens=state::activity::beyond_infinity::lens_request();
    AcquireSRWLockShared(&g_lock);const auto candidate=g_beyondLensCandidate;ReleaseSRWLockShared(&g_lock);
    const bool beyondResult=beyond_infinity_lens_damage::allowed(lens,
        beyond_infinity_lens_damage::current(read,lens,candidate,sample),gatewayResult);
    const auto deep=state::activity::deep_storage::lens_request();
    AcquireSRWLockShared(&g_lock);const auto deepCandidate=g_deepLensCandidate;ReleaseSRWLockShared(&g_lock);
    gateway_native::Read deepRead{g_image};
    return deep_storage_lens_damage::allowed(deep,deep_storage_lens_damage::current(deepRead,deep,deepCandidate,sample),beyondResult);
}
__declspec(noinline) void __fastcall gateway_damage_hook(const void* context,const void* damage,std::byte* packet,
    bool mode,bool secondary,const void* extra,std::int32_t index) noexcept {
    const hooking::CallGate::Scope gate{g_gate};
    if(gate.accepts_side_effects() && (gateway_damage_blocked(context) || !strike_pact_damage::before(context,packet) || !hijacked_damage::before(context,packet) || !garden_damage::before(context,packet))) { return; }
    hooking::await_original(g_moduleDamage)(context,damage,packet,mode,secondary,extra,index);
    if(gate.accepts_side_effects()) { strike_pact_damage::after(context);hijacked_damage::after(context);garden_damage::after(context);gateway_damage_receipt(context); }
}
__declspec(noinline) void __fastcall gateway_damage_summary_hook(const void* context,std::uint32_t attacker,std::uint32_t target,
    bool killed,bool mode,const void* regions,float amount) noexcept {
    const hooking::CallGate::Scope gate{g_gate};
    if(gate.accepts_side_effects()) {
        // Observe-only probe for the damage HUD; it never changes a native argument.
        observe_damage_summary(attacker,target,killed,mode,regions,amount);
        if(killed) { gateway_damage_receipt(context); }
    }
    hooking::await_original(g_moduleDamageSummary)(context,attacker,target,killed,mode,regions,amount);
}
