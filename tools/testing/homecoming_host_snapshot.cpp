#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <vector>
#include "../../Dawn/src/state/activity/vanilla/homecoming/ship_barrier.h"

namespace m=dawn::state::activity::vanilla::homecoming;
struct Found {DWORD64 address{},module{};ULONG type{};unsigned count{};};
BOOL CALLBACK found(PSYMBOL_INFO symbol,ULONG,void* raw) {
    const std::string_view name{symbol->Name,symbol->NameLen};
    if(name.find("dawn::state::activity::vanilla::homecoming::")!=std::string_view::npos
        && name.ends_with("::controller")) {
        auto& out=*static_cast<Found*>(raw);out={symbol->Address,symbol->ModBase,symbol->TypeIndex,out.count+1};
        std::printf("symbol=%s address=%llX type=%lu\n",symbol->Name,symbol->Address,symbol->TypeIndex);
    }
    return TRUE;
}
bool field(HANDLE process,const Found& controller,std::wstring_view target,DWORD& offset,ULONG64& length,ULONG* memberType=nullptr) {
    ULONG count{};
    if(!SymGetTypeInfo(process,controller.module,controller.type,TI_GET_CHILDRENCOUNT,&count) || count>1024) return false;
    std::vector<ULONG> storage(count+2);auto* children=reinterpret_cast<TI_FINDCHILDREN_PARAMS*>(storage.data());
    children->Count=count;children->Start=0;
    if(!SymGetTypeInfo(process,controller.module,controller.type,TI_FINDCHILDREN,children)) return false;
    for(ULONG i=0;i<count;++i) {
        WCHAR* name{};const auto child=children->ChildId[i];
        if(!SymGetTypeInfo(process,controller.module,child,TI_GET_SYMNAME,&name)) continue;
        const bool matched=std::wstring_view(name)==target;LocalFree(name);
        if(!matched) continue;
        ULONG type{};
        const bool ok=SymGetTypeInfo(process,controller.module,child,TI_GET_OFFSET,&offset)
            && SymGetTypeInfo(process,controller.module,child,TI_GET_TYPEID,&type)
            && SymGetTypeInfo(process,controller.module,type,TI_GET_LENGTH,&length);
        if(ok && memberType) *memberType=type;return ok;
    }
    return false;
}
template<class T> bool read_value(HANDLE process,std::uintptr_t address,T& value) {
    SIZE_T got{};return ReadProcessMemory(process,reinterpret_cast<void*>(address),&value,sizeof value,&got) && got==sizeof value;
}
// Explicit, attempt-locked recovery only. Bypass the incorrect observation gate;
// do not edit immutable graph definitions, enemy deaths, devices or physics.
int reactor_gate(HANDLE process,const Found& controller,DWORD pid,bool apply) {
    namespace coo=dawn::state::activity::coo;
    if(pid!=72428) return 20;
    std::array<wchar_t,32768> path{};DWORD pathSize=static_cast<DWORD>(path.size());
    if(!QueryFullProcessImageNameW(process,0,path.data(),&pathSize)
        || std::wstring_view(path.data(),pathSize)!=L"C:\\Destiny 2 Development\\destiny2.exe") return 21;
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 pe{};
    if(!read_value(process,controller.module,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE
        || !read_value(process,controller.module+dos.e_lfanew,pe) || pe.Signature!=IMAGE_NT_SIGNATURE
        || pe.FileHeader.TimeDateStamp!=1789942185U || pe.OptionalHeader.SizeOfImage!=451174400U) return 22;
    DWORD frameOffset{},executorOffset{},statesOffset{},definitionOffset{};ULONG64 length{};ULONG executorType{};
    if(!field(process,controller,L"frame_",frameOffset,length) || length!=sizeof(m::Frame)
        || !field(process,controller,L"executor_",executorOffset,length,&executorType) || length!=sizeof(coo::Executor)) return 23;
    const Found executorInfo{controller.address+executorOffset,controller.module,executorType,1};
    if(!field(process,executorInfo,L"states_",statesOffset,length) || length!=32*sizeof(coo::StepState)
        || !field(process,executorInfo,L"definition_",definitionOffset,length) || length!=sizeof(void*)) return 24;
    const auto completedAddress=executorInfo.address+statesOffset+5*sizeof(coo::StepState)
        +offsetof(coo::StepState,commands)+offsetof(coo::CommandState,completed);
    const auto valid=[&] {
        auto frame=std::make_unique<m::Frame>();auto executor=std::make_unique<coo::Executor>();
        std::uintptr_t definitionAddress{};coo::Definition definition{};
        if(!read_value(process,controller.address+frameOffset,*frame) || !read_value(process,executorInfo.address,*executor)
            || !frame->enabled || frame->fault || frame->finished || frame->section!=7 || frame->spawnGeneration!=1
            || !frame->consoleScanned || frame->cinematic.phase!=m::cinematics::Phase::gameplay
            || frame->cinematic.owner!=coo::Generation{1,1}) return false;
        const auto d=executor->diagnostics();
        if(d.run!=1 || d.incarnation!=8 || d.phase!=coo::Phase::running || d.failure!=coo::Failure::none) return false;
        const auto& door=frame->native[m::asset_index(m::asset(m::kShip,23,59))];
        const auto& gate=executor->step_state(5);
        if(door.managed || door.generation || door.active || executor->step_state(1).phase!=coo::StepPhase::complete
            || gate.phase!=coo::StepPhase::active || !gate.commands[0].requested || gate.commands[0].completed
            || gate.commands[0].retired || executor->step_state(6).phase!=coo::StepPhase::pending) return false;
        if(!read_value(process,executorInfo.address+definitionOffset,definitionAddress)
            || !read_value(process,definitionAddress,definition) || definition.steps.size()<8 || definition.steps.size()>32) return false;
        coo::Step boss{},entrance{};coo::CommandSpec wait{},open{};
        if(!read_value(process,reinterpret_cast<std::uintptr_t>(definition.steps.data()+5),boss)
            || !read_value(process,reinterpret_cast<std::uintptr_t>(definition.steps.data()+6),entrance)
            || boss.commands.size()!=1 || entrance.commands.size()!=2 || entrance.dependencies!=((1U<<1)|(1U<<5))
            || !read_value(process,reinterpret_cast<std::uintptr_t>(boss.commands.data()),wait)
            || !read_value(process,reinterpret_cast<std::uintptr_t>(entrance.commands.data()),open)) return false;
        std::array<char,12> label{};
        return boss.name.size()==label.size() && read_value(process,reinterpret_cast<std::uintptr_t>(boss.name.data()),label)
            && std::string_view(label.data(),label.size())=="boss cleared"
            && wait.operation==coo::Operation::observation && wait.wait==coo::Wait::observed && wait.asset==m::kModule
            && wait.argument==0x100U+static_cast<unsigned>(m::Cohort::boss)
            && open.operation==coo::Operation::device && open.asset==m::asset(m::kShip,23,59)
            && open.argument==std::bit_cast<std::uint32_t>(1.F);
    };
    if(!valid()) {std::printf("reactor gate guards failed; no write\n");return 25;}
    std::printf("reactor_gate pid=%lu run=1 incarnation=8 approach=complete gate=active door=pending completion_byte=%llX apply=%u\n",
        pid,completedAddress,apply);
    if(!apply) return 0;
    HANDLE writable=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_VM_OPERATION,FALSE,pid);
    if(!writable) return 26;
    if(!valid()) {CloseHandle(writable);return 27;}
    const std::uint8_t complete=1;SIZE_T written{};
    const bool ok=WriteProcessMemory(writable,reinterpret_cast<void*>(completedAddress),&complete,1,&written) && written==1;
    CloseHandle(writable);
    if(!ok) return 28;
    std::uint8_t after{};
    if(!read_value(process,completedAddress,after) || after!=1) return 29;
    std::printf("Bypassed this run's boss gate only. No deaths, targets, collision, or native callbacks changed. Verify door publication next.\n");
    return 0;
}
int main(int argc,char** argv) {
    if(argc!=2 && argc!=3) return 2;
    HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,static_cast<DWORD>(std::strtoul(argv[1],nullptr,10)));
    if(!process) return 3;
    SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_FAIL_CRITICAL_ERRORS);
    if(!SymInitialize(process,"C:\\Destiny 2 Development;C:\\Destiny 2 Development\\bin\\x64",TRUE)) return 4;
    Found controller{};SymEnumSymbols(process,0,"steam_api64!*controller*",found,&controller);
    if(controller.count!=1) {std::printf("controller matches=%u\n",controller.count);return 5;}
    if(argc==3) {
        const std::string_view mode=argv[2];
        if(mode!="--reactor-gate" && mode!="--bypass-reactor-gate") return 12;
        const auto result=reactor_gate(process,controller,GetProcessId(process),mode=="--bypass-reactor-gate");
        SymCleanup(process);CloseHandle(process);return result;
    }
    DWORD offset{};ULONG64 length{};
    if(!field(process,controller,L"frame_",offset,length) || length!=sizeof(m::Frame)) return 8;
    std::printf("frame_offset=%lu pdb_size=%llu compiled_size=%zu\n",offset,length,sizeof(m::Frame));
    auto frame=std::make_unique<m::Frame>();SIZE_T got{};
    if(!ReadProcessMemory(process,reinterpret_cast<void*>(controller.address+offset),frame.get(),sizeof *frame,&got) || got!=sizeof *frame) return 9;
    std::printf("enabled=%u fault=%u finished=%u section=%u spawn_generation=%u scanned=%u\n",
        frame->enabled,frame->fault,frame->finished,frame->section,frame->spawnGeneration,frame->consoleScanned);
    const auto& movie=frame->cinematic;
    std::printf("cinematic_owner=%llu/%u phase=%u movie=%u fly_in=%u deadline=%llu now=%llu masking=%u\n",
        movie.owner.run,movie.owner.value,static_cast<unsigned>(movie.phase),movie.movie,movie.flyInComplete,
        movie.deadline,GetTickCount64(),movie.masking_opening(GetTickCount64()));
    for(const unsigned slot:{56U,57U,58U,59U,60U}) {
        const auto& s=frame->native[m::asset_index(m::asset(m::kShip,23,static_cast<std::uint16_t>(slot)))];
        std::printf("slot=%u generation=%u command=%.9f observed=%.9f revision=%d managed=%u active=%u acknowledged=%u pose_known=%u synchronized=%u retired=%u\n",
            slot,s.generation,s.position,s.observedPosition,s.observedRevision,s.managed,s.active,s.acknowledged,s.poseKnown,s.deviceSynchronized,s.retired);
    }
    using Population=dawn::state::activity::coo::PopulationService<m::EnemyReceipt,m::kSpawns.size(),16>;
    auto population=std::make_unique<Population>();
    if(!field(process,controller,L"population_",offset,length) || length!=sizeof(Population)) return 10;
    if(!ReadProcessMemory(process,reinterpret_cast<void*>(controller.address+offset),population.get(),sizeof *population,&got) || got!=sizeof *population) return 11;
    for(const auto member:m::kBoss) {
        const auto index=m::spawn_index(m::asset(member.registry,1,member.slot));
        const auto& source=m::kSpawns[index];
        const auto missing=population->missing(index,source.count,true);
        std::printf("boss slot=%u enabled=%u requested=%u cleared=%u missing=%u detail=%u/%u\n",member.slot,population->enabled(index),source.count,
            population->cleared(index,source.count),static_cast<unsigned>(missing.missing),missing.expected,missing.actual);
    }
    population->living([](const m::EnemyReceipt& r) {
        if(r.registry==m::kShip && (r.source==33 || r.source==35))
            std::printf("boss_living run=%llu actor=%08X owner=%08X generation=%u source=%u\n",r.run,r.actor,r.owner,r.generation,r.source);
    });
    const auto wanted=m::ship_barrier::wanted({1,1},*frame);
    std::printf("release_eligible_with_valid_owner=%u revisions=%u,%u read_only=1\n",wanted.enabled(),wanted.revisions[0],wanted.revisions[1]);
    SymCleanup(process);CloseHandle(process);return 0;
}
