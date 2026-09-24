namespace ship_barrier_tests {
struct Memory {
    static constexpr std::uintptr_t base=0x100000;
    std::array<std::byte,0x3000> bytes{};
    template<class T> bool value(std::uintptr_t address,T& out) const noexcept {
        if(address<base || address-base>bytes.size()-sizeof(T)) return false;
        std::memcpy(&out,bytes.data()+(address-base),sizeof(T));return true;
    }
    template<class T> void put(std::size_t offset,const T& value) noexcept {std::memcpy(bytes.data()+offset,&value,sizeof(T));}
    void header(std::size_t offset,std::uint32_t tag,std::uint32_t kind,std::uint32_t definition) {
        put(offset,std::array<std::uint32_t,4>{tag,kind,definition,0});
    }
};
}
static void ship_barriers() {
    namespace sb=m::ship_barrier;using Memory=ship_barrier_tests::Memory;
    const coo::Generation owner{11,2};auto frame=std::make_unique<m::Frame>();
    frame->enabled=true;frame->spawnGeneration=4;frame->section=static_cast<std::uint8_t>(m::Section::ship);
    for(const auto& placement:sb::kPlacements) {
        auto& s=frame->native[m::asset_index(m::asset(m::kShip,23,placement.slot))];
        s.managed=s.active=s.acknowledged=s.poseKnown=s.deviceSynchronized=true;
        s.generation=5;s.position=s.observedPosition=1.F;s.observedRevision=5;
    }
    check(!sb::wanted(owner,*frame).enabled(),"pod barriers cannot release before the scan");
    frame->consoleScanned=true;const auto wanted=sb::wanted(owner,*frame);
    check(wanted.enabled() && wanted.revisions==std::array<std::uint32_t,2>{5,5},"accepted scan releases both pod barriers independently");
    auto& first=frame->native[m::asset_index(m::asset(m::kShip,23,56))];
    first.observedRevision=4;
    check(sb::wanted(owner,*frame).revisions==std::array<std::uint32_t,2>{0,5},"stale first door cannot suppress the ready second door");
    first.observedRevision=5;first.poseKnown=false;
    check(sb::wanted(owner,*frame).revisions[0]==0,"published open command alone is insufficient");
    first.poseKnown=true;first.observedPosition=0.F;
    check(sb::wanted(owner,*frame).revisions[0]==0,"native pose must reach open");
    first.observedPosition=1.F;first.retired=true;
    check(sb::wanted(owner,*frame).revisions[0]==0,"retired doors are ignored");
    first.retired=false;first.acknowledged=false;
    check(sb::wanted(owner,*frame).revisions[0]==0,"unacknowledged commands are ignored");
    first.acknowledged=true;frame->fault=true;
    check(!sb::wanted(owner,*frame).enabled(),"faulted mission cannot mutate pod barriers");
    frame->fault=false;frame->finished=true;
    check(!sb::wanted(owner,*frame).enabled(),"finished mission releases no pod barrier");
    frame->finished=false;frame->section=static_cast<std::uint8_t>(m::Section::boulevard);
    check(!sb::wanted(owner,*frame).enabled(),"earlier checkpoints cannot open the ship fields");
    frame->section=static_cast<std::uint8_t>(m::Section::generator);
    check(sb::wanted(owner,*frame).enabled(),"open barriers stay open through the later ship checkpoints");
    ++frame->spawnGeneration;
    check(sb::wanted(owner,*frame)!=wanted,"checkpoint reincarnation invalidates cached identities");
    check(!sb::wanted({},*frame).enabled(),"absent owner cannot release a barrier");
    for(std::size_t i=0;i<2;++i) {
        auto row=m::door_native::Row{0,0x2345,9,sb::kList,sb::kPlacements[i].record,sb::kPlacements[i].authored};
        check(sb::placement(row,i) && !sb::placement(row,1-i),"pod barrier GUID and record are exact and distinct");
        const sb::Request awaiting{owner,4,{}};
        check(!awaiting.enabled() && sb::retain_tick(awaiting,row,true),
            "exact post-scan barrier keeps ticking before its open receipt arrives");
        check(!sb::retain_tick({},row,true) && !sb::retain_tick(awaiting,row,false),
            "mission departure and absent local ownership never retain callbacks");
        ++row.authored;check(!sb::placement(row,i),"neighbor or recycled placement is not accepted");
        check(!sb::retain_tick(awaiting,row,true),"unrelated devices retain their original scheduler lifetime");
    }
    for(unsigned native=0;native<256;++native) {
        const auto value=static_cast<std::uint8_t>(native);
        check(sb::tick_result(value,false)==value,"completed/unrelated callbacks preserve every native return value");
        check(sb::tick_result(value,true)==(value?value:1),"pending barrier extends only an idle native callback");
    }
    // Reproduce both orderings: device motion ends at 1.1 seconds, while the
    // fade ends at 3.5 seconds and receipts may arrive before or after it.
    for(const unsigned receiptAt:{12U,40U}) {
        for(const auto& placement:sb::kPlacements) {
            const m::door_native::Row row{0,0x2345,9,sb::kList,placement.record,placement.authored};
            bool scheduled=true,released=false;unsigned releases{},lastTick{};
            for(unsigned tick=0;scheduled && tick<60;++tick) {
                sb::Request request{owner,4,{}};
                if(tick>=receiptAt) request.revisions[placement.slot-56]=5;
                bool pending=sb::retain_tick(request,row,true);
                if(request.enabled() && tick>=35) {
                    std::int32_t state=1;
                    released=sb::release([] {return true;},[] {return true;},[&](std::int32_t& value) {
                        value=state;return true;
                    },[] {return true;},[&] {state=0;return true;},[&] {++releases;})==sb::Release::opened;
                    pending=!released;
                }
                scheduled=sb::tick_result(tick<=11?1:0,pending)!=0;lastTick=tick;
            }
            check(released && releases==1 && lastTick==(receiptAt>35?receiptAt:35),
                "callback survives settled device until fade plus receipt, releases once, then retires");
        }
    }
    Memory memory;const auto graph=Memory::base,provider=Memory::base+0x2000;
    memory.header(0,sb::kGraph,0x808084E9,0xD20);
    memory.put(0xB0,std::uint64_t{7});memory.put(0xB8,std::int64_t{0x628});
    memory.put(0xC0,std::uint64_t{1});memory.put(0xC8,std::int64_t{0x9F8});memory.put(0x800,std::int64_t{0x200});
    memory.header(0xA00,sb::kGraph,0x808093C4,0x1788);memory.header(0xAD0,sb::kGraph,0x808084DF,0x1900);
    check(!sb::open_curve_finished(memory,graph),"an unstarted opening curve is not a completed opening");
    memory.put(0xA24,3.5F);
    check(sb::open_curve_finished(memory,graph),"completed native zero curve can hand off to retained presentation");
    memory.put(0xA28,std::uint64_t{1});
    check(!sb::open_curve_finished(memory,graph),"running opening is allowed to finish naturally");
    memory.put(0xA28,std::uint64_t{0});memory.put(0xA30,std::uint8_t{1});
    check(!sb::open_curve_finished(memory,graph),"active action phase prevents the fallback");
    memory.put(0xA30,std::uint8_t{0});memory.put(0xB00,std::int32_t{1});
    check(!sb::open_curve_finished(memory,graph),"an active writer retains ownership");
    memory.put(0xB00,std::int32_t{0});memory.put(0xAF0,std::array<float,4>{1,1,1,1});
    check(!sb::open_curve_finished(memory,graph),"closed curve sample is never retained as open");
    memory.put(0xAF0,std::array<float,4>{});memory.put(0x800,std::int64_t{0x100});
    check(!sb::open_curve_finished(memory,graph),"unexpected native action layout fails closed");
    check(!sb::open_curve_finished(memory,0) && !sb::open_curve_finished(memory,UINTPTR_MAX),"invalid graph pointers fail closed");
    memory.header(0x2000,sb::kProvider,0x80808870,0x128);
    memory.put(0x2030,std::uint64_t{1});memory.put(0x2038,std::int64_t{0x18});
    memory.header(0x2060,sb::kProvider,0x80808868,0x1D0);memory.put(0x2070,std::int64_t{-0x70});
    memory.put(0x2080,std::int32_t{1});std::int32_t index{};
    check(sb::state_index(memory,provider,index) && index==1,"collision fallback is read from the real state provider");
    memory.put(0x2080,std::int32_t{0});
    check(sb::state_index(memory,provider,index) && index==0,"native open state has a separate readback");
    memory.put(0x2080,std::int32_t{4});
    check(!sb::state_index(memory,provider,index),"unknown named state is rejected");
    memory.put(0x2080,std::int32_t{1});memory.header(0x2000,0x80F27699,0x80808A0C,0x378);
    check(!sb::state_index(memory,provider,index),"physics component must never be passed to named-state setter");

    constexpr std::uint32_t net=0x1234,bundle=0x4567,entity=0x6789;
    const auto record=Memory::base+0x1000;
    memory.put(0x10C0,net);memory.put(0x10C8,bundle);memory.put(0x1120,entity);
    memory.put(0x1124,std::uint16_t{8});std::uint16_t flags{};
    check(sb::network_record(memory,record,net,bundle,entity,flags) && flags==8,
        "attached network record is validated separately from entity authority");
    for(const auto bad:{0U,1U,4U,10U,0x88U,0x108U,0x208U,0x408U,0xFFFFU}) {
        memory.put(0x1124,static_cast<std::uint16_t>(bad));
        check(!sb::network_record(memory,record,net,bundle,entity,flags),"unrecognized or transferring authority records fail closed");
    }
    memory.put(0x1124,std::uint16_t{9});
    check(sb::network_record(memory,record,net,bundle,entity,flags) && flags==9,"already-local record is recognized without a new grant");
    check(!sb::network_record(memory,record,net+1,bundle,entity,flags)
        && !sb::network_record(memory,record,net,bundle+1,entity,flags)
        && !sb::network_record(memory,record,net,bundle,entity+1,flags),"recycled handle, other bundle and other entity are rejected");
    check(!sb::network_record(memory,0,net,bundle,entity,flags)
        && !sb::network_record(memory,UINTPTR_MAX,net,bundle,entity,flags)
        && !sb::network_record(memory,record,UINT32_MAX,bundle,entity,flags),"invalid network identity is rejected");

    // The regression: the entity was local, but its network permission was 8
    // and the real named-state setter rejected it. Exercise production ordering.
    for(const auto& placement:sb::kPlacements) {
        (void)placement;bool current=true,grantWorks=true,setWorks=true,readbackWorks=true;
        unsigned grants{},sets{},publishes{};std::int32_t nativeState=1;std::uint16_t permission=8;
        bool contextReady=true;
        const auto run=[&] {return sb::release([&] {return contextReady;},[&] {return current;},[&](std::int32_t& value) {
            value=nativeState;return true;
        },[&] {++grants;if(grantWorks) permission=9;return permission==9;},[&] {
            ++sets;check(permission==9,"named-state setter follows bundle authority grant");
            if(setWorks && readbackWorks) nativeState=0;return setWorks;
        },[&] {++publishes;check(nativeState==0,"presentation follows verified native state");});};
        contextReady=false;
        check(run()==sb::Release::contextUnavailable && !grants && !sets && !publishes,
            "camera callback without allocator context cannot enter native mutation");
        contextReady=true;grantWorks=false;
        check(run()==sb::Release::authorityRejected && !sets && !publishes,"denied grant cannot falsely announce an open barrier");
        grantWorks=true;setWorks=false;
        check(run()==sb::Release::stateRejected && !publishes,"setter rejection cannot clear presentation or report success");
        setWorks=true;readbackWorks=false;
        check(run()==sb::Release::stateRejected && !publishes,"setter acknowledgement alone is insufficient");
        readbackWorks=true;
        check(run()==sb::Release::opened && permission==9 && nativeState==0 && publishes==1,"both barriers acquire ownership then open");
        const auto previousGrants=grants,previousSets=sets;
        check(run()==sb::Release::opened && grants==previousGrants && sets==previousSets && publishes==2,
            "retaining an open barrier does not repeatedly grant authority or set state");
        current=false;
        check(run()==sb::Release::stale && grants==previousGrants && publishes==2,"mission departure cannot mutate retired barriers");
        current=true;nativeState=2;
        check(run()==sb::Release::waiting && grants==previousGrants && publishes==2,"unrelated authored states remain untouched");
    }
    unsigned calls{},writes{};std::int32_t closed=1;
    check(sb::release([] {return true;},[&] {return ++calls==1;},[&](std::int32_t& value) {value=closed;return true;},[] {return true;},
        [&] {++writes;return true;},[&] {++writes;})==sb::Release::stale && !writes,
        "authority handoff rechecks run identity before state mutation");
    calls=0;
    check(sb::release([&] {return ++calls==1;},[] {return true;},[&](std::int32_t& value) {value=closed;return true;},[] {return true;},
        [&] {++writes;return true;},[&] {++writes;})==sb::Release::contextUnavailable && !writes,
        "allocator context must still be present after the ownership call");
    calls=0;closed=0;
    check(sb::release([&] {return ++calls==1;},[] {return true;},[&](std::int32_t& value) {value=closed;return true;},[] {return true;},
        [&] {++writes;return true;},[&] {++writes;})==sb::Release::contextUnavailable && !writes,
        "presentation writes also require the live allocator context");

    Memory tlsMemory;const auto tls=Memory::base;
    const auto executable=[](std::uintptr_t p) {return p==0x123456;};
    check(!sb::allocator_ready(tlsMemory,0,executable)
        && !sb::allocator_ready(tlsMemory,UINTPTR_MAX,executable)
        && !sb::allocator_ready(tlsMemory,tls,executable),"missing native TLS/services defer release");
    for(const auto offset:{0x50U,0x58U}) {
        const std::size_t serviceOffset=offset*8,vtableOffset=serviceOffset+0x1000;
        tlsMemory.put(offset,Memory::base+serviceOffset);
        tlsMemory.put(serviceOffset,Memory::base+vtableOffset);
        for(const auto method:{0x08U,0x10U,0x18U,0x20U}) tlsMemory.put(vtableOffset+method,std::uintptr_t{0x123456});
    }
    check(sb::allocator_ready(tlsMemory,tls,executable),"device callback with both native allocator services is allowed");
    for(const auto offset:{0x50U,0x58U}) {
        const auto service=Memory::base+offset*8;
        tlsMemory.put(offset,std::uintptr_t{});
        check(!sb::allocator_ready(tlsMemory,tls,executable),"null allocation/free service cannot reach native vtable dispatch");
        tlsMemory.put(offset,service);
        for(const auto method:{0x08U,0x10U,0x18U,0x20U}) {
            tlsMemory.put(offset*8+0x1000+method,std::uintptr_t{0x543210});
            check(!sb::allocator_ready(tlsMemory,tls,executable),"unexecutable allocator methods fail closed");
            tlsMemory.put(offset*8+0x1000+method,std::uintptr_t{0x123456});
        }
    }
}
