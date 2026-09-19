#pragma once
#include "state/activity/vanilla/one_au/bridge_native.h"
#include <functional>
#include <map>

namespace bridge_test {
namespace native=m::bridge_native;
namespace gn=native::gn;
inline void resolver_compaction() {
    std::array<std::byte,0x20> directory{},head{};
    std::array<std::byte,0x80> registry{};
    std::array<std::byte,0x40> metadata{};
    std::array<std::byte,0x200> allocations{};
    std::array<std::uint32_t,2> serials{0,0xA1B2C3D4};
    const auto address=[](auto& v){return reinterpret_cast<std::uintptr_t>(v.data());};
    const auto put=[](std::uintptr_t p,const auto& v){std::memcpy(reinterpret_cast<void*>(p),&v,sizeof v);};
    auto directoryPointer=address(directory);
    const auto base=reinterpret_cast<std::uintptr_t>(&directoryPointer)-0x2439C70;
    put(address(directory),address(registry));put(address(directory)+0x10,std::int32_t{0x40});
    put(address(registry)+0x48,address(allocations)+0xC0);put(address(registry)+0x70,std::int32_t{0x40});
    put(address(registry)+0x74,std::int32_t{-1});put(address(registry)+0x50,address(metadata));
    put(address(metadata),address(head));put(address(metadata)+8,address(serials));
    put(address(metadata)+0x1C,std::uint32_t{0});put(address(metadata)+0x20,std::uint32_t{4});
    put(address(head)+0x1C,std::uint16_t{2});
    for(const std::uintptr_t offset:{0x80U,0x180U,0x100U}) {
        const auto allocation=address(allocations)+0x100,target=address(allocations)+offset;
        put(allocation+8,static_cast<std::uint64_t>(allocation-target));
        gn::Read actual{base};std::uintptr_t resolved{},raw{};
        check(actual.resolve(0x2001,resolved,&raw) && resolved==target && raw==allocation,
            "production native resolver handles positive, negative and zero compaction corrections");
        check(actual.weak({serials[1],0x2001}) && !actual.weak({serials[1]+1,0x2001}),
            "production native registry validates salted controller references");
    }
}
constexpr std::uintptr_t image=0x10000000,sensor=0x20000000,definition=0x21000000,controller=0x22000000,row=0x23000000;
constexpr gn::Weak component{0x7D6A4F7D,0x3EF9E235},entity{0x12345678,0x33FAA02D};
constexpr auto wordAddress=image+native::kAuthorityTable+4U*((entity.handle&0x1FFFU)>>5U);
constexpr auto bit=1U<<(entity.handle&31U);
struct Read {
    std::map<std::uintptr_t,std::byte> bytes;
    std::map<std::uint32_t,gn::Weak> handles{{component.handle,component},{entity.handle,entity}};
    std::function<void(Read&,std::uintptr_t)> before;
    template<class T> void put(std::uintptr_t address,const T& value) {
        const auto data=std::as_bytes(std::span{&value,std::size_t{1}});
        for(std::size_t i=0;i<data.size();++i) bytes[address+i]=data[i];
    }
    template<class T> bool value(std::uintptr_t address,T& out) {
        if(before) before(*this,address);
        auto data=std::as_writable_bytes(std::span{&out,std::size_t{1}});
        for(std::size_t i=0;i<data.size();++i) {
            const auto found=bytes.find(address+i);if(found==bytes.end()) return false;data[i]=found->second;
        }
        return true;
    }
    bool resolve(std::uint32_t handle,std::uintptr_t& out) {
        if(handle==m::kBridgeLink.definition) out=definition;
        else if(handle==component.handle) out=controller;
        else return false;
        return true;
    }
    bool make_weak(std::uint32_t handle,gn::Weak& out) {
        const auto found=handles.find(handle);if(found==handles.end()) return false;out=found->second;return true;
    }
    bool weak(gn::Weak wanted) {gn::Weak actual{};return make_weak(wanted.handle,actual) && actual==wanted;}
    bool entity_row(gn::Weak wanted,std::uintptr_t& out) {out=row;return wanted.handle==entity.handle && weak(wanted);}
    Read() {
        // Values captured from the stalled bridge: active authority generation
        // 258, native mode 2, a three-second scan, no participants or elapsed time.
        put(sensor,gn::Ref{m::kBridgeLink.definition,0x80804D32U,0x258});
        put(definition+0x288,m::kBridgeLink.registry);put(definition+0x28C,std::uint16_t{65});put(definition+0x28E,std::uint16_t{60});
        put(sensor+0x1C0,std::uint32_t{258});put(sensor+0x1C4,std::uint8_t{1});put(sensor+0x1C8,std::uint32_t{0x811C9DC5});
        put(sensor+0x1D8,component);put(controller,gn::Ref{0x80C3D38C,0x80804D3A,0x358});
        put(controller+0x24,component.handle);put(controller+0x2C,entity.handle);
        put(row+4,std::uint32_t{0x70020});put(row+0xC,entity.handle);
        put(controller+0x290,3.F);put(controller+0x294,std::uint32_t{258});put(controller+0x298,std::uint8_t{2});
        put(controller+0x299,std::uint8_t{0});put(controller+0x29C,0.F);put(controller+0x80,std::array<gn::Weak,6>{});
        put(wordAddress,std::uint32_t{0x1FFF});put(image+native::kAuthoritySetter,native::kSetterPrefix);
    }
};
inline void run() {
    resolver_compaction();
    auto frame=std::make_unique<m::Frame>();frame->enabled=true;frame->section=static_cast<std::uint8_t>(m::Section::bridge);
    frame->spawnGeneration=257;frame->interactions[0].armed=true;
    const coo::Generation owner{2,257};const auto wanted=m::bridge_scan_request(owner,*frame);
    check(wanted.enabled() && wanted.generation==258,"armed bridge uses its next native Ghost generation");
    const auto rejectsFrame=[&](auto change,const char* why) {
        auto changed=std::make_unique<m::Frame>(*frame);change(*changed);check(!m::bridge_scan_request(owner,*changed).enabled(),why);
    };
    check(!m::bridge_scan_request({},*frame).enabled(),"unselected mission cannot claim console authority");
    rejectsFrame([](auto& f){f.enabled=false;},"disabled mission cannot claim console authority");
    rejectsFrame([](auto& f){f.fault=true;},"faulted mission cannot claim console authority");
    rejectsFrame([](auto& f){f.finished=true;},"finished mission cannot claim console authority");
    rejectsFrame([](auto& f){f.section=0;},"opening cannot claim console authority");
    rejectsFrame([](auto& f){f.interactions[0].armed=false;},"unarmed console cannot claim authority");
    rejectsFrame([](auto& f){f.interactions[0].completed=true;},"completed scan cannot claim authority");
    rejectsFrame([](auto& f){f.recovery.phase=m::recovery::Phase::requested;},"checkpoint handoff cannot claim authority");
    rejectsFrame([](auto& f){f.spawnGeneration=INT32_MAX;},"overflowed generation cannot claim authority");
    Read read;unsigned calls{};native::Identity granted{};
    const auto setter=[&](std::uint32_t target) {
        ++calls;check(target==entity.handle,"only the captured bridge console owner receives authority");
        std::uint32_t word{};read.value(wordAddress,word);read.put(wordAddress,word|bit);
    };
    check(native::retain(read,image,sensor,wanted,[&]{return wanted;},setter,granted)==native::Result::granted && calls==1,
        "stalled bridge receives its missing native owner authority");
    check(granted.controller==component && granted.entity==entity,"grant retains exact salted native identities");
    std::uint32_t word{};float elapsed{};std::array<gn::Weak,6> participants{};
    check(read.value(wordAddress,word) && word==(0x1FFFU|bit),"other entity authority bits remain unchanged");
    check(read.value(controller+0x29C,elapsed) && elapsed==0.F && read.value(controller+0x80,participants)
        && participants==std::array<gn::Weak,6>{},"grant does not summon Ghost, insert participants or advance the timer");
    check(native::retain(read,image,sensor,wanted,[&]{return wanted;},setter,granted)==native::Result::local && calls==1,
        "already-owned console is not written again");
    const auto rejects=[&](auto change,const char* why) {
        Read changed;change(changed);unsigned writes{};native::Identity result{};
        check(native::retain(changed,image,sensor,wanted,[&]{return wanted;},[&](auto){++writes;},result)==native::Result::ignored
            && writes==0,why);
    };
    rejects([](auto& r){r.put(sensor,gn::Ref{0x80B2E6D7,0x80804D32,0x258});},"A Deadly Trial scan cannot receive 1AU authority");
    rejects([](auto& r){r.put(sensor+0x1C0,std::uint32_t{257});},"stale source generation rejected");
    rejects([](auto& r){r.put(sensor+0x1C4,std::uint8_t{0});},"inactive native source rejected");
    rejects([](auto& r){r.put(sensor+0x1C8,std::uint32_t{0});},"foreign interaction selector rejected");
    rejects([](auto& r){r.handles[component.handle].serial++;},"recycled controller reference rejected");
    rejects([](auto& r){r.put(controller,gn::Ref{0x80FBDA45,0x80804D3A,0x358});},"another native scan controller rejected");
    rejects([](auto& r){r.put(controller+0x24,std::uint32_t{0});},"mismatched controller self handle rejected");
    rejects([](auto& r){r.put(controller+0x2C,UINT32_MAX);},"missing owner rejected");
    rejects([](auto& r){r.put(row+0xC,entity.handle+1);},"recycled entity table slot rejected");
    rejects([](auto& r){r.put(row+4,std::uint32_t{1});},"retiring owner rejected");
    rejects([](auto& r){r.put(row+4,std::uint32_t{4});},"dead owner rejected");
    rejects([](auto& r){r.put(controller+0x294,std::uint32_t{257});},"stale controller generation rejected");
    // The entrance candidate repairs ownership independently of native Ghost
    // playback. A pre-start mode/timer must not prevent the ownership handoff.
    Read prestart;prestart.put(controller+0x298,std::uint8_t{1});prestart.put(controller+0x290,0.F);
    const auto before=prestart.bytes;
    check(native::retain(prestart,image,sensor,wanted,[&]{return wanted;},[&](auto) {
        prestart.put(wordAddress,std::uint32_t{0x1FFFU|bit});
    },granted)==native::Result::granted,"console authority is repaired before native playback starts");
    prestart.put(wordAddress,std::uint32_t{0x1FFF});
    check(prestart.bytes==before,"console repair preserves all playback and participant data");
    rejects([](auto& r){r.bytes.erase(wordAddress);},"unreadable authority table rejected");
    rejects([](auto& r){r.put(image+native::kAuthoritySetter,std::uint8_t{0});},"different client setter signature rejected");
    rejects([](auto& r){r.before=[count=0](auto& data,auto address) mutable {
        if(address==sensor+0x1D8 && ++count==2) data.handles[component.handle].serial++;
    };},"controller recycled between validation passes cannot receive authority");
    rejects([](auto& r){r.before=[count=0](auto& data,auto address) mutable {
        if(address==controller+0x2C && ++count==2) data.handles[entity.handle].serial++;
    };},"owner recycled between validation passes cannot receive authority");
    for(const m::BridgeScanRequest changed:{m::BridgeScanRequest{},{{3,257},258},{{2,258},259}}) {
        Read raced;unsigned writes{};
        check(native::retain(raced,image,sensor,wanted,[&]{return changed;},[&](auto){++writes;},granted)==native::Result::ignored
            && writes==0,"mission change before setter cancels authority handoff");
    }
}
}
