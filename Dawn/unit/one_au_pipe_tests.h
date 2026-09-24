#pragma once
inline void pipe_encounter() {
    auto controller=std::make_unique<m::Controller>();
    check(controller->select(71,0),"pipe fixture selects mission");
    const auto owner=controller->owner();const auto movie=m::cinematics::kMovies[0];
    check(controller->arrival(owner,1,10) && controller->cinematic(owner,{5239,movie.registry,1,6,0},20)
        && controller->cinematic(owner,{1685,movie.registry,2,6,0},30) && controller->arrival(owner,4,40),"pipe fixture completes opening");
    std::uint64_t now=100;
    const auto visit=[&](coo::Asset area) {
        bool found{};
        for(const auto& v:m::kVolumes) if(v.asset==area) {
            m::Point point{};for(const auto& p:v.vertices) {point.x+=p.x;point.y+=p.y;}
            point.x/=static_cast<float>(v.vertices.size());point.y/=static_cast<float>(v.vertices.size());point.z=(v.min.z+v.max.z)*.5F;
            check(m::contains(v,point),"fixture point lies inside authored trigger");
            controller->position(owner.run,point);found=true;break;
        }
        check(found && controller->advance(owner.run,now+=100,true),"actual position and graph update activate encounter");
    };
    visit(m::volume(m::kLanding,86));visit(m::volume(m::kLanding,85));visit(m::volume(m::kBridge,203));
    const auto omitted=m::asset(m::kBridge,1,58),retained=m::asset(m::kBridge,1,59);
    const auto& frame=controller->frame();const auto& pipe=frame.native[m::asset_index(retained)];
    check(!frame.fault && !frame.native[m::asset_index(omitted)].active && pipe.active,"actual pipe spawn omits 58 and retains 59");
    const coo::CommandSpec clear{coo::Operation::observation,m::kModule,0x100U+static_cast<std::uint32_t>(m::Cohort::pipes),coo::Wait::observed};
    check(controller->missing(clear).asset==retained,"pipe clearance waits only on retained source");
    const auto& spawn=m::kSpawns[m::spawn_index(retained)];
    for(unsigned i=0;i<spawn.count;++i) {
        const m::EnemyReceipt receipt{owner.run,0x20002000U+i,0x40002000U,pipe.generation,59,m::kBridge};
        check(controller->admitted(receipt) && controller->died(receipt),"retained pipe enemies complete through real admission and death ledger");
    }
    check(controller->missing(clear).missing==coo::Missing::none,"omitted enemy cannot become an unmet kill requirement");
    check(!controller->admitted({owner.run,0x20003000U,0x40002000U,pipe.generation,58,m::kBridge}),"omitted pipe actor cannot enter live encounter ledger");
    constexpr std::array<std::uint16_t,6> mercury{40,51,52,53,54,55};
    check(m::kMercury.size()==mercury.size(),"Mercury cohort size preserved");
    for(std::size_t i=0;i<mercury.size();++i) check(m::kMercury[i].registry==m::kBridge && m::kMercury[i].slot==mercury[i]
        && frame.native[m::asset_index(m::asset(m::kBridge,1,mercury[i]))].active,"all adjacent Mercury sources, including 53, remain active");
}
