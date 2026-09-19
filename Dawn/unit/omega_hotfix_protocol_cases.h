#pragma once
#include <memory>

void omega_hotfix_protocol_checks() {
    const auto check=[](bool ok){if(!ok){std::fputs("Omega hotfix HUD regression failed\n",stderr);std::abort();}};
    for(bool omega:{false,true})for(bool flag:{false,true})for(std::int32_t state:{-1,0,1,100}) {
        auto s=std::make_unique<wire::Snapshot>();
        s->archiveOmega=omega;s->initializeMissionAuthorityRuntime=true;
        s->activityScriptFlag=flag;s->activityScriptState=state;
        for(bool legacy:{false,true}) {
            std::array<std::byte,64> actual{},expected{};
            bits::Writer w(actual),e(expected);
            check(legacy?wire::legacy_write_auth_body(w,*s,0x4786C0E0U,18,0,false)
                :wire::write_auth_body(w,*s,0x4786C0E0U,18,0,false));
            check(e.write(omega?0U:1U,1));
            for(unsigned i=0;i<5;++i)check(e.write(omega?(i==1?0x134F00C00000ULL:i==4?UINT64_MAX:0ULL):0ULL,64));
            check(e.write(omega?0x3F800000U:0U,32));
            check(e.write(!omega && flag?1U:0U,1));
            check(e.write(0x80000000U+static_cast<std::uint32_t>(state),32));
            check(w.bit_count()==386 && e.bit_count()==386);check(actual==expected);
            std::array<std::byte,48> shortBuffer{};bits::Writer shortWriter(shortBuffer);
            check(!(legacy?wire::legacy_write_auth_body(shortWriter,*s,0x4786C0E0U,18,0,false)
                :wire::write_auth_body(shortWriter,*s,0x4786C0E0U,18,0,false)));
        }
    }
    std::puts("Omega hotfix: HUD ring removed; script state, other missions and truncated-buffer guards preserved");
}
