#pragma once
#include "../src/state/activity/Newlight/launchpad/lighting_placed.h"
#include <map>
namespace launchpad_lighting_placed_tests {
namespace light=lp::lighting;
struct Context {
    static constexpr std::uintptr_t image=0x100000,tls=0x20000,block=0x30000,queue=0x40000;
    std::map<std::uintptr_t,std::array<std::byte,8>> memory;
    template<class T> void put(std::uintptr_t a,T v){std::memcpy(memory[a].data(),&v,sizeof v);}
    template<class T> bool value(std::uintptr_t a,T& v){
        const auto i=memory.find(a);if(i==memory.end() || sizeof v>8) {return false;}
        std::memcpy(&v,i->second.data(),sizeof v);return true;
    }
    Context(){put(image+0x330F0E0,std::uint32_t{2});put(tls+16,block);put(block+0x2428,queue);put(queue,std::uint32_t{1});}
};
inline void verify() {
    Context context;const auto ready=[&]{return light::simulation_context(context,context.image,context.tls);};
    CHECK(ready());CHECK(!light::simulation_context(context,context.image,0));
    context.put(context.block+0x2428,std::uintptr_t{});CHECK(!ready());
    context.put(context.block+0x2428,context.queue);context.put(context.queue,std::uint32_t{});CHECK(!ready());
    context.put(context.queue,std::uint32_t{65});CHECK(!ready());context.put(context.queue,std::uint32_t{1});CHECK(ready());
    context.put(context.image+0x330F0E0,std::uint32_t{1024});CHECK(!ready());
    using F=launchpad_shutter_tests::Fixture;
    const auto init=[](F& f) {
        f.put(f.physical,light::gn::Ref{0x80C7069BU,0x80803910U,0xA78});
        f.put(f.physical+0x70,UINT32_MAX);f.put(f.physical+0x960,std::int32_t{-1});
    };
    const auto binding=[](F& f) {
        light::Binding b{};b.object={{1,2},light::kSource,f.logicalEntity.handle,f.logicalEntity.serial};
        b.self=f.logicalDevice.handle;b.revision=2;b.target=1.F;return b;
    };
    F f;init(f);light::Placed selected{};
    CHECK(light::placed(f,binding(f),f.device.handle,selected));
    CHECK(selected.entity==f.entity && selected.device==f.device);
    light::Placed captured{};CHECK(light::capture(f,f.physical,captured));CHECK(captured==selected);
    const auto saved=selected;
    CHECK(light::select(f,binding(f),std::array{saved,light::Placed{},saved},selected));CHECK(selected==saved);
    auto stale=saved;++stale.device.serial;
    CHECK(!light::select(f,binding(f),std::array{stale},selected));
    CHECK(!light::select(f,binding(f),std::array<light::Placed,0>{},selected));
    CHECK(!light::placed(f,binding(f),f.logicalDevice.handle,selected));
    F foreign;init(foreign);foreign.put(foreign.physical,0x80C7069CU);
    CHECK(!light::placed(foreign,binding(foreign),foreign.device.handle,selected));
    F displaced;init(displaced);displaced.put(displaced.row+0xD0,999U);
    CHECK(!light::placed(displaced,binding(displaced),displaced.device.handle,selected));
    F zero;init(zero);zero.put(zero.row+0xD0,std::array<std::byte,16>{});zero.put(zero.logicalRow+0xD0,std::array<std::byte,16>{});
    CHECK(!light::placed(zero,binding(zero),zero.device.handle,selected));
    F bound;init(bound);bound.put(bound.physical+0x70,123U);
    CHECK(!light::placed(bound,binding(bound),bound.device.handle,selected));
    CHECK(!light::capture(bound,bound.physical,captured));
    F already;init(already);already.put(already.physical+0x960,2);
    CHECK(!light::placed(already,binding(already),already.device.handle,selected));
    F active;init(active);active.put(active.physical+0x37C,1.F);
    CHECK(!light::placed(active,binding(active),active.device.handle,selected));
    F retired;init(retired);retired.put(retired.row+4,4U);
    CHECK(!light::placed(retired,binding(retired),retired.device.handle,selected));
    CHECK(!light::capture(retired,retired.physical,captured));
    F owner;init(owner);owner.put(owner.physical+0x2C,999U);
    CHECK(!light::placed(owner,binding(owner),owner.device.handle,selected));
    F moved;init(moved);moved.physicalAddress=0x6000;
    std::memcpy(moved.memory.data()+moved.physicalAddress,moved.memory.data()+moved.physical,0xA78);
    moved.put(moved.physical,0xDEADBEEFU);
    CHECK(light::placed(moved,binding(moved),moved.device.handle,selected));
    CHECK(!light::capture(moved,moved.physical,captured));
    CHECK(light::capture(moved,moved.physicalAddress,captured));CHECK(captured==saved);
    struct Pair : F {
        light::gn::Weak extraEntity{81,9},extraDevice{82,10};
        const std::uintptr_t extraAddress=0x6800,extraRow=0x7800;
        bool weak(light::gn::Weak v){return v==extraEntity || v==extraDevice || F::weak(v);}
        bool make_weak(std::uint32_t h,light::gn::Weak& out){
            for(const auto v:{extraEntity,extraDevice}) {if(v.handle==h){out=v;return true;}}return F::make_weak(h,out);
        }
        bool resolve(std::uint32_t h,std::uintptr_t& out){if(h==extraDevice.handle){out=extraAddress;return true;}return F::resolve(h,out);}
        bool entity_row(light::gn::Weak v,std::uintptr_t& out){if(v==extraEntity){out=extraRow;return true;}return F::entity_row(v,out);}
    };
    Pair pair;init(pair);
    std::memcpy(pair.memory.data()+pair.extraAddress,pair.memory.data()+pair.physical,0xA78);
    pair.put(pair.extraAddress+0x24,pair.extraDevice.handle);pair.put(pair.extraAddress+0x2C,pair.extraEntity.handle);
    pair.put(pair.extraRow+0xC,pair.extraEntity.handle);
    pair.put(pair.extraRow+0xD0,std::array<std::uint32_t,4>{1,2,3,4});
    light::Placed extra{};CHECK(light::capture(pair,pair.extraAddress,extra));
    CHECK(!light::select(pair,binding(pair),std::array{saved,extra},selected));
    pair.put(pair.extraRow+0xD0,999U);
    CHECK(light::select(pair,binding(pair),std::array{saved,extra},selected));CHECK(selected==saved);
    const auto bytes=light::position_record(2);
    const auto get=[&]<class T>(std::size_t offset){T v{};std::memcpy(&v,bytes.data()+offset,sizeof v);return v;};
    CHECK(get.operator()<std::int32_t>(0)==1 && get.operator()<std::uint32_t>(0x10)==0x80805063U);
    for(auto offset:{0x20U,0x24U,0x2CU,0x30U,0x3CU}) {CHECK(get.operator()<std::int32_t>(offset)==-1);}
    CHECK(get.operator()<std::int32_t>(0x38)==2 && get.operator()<float>(0x40)==1.F);
    CHECK(get.operator()<float>(0x28)==1.F && get.operator()<float>(0x34)==0.F);
}
}
