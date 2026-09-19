#pragma once
#include "state/activity/vanilla/one_au/entrance_native.h"
#include <vector>
#include <functional>

namespace entrance_test {
namespace native=m::entrance_native;
struct World {
    std::vector<native::Row> rows=std::vector<native::Row>(native::kRows);
    std::array<bool,native::kRows> owners{};
    std::vector<std::uint32_t> grants,retirements;
    bool valid{true};
    std::function<void(World&,std::uint32_t)> beforeOwner;
    bool stable() const { return valid; }
    bool row(std::size_t slot,native::Row& out) {out=rows[slot];return true;}
    bool owned(std::uint32_t entity,bool& out) {
        if(beforeOwner) beforeOwner(*this,entity);
        out=owners[entity&0x1FFFU];return true;
    }
    void grant(std::uint32_t entity) {grants.push_back(entity);owners[entity&0x1FFFU]=true;}
    void retire(std::uint32_t entity) {retirements.push_back(entity);rows[entity&0x1FFFU].flags|=1;}
    void put(std::size_t slot,std::uint32_t table,std::uint32_t record,std::uint64_t id,bool owner=false) {
        rows[slot]={0x70020U,0x33FAA000U|static_cast<std::uint32_t>(slot),0x12345678,table,record,id};owners[slot]=owner;
    }
    void bridge(std::size_t slot,bool owner=false,std::uint32_t record=91,std::uint64_t id=0xFEDCBA9876543210ULL) {
        put(slot,0x80F0C055,record,id,owner);
    }
};
inline void run() {
    auto frame=std::make_unique<m::Frame>();frame->enabled=true;frame->spawnGeneration=257;frame->section=1;
    const coo::Generation owner{2,257};const auto wanted=m::entrance_request(owner,*frame);
    constexpr auto callback=0x12002007U;
    const auto apply=[&](World& w) {return native::repair(w,callback,wanted,[&]{return wanted;});};
    for(unsigned flags=0;flags<16;++flags) for(std::uint8_t section=0;section<4;++section) {
        frame->enabled=(flags&1)!=0;frame->finished=(flags&2)!=0;frame->restricted=(flags&4)!=0;frame->fault=(flags&8)!=0;frame->section=section;
        const auto request=m::entrance_request(owner,*frame);
        const bool enabled=(flags&1) && !(flags&10) && section<=2;
        check(request.enabled()==enabled,"entrance gating ignores Darkness Zone but rejects disabled, finished, faulted and later sections");
        auto world=std::make_unique<World>();world->bridge(1);world->bridge(8191,true);
        const auto result=native::repair(*world,callback,request,[&]{return request;});
        check((result.retired!=UINT32_MAX)==enabled,"actual repairs obey all frame gating combinations");
    }
    frame->enabled=true;frame->finished=frame->fault=false;frame->section=1;
    check(!m::entrance_request({},*frame).enabled(),"unselected mission cannot repair entrance");
    frame->recovery.phase=m::recovery::Phase::requested;
    check(!m::entrance_request(owner,*frame).enabled(),"checkpoint transition cannot repair stale objects");
    auto doors=std::make_unique<World>();
    for(std::size_t i=0;i<native::kRows;++i) doors->put(i,0x80C32CEE,static_cast<std::uint32_t>(i%2),i%2?0xAB48F4FA0B44B151ULL:0x30DB525724EDDB9FULL);
    const auto rows=doors->rows;
    check(apply(*doors).doors==native::kRows && doors->grants.size()==native::kRows,"both doors receive authority at every native row index, including 8191");
    check(doors->rows==rows && doors->retirements.empty(),"door repairs only grant authority and preserve native state");
    check(apply(*doors).doors==0,"owned doors need no repeated writes");
    auto landing=wanted;landing.section=0;
    auto w=std::make_unique<World>();w->put(0,0x80C32CEE,0,0x30DB525724EDDB9FULL);
    check(native::repair(*w,callback,landing,[&]{return landing;}).doors==0,"doors stay untouched during landing");
    for(const auto record:{91U,92U,93U,102U,108U,124U}) for(bool reverse:{false,true}) {
        w=std::make_unique<World>();w->bridge(0,reverse,record);w->bridge(8191,!reverse,record);
        const auto target=w->rows[reverse?8191:0].entity;
        check(apply(*w).retired==target && w->retirements.size()==1,"all six bridge placements retire only their unowned duplicate in either order");
    }
    const auto rejects=[&](auto change,const char* why) {
        auto world=std::make_unique<World>();world->bridge(0);world->bridge(8191,true);change(*world);
        check(apply(*world).retired==UINT32_MAX && world->retirements.empty() && world->grants.empty(),why);
    };
    rejects([](auto& v){v.owners[8191]=false;},"no owned original means no retirement");
    rejects([](auto& v){v.owners[0]=true;},"owned copies are never retired");
    rejects([](auto& v){v.rows[8191].authored^=1ULL<<40;},"full 64-bit authored identity must match");
    rejects([](auto& v){v.rows[8191].record=92;},"different bridge records are distinct objects");
    rejects([](auto& v){v.rows[8191].table++;},"different placement tables cannot authorize cleanup");
    for(const auto slot:{0U,8191U}) {
        rejects([&](auto& v){v.rows[slot].entity++;},"stale salted row handle rejected");
        rejects([&](auto& v){v.rows[slot].entity=UINT32_MAX;},"absent entity rejected");
        for(const auto flag:{1U,4U}) rejects([&](auto& v){v.rows[slot].flags|=flag;},"retiring and uninitialized rows are skipped");
    }
    rejects([](auto& v){v.rows[0].bundle=UINT32_MAX;},"invalid retiring candidate bundle never reaches native retirement (5979C4 regression)");
    w=std::make_unique<World>();w->bridge(0);w->bridge(8191,true);w->rows[8191].bundle=UINT32_MAX;
    check(apply(*w).retired==w->rows[0].entity,"owned placement can prove duplication before its component bundle is ready");
    w=std::make_unique<World>();w->bridge(0);w->bridge(8191,true);
    check(native::repair(*w,UINT32_MAX,wanted,[&]{return wanted;}).retired==w->rows[0].entity,
        "callback with no entity can retire a distinct valid bridge duplicate");
    w=std::make_unique<World>();w->put(0,0x80C32CEE,0,0x30DB525724EDDB9FULL);w->rows[0].bundle=UINT32_MAX;
    check(native::repair(*w,UINT32_MAX,wanted,[&]{return wanted;}).doors==1,
        "door authority does not dereference a component bundle or require a callback owner");
    rejects([](auto& v){v.rows[0].record=v.rows[8191].record=94;},"adjacent bridge records remain unchanged");
    rejects([](auto& v){v.valid=false;},"replaced native table cancels mutation");
    rejects([](auto& v){v.beforeOwner=[reads=0](auto& a,auto entity) mutable {
        if((entity&0x1FFFU)==0 && ++reads==2) a.owners[0]=true;
    };},"ownership acquired before retirement cancels cleanup");
    rejects([](auto& v){v.beforeOwner=[reads=0](auto& a,auto entity) mutable {
        if((entity&0x1FFFU)==8191 && ++reads==1) a.rows[0].bundle=UINT32_MAX;
    };},"bundle invalidated after initial scan is rechecked");
    rejects([](auto& v){v.beforeOwner=[reads=0](auto& a,auto entity) mutable {
        if((entity&0x1FFFU)==8191 && ++reads==2) a.owners[8191]=false;
    };},"owned original losing authority cancels cleanup");
    for(const auto changed:{m::EntranceRequest{},m::EntranceRequest{{3,257},257,1},m::EntranceRequest{{2,258},258,1},m::EntranceRequest{{2,257},257,2}}) {
        w=std::make_unique<World>();w->bridge(0);w->bridge(8191,true);w->put(1,0x80C32CEE,0,0x30DB525724EDDB9FULL);
        const auto result=native::repair(*w,callback,wanted,[&]{return changed;});
        check(result.retired==UINT32_MAX && result.doors==0,"run, generation and section changes cancel all writes");
    }
    w=std::make_unique<World>();w->bridge(0);w->bridge(1);w->bridge(2);w->bridge(8191,true);w->rows[0].bundle=UINT32_MAX;
    check(native::repair(*w,w->rows[1].entity,wanted,[&]{return wanted;}).retired==w->rows[2].entity,
        "scan continues after incomplete and callback-self candidates");
    check(w->retirements.size()==1 && !(w->rows[0].flags&1) && !(w->rows[1].flags&1),"at most one safe duplicate retires per callback");
    for(const auto bad:{native::Row{0,0x2000,1,0x80C32CEE,0,0xAB48F4FA0B44B151ULL},
            native::Row{0,0x2000,1,0x80C32CEE,2,0x30DB525724EDDB9FULL},native::Row{0,0x2000,1,0x80C32CEF,0,0x30DB525724EDDB9FULL}}) {
        w=std::make_unique<World>();w->rows[0]=bad;check(apply(*w).doors==0,"door identity requires exact table, record and authored ID together");
    }
}
}
