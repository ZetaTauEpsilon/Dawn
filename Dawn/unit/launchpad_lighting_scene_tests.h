#pragma once
#include "../src/state/activity/Newlight/launchpad/lighting_scene.h"
#include <map>
namespace launchpad_lighting_scene_tests {
namespace sc=lp::lighting::scene;
namespace gn=lp::lighting::gn;
struct Fixture {
    static constexpr std::uintptr_t device=0x1000,identity=0x2000,publisher=0x2400,
        sound=0x3000,bank=0x3400,row=0x4000,bundle=0x5000,metadata=0x6000;
    std::array<std::byte,0x8000> memory{};
    std::map<std::uint32_t,std::uintptr_t> handles{{1,device},{2,identity},{3,publisher},
        {4,sound},{5,bank},{7,bundle},{8,metadata}};
    gn::Weak entity{100,6},weakDevice{101,1};bool stale{};
    std::uintptr_t forbiddenBegin{},forbiddenEnd{};unsigned forbiddenReads{};
    template<class T> void put(std::uintptr_t a,const T& value){std::memcpy(memory.data()+a,&value,sizeof value);}
    template<class T> bool value(std::uintptr_t a,T& out){
        if(forbiddenEnd && a<forbiddenEnd && a+sizeof out>forbiddenBegin) {++forbiddenReads;return false;}
        if(a>memory.size() || sizeof out>memory.size()-a) {return false;}
        std::memcpy(&out,memory.data()+a,sizeof out);return true;
    }
    bool resolve(std::uint32_t h,std::uintptr_t& out,std::uintptr_t* allocation=nullptr){
        const auto found=handles.find(h);if(found==handles.end()) {return false;}
        out=found->second;if(allocation) {*allocation=out;}return true;
    }
    bool make_weak(std::uint32_t h,gn::Weak& out){
        if(h==entity.handle) {out=entity;return true;}
        if(h==weakDevice.handle) {out=weakDevice;return true;}return false;
    }
    bool weak(gn::Weak v){return !stale && (v==entity || v==weakDevice);}
    bool entity_row(gn::Weak v,std::uintptr_t& out){if(!weak(v) || v!=entity) {return false;}out=row;return true;}
    Fixture(){
        const auto component=[&](std::uintptr_t a,gn::Ref header,std::uint32_t self){
            put(a,header);put(a+0x24,self);put(a+0x2C,entity.handle);
        };
        component(device,{sc::kHeader[0],sc::kHeader[1],0xA78},1);
        component(identity,{0x80C70CAEU,0x808090E8U,0xE8},2);
        component(publisher,{0x80F65036U,0x80803902U,0x4E8},3);
        component(sound,{0x80F64F8CU,0x808084E9U,0x540},4);
        component(bank,{0x80F6503FU,0x808084E9U,0x16D8},5);
        put(device+0x70,UINT32_MAX);put(device+0x74,std::uint8_t{1});
        put(device+0x960,std::int32_t{-1});put(device+0x964,std::int32_t{-1});
        put(row+0xC,entity.handle);put(row+0x4C,std::uint32_t{7});
        put(identity+0x40,sc::kPlacement);
        put(device+0x3B8,std::uint32_t{4});put(publisher+0x68,std::uint32_t{5});
        put(publisher+0xD8,std::uint32_t{1});
        put(bundle+4,std::uint32_t{8});put(bundle+0x18,UINT32_MAX);
        put(metadata+0x68,std::uint64_t{2});put(metadata+0x70,std::int64_t{0x80});
        put(metadata+0x100+0x14,std::int32_t{0x100});
        put(metadata+0x118+0x14,std::int32_t{0x200});
        // Native metadata aliases refer to the actual separately-resolved components.
        std::memcpy(memory.data()+bundle+0x100,memory.data()+identity,0x80);
        std::memcpy(memory.data()+bundle+0x200,memory.data()+publisher,0x100);
        handles[2]=bundle+0x100;handles[3]=bundle+0x200;
    }
    bool captured(){sc::Binding binding{};return sc::capture(*this,device,binding);}
};
inline void verify(){
    Fixture f;CHECK(f.captured());CHECK(sc::pending(f,f.device));
    sc::Binding captured{};CHECK(sc::capture(f,f.device,captured));
    CHECK(captured.entity==f.entity && captured.device==f.weakDevice && captured.address==f.device);
    const auto rejected=[&](auto change){Fixture changed;change(changed);CHECK(!changed.captured());};
    rejected([](auto& v){v.put(v.device,0x80C7069BU);}); // Previous wrong target.
    rejected([](auto& v){v.put(v.device+4,0x80803911U);});
    rejected([](auto& v){v.put(v.device+8,std::int64_t{0xA79});});
    rejected([](auto& v){v.put(v.device+0x70,std::uint32_t{0});});
    rejected([](auto& v){v.put(v.device+0x74,std::uint8_t{0});});
    rejected([](auto& v){v.put(v.row+4,std::uint32_t{4});});
    rejected([](auto& v){v.put(v.row+0xC,std::uint32_t{8});});
    rejected([](auto& v){v.put(v.bundle+0x140,sc::kPlacement+1);});
    rejected([](auto& v){v.put(v.device+0x3B8,UINT32_MAX);});
    rejected([](auto& v){v.put(v.device+0x3C0,std::int64_t{1});});
    rejected([](auto& v){v.put(v.sound+0x2C,std::uint32_t{8});});
    rejected([](auto& v){v.put(v.bundle+0x2D8,std::uint32_t{2});});
    rejected([](auto& v){v.put(v.bank,0x80F65040U);});
    rejected([](auto& v){v.handles[1]=0x1100;});
    rejected([](auto& v){v.stale=true;});
    for(const auto offset:{0x960U,0x964U}) {Fixture changed;changed.put(changed.device+offset,std::int32_t{2});CHECK(!sc::pending(changed,changed.device));}
    for(const auto offset:{0x370U,0x37CU}) {Fixture changed;changed.put(changed.device+offset,0.1F);CHECK(!sc::pending(changed,changed.device));}
    f.put(f.device+0x960,std::int32_t{2});f.put(f.device+0x37C,1.F);
    for(unsigned i=0;i<1000;++i) {CHECK(!sc::pending(f,f.device));}
    const auto switchFixture=[] {
        Fixture v;
        v.put(v.device,sc::kSwitchHeader);
        v.put(v.device+0x30,std::array<char,13>{'b','r','e','a','c','h','_','g','r','o','u','p',0});
        v.put(v.device+0x70,std::uint32_t{7});
        v.put(v.device+0x960,std::int32_t{2});v.put(v.device+0x964,std::int32_t{2});
        v.put(v.device+0x370,1.F);v.put(v.device+0x37C,1.F);
        return v;
    };
    auto logical=switchFixture();
    CHECK(sc::accepted_switch(logical,logical.device,2,1.F,1));
    CHECK(!sc::accepted_switch(logical,logical.device,1,1.F,1));
    CHECK(!sc::accepted_switch(logical,logical.device,2,0.F,1));
    CHECK(!sc::accepted_switch(logical,logical.device,2,1.F,0));
    CHECK(!sc::accepted_switch(logical,logical.device,0,1.F,1));
    const auto rejectsSwitch=[&](auto change){auto v=switchFixture();change(v);CHECK(!sc::accepted_switch(v,v.device,2,1.F,1));};
    rejectsSwitch([](auto& v){v.put(v.device,sc::kHeader);});
    rejectsSwitch([](auto& v){v.put(v.device+0x30,char{'x'});});
    rejectsSwitch([](auto& v){v.put(v.device+0x70,UINT32_MAX);});
    rejectsSwitch([](auto& v){v.put(v.device+0x960,std::int32_t{-1});});
    rejectsSwitch([](auto& v){v.put(v.device+0x964,std::int32_t{-1});});
    rejectsSwitch([](auto& v){v.put(v.device+0x370,0.F);});
    rejectsSwitch([](auto& v){v.put(v.device+0x37C,0.F);});
    rejectsSwitch([](auto& v){v.put(v.row+4,std::uint32_t{4});});
    rejectsSwitch([](auto& v){v.put(v.row+0xC,std::uint32_t{8});});
    rejectsSwitch([](auto& v){v.stale=true;});
    rejectsSwitch([](auto& v){v.handles[1]=0x1100;});

    // Captured failure: the weak identity is still valid after compaction, but
    // the cached address now contains 80F6503F/808084DF, not the light device.
    Fixture moved;sc::Binding remembered{},refreshed{};
    CHECK(sc::capture(moved,moved.device,remembered));
    constexpr std::uintptr_t relocated=0x6800;
    std::memcpy(moved.memory.data()+relocated,moved.memory.data()+moved.device,0x9D0);
    moved.handles[1]=relocated;
    moved.put(moved.device,gn::Ref{0x80F6503FU,0x808084DFU,0});
    moved.put(moved.device+0x960,std::int32_t{0x3F800000});
    moved.put(moved.device+0x964,std::int32_t{0});
    CHECK(moved.weak(remembered.entity) && moved.weak(remembered.device));
    CHECK(!sc::pending(moved,remembered.address)); // Old delivery rejected here.
    sc::Binding old{};CHECK(!sc::capture(moved,remembered.address,old));
    moved.forbiddenBegin=moved.device;moved.forbiddenEnd=moved.device+0x9D0;
    CHECK(sc::refresh(moved,remembered,refreshed));
    CHECK(refreshed.address==relocated && refreshed.entity==remembered.entity && refreshed.device==remembered.device);
    CHECK(sc::pending(moved,refreshed.address));CHECK(moved.forbiddenReads==0);
    CHECK(!sc::refresh(moved,{},old));
    const auto rejectRefresh=[&](auto change){auto v=moved;change(v);sc::Binding result{};CHECK(!sc::refresh(v,remembered,result));};
    rejectRefresh([](auto& v){v.stale=true;});
    rejectRefresh([](auto& v){++v.weakDevice.serial;});
    rejectRefresh([](auto& v){++v.entity.serial;});
    rejectRefresh([](auto& v){v.handles.erase(1);});
    rejectRefresh([](auto& v){v.put(relocated,gn::Ref{0x80F6503FU,0x808084DFU,0});});
    rejectRefresh([](auto& v){v.put(relocated+0x2C,std::uint32_t{8});});
    rejectRefresh([](auto& v){v.put(v.bundle+0x140,sc::kPlacement+1);});
    rejectRefresh([](auto& v){v.put(v.bundle+0x2D8,std::uint32_t{2});});
    rejectRefresh([](auto& v){v.put(v.row+4,std::uint32_t{4});});
    // Resolving an already-started scene never rewinds it or bypasses pending.
    moved.put(relocated+0x960,std::int32_t{2});moved.put(relocated+0x37C,1.F);
    for(unsigned i=0;i<1000;++i){
        CHECK(sc::refresh(moved,remembered,refreshed));CHECK(!sc::pending(moved,refreshed.address));
    }
    CHECK(moved.forbiddenReads==0);
}
}
