// Operation-count regressions, not timing thresholds: deterministic even on CI.
static void performance_regressions() {
    using dawn::client::hooks::bootflow::GateTraceCache;
    using dawn::client::hooks::bootflow::GateTraceSample;
    GateTraceCache<> cache;
    const GateTraceSample bound{0x10000,1,7,{0x33F800000ULL,0x33F800000ULL,0x300000000ULL,42}};
    const GateTraceSample unbound{0x20000,2,8,{0,0,0,UINT64_MAX}};
    check(cache.report(bound,0) && cache.report(unbound,0),"first committed state is logged for each gate");
    unsigned repeatedLogs{};
    for(unsigned update=1;update<80;++update) {
        repeatedLogs+=cache.report(bound,update*125U)?1U:0U;
        repeatedLogs+=cache.report(unbound,update*125U)?1U:0U;
    }
    check(repeatedLogs==0,"alternating unchanged bound/unbound gates cause no synchronous log writes");
    check(cache.report(bound,10000) && cache.report(unbound,10000),"each gate retains its bounded heartbeat");
    auto changed=bound;changed.channelsAndDevice[2]^=1;
    check(cache.report(changed,10001),"third-channel changes are not lost by a two-channel hash");
    changed.channelsAndDevice[3]^=1;
    check(cache.report(changed,10002),"a new bound device is reported immediately");
    ++changed.self;check(cache.report(changed,10003),"a recycled component with a new self handle is reported");
    ++changed.definition;check(cache.report(changed,10004),"a changed component definition is reported");
    check(!cache.report({},10005),"null components never occupy the diagnostic cache");
    cache.clear();check(cache.report(changed,10005),"reset discards diagnostic history");
    GateTraceCache<2> small;
    check(small.report(bound,1) && small.report(unbound,2),"bounded cache fills");
    auto third=bound;third.component=0x30000;
    check(small.report(third,3) && !small.report(unbound,4),"capacity eviction preserves the more recent gate");
    check(small.report(bound,5),"evicted entries are observable again");
    check(small.report(bound,1),"clock rollback cannot suppress a diagnostic forever");

    namespace roster=dawn::server::bap::encrypted::push::activity::homecoming_roster;
    namespace layouts=dawn::state::build_data::scenarios;
    const m::Group* root{};for(const auto& g:m::kGroups) if(g.key==m::kRoot) root=&g;
    check(root!=nullptr,"Homecoming has an exact root binding");
    auto row=std::make_unique<layouts::RosterGroup>();row->registryKey=root->key;row->objectTag=root->tag;
    row->slotCount=static_cast<std::uint16_t>(root->slots.size());
    for(std::size_t i=0;i<root->slots.size();++i) {
        const auto& s=root->slots[i];row->slotTypes[i]=static_cast<std::uint8_t>(s.asset.type);
        row->slotFlags[i]=m::slot_flags(s);row->slotIndices[i]=s.asset.slot;
        row->descriptorTags[i]=s.asset.definition;row->descriptorOffsets[i]=s.offset;
        row->componentClasses[i]=s.component;row->senseSchemas[i]=s.sense;row->authSchemas[i]=s.authority;
    }
    layouts::Definition layout{};layout.tag=m::kScenario;layout.nameLength=static_cast<std::uint8_t>(roster::kPackage.size());
    std::copy(roster::kPackage.begin(),roster::kPackage.end(),layout.name.begin());layout.bubbleCount=roster::kBubbleCount;
    unsigned queries{},copies{};std::uint16_t currentIndex=3010;
    const auto index=[&](std::uint32_t key,std::uint32_t tag,std::uint16_t& out) {
        ++queries;check(key==root->key && tag==root->tag,"root lookup uses both key and object tag");out=currentIndex;return true;
    };
    const auto find=[&](std::size_t i,layouts::RosterGroup& out) {
        ++copies;if(i!=currentIndex) return false;out=*row;return true;
    };
    for(unsigned repeat=0;repeat<100;++repeat) {
        queries=copies=0;currentIndex=static_cast<std::uint16_t>(3010-repeat);
        check(roster::prepare_layout(layout,index,find),"root resolution survives catalog reordering");
        check(queries==1 && copies==1,"periodic layout publication copies only one matching descriptor record");
        for(unsigned b=0;b<roster::kBubbleCount;++b)
            check(layout.authoredGroupCounts[b]==1 && layout.authoredGroups[b][0]==currentIndex,"stable bubble keys use the newly verified ordinal");
    }
    const auto before=layout;
    row->authSchemas[0]^=1;
    check(!roster::prepare_layout(layout,index,find),"descriptor schema mismatch still rejects publication");
    check(layout.authoredGroups==before.authoredGroups,"failed validation leaves layout unchanged");
    row->authSchemas[0]^=1;copies=0;
    check(!roster::prepare_layout(layout,[](auto,auto,std::uint16_t&){return false;},find) && copies==0,
        "missing or ambiguous identity does not fall back to a broad scan");
    copies=0;currentIndex=layouts::kRosterGroupCapacity;
    check(!roster::prepare_layout(layout,index,find) && copies==0,"out-of-range ordinal is rejected before copy");

    m::SceneState done{};done.started=done.performanceFinished=done.entryCue=done.combatHeld=true;
    done.speech.set();done.combatReleased=done.damageReleased=UINT16_MAX;
    for(const auto& scene:m::kScenes) {
        if(!m::playback_scene(scene.asset.definition)) continue;
        const m::PlaybackRequest request{{77,3},scene.asset,3};
        check(!m::playback_pending(request,done),"fully consumed scene needs no further selector reads");
        auto pending=done;pending.started=false;
        check(m::playback_pending(request,pending),"every new generation must prove selector start");
        if(m::performance_for(request)) {
            pending=done;pending.performanceFinished=false;
            check(m::playback_pending(request,pending),"speech alone cannot retire a performance-completion watch");
        }
        if(scene.asset==m::kShaxxDoorOpening.scene || scene.asset==m::asset(m::kBoulevard,43,16)) {
            pending=done;pending.entryCue=false;
            check(m::playback_pending(request,pending),"door and ship entry cues remain watched after dialogue");
        }
        if(m::combat_hold_for(scene.asset)) {
            pending=done;pending.combatHeld=false;
            check(m::playback_pending(request,pending),"Cabal hold handoff cannot be skipped");
        }
        for(const auto& source:m::kSceneActionSources) if(source.definition==scene.asset.definition) {
            const auto* plan=m::scene_action_plan(source.graph,source.root);check(plan!=nullptr,"every generated source maps to its exact action plan");
            for(const auto& action:plan->actions) {
                pending=done;
                if(action.kind==m::SceneActionKind::speech) pending.speech.reset(action.value);
                else if(action.kind==m::SceneActionKind::combat) pending.combatReleased&=static_cast<std::uint16_t>(~(1U<<action.value));
                else pending.damageReleased&=static_cast<std::uint16_t>(~(1U<<action.value));
                check(m::playback_pending(request,pending),"each unconsumed speech or immunity/combat release keeps the watch alive");
            }
        }
        check(m::playback_pending(request,m::SceneState{}),"restarting a scene re-arms every receipt watch");
    }
    for(const auto definition:m::kSceneActionDefinitions) {
        bool found{};for(const auto& source:m::kSceneActionSources) found|=source.definition==definition;
        check(found,"no speech or flag descriptor is omitted from source-to-plan inventory");
    }
    const m::PlaybackRequest revival{{77,3},m::kZavalaRevival.scene,4,true};
    auto pending=done;pending.performanceFinished=false;
    check(m::playback_pending(revival,pending),"Zavala's later revival still waits for its own performance");
    check(!m::playback_pending(m::PlaybackRequest{},pending),"no observation for an invalid owner");
}
