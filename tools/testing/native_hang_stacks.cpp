#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cstdlib>
#include <array>

// Local read-only hang diagnostic. Each thread is resumed before formatting or
// symbol lookup; no remote calls, memory writes, or persistent debugger attach.
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    const auto pid=static_cast<DWORD>(std::strtoul(argv[1],nullptr,10));
    HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
    if(!process) {std::printf("OpenProcess error=%lu\n",GetLastError());return 3;}
    SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_FAIL_CRITICAL_ERRORS);
    const bool symbols=SymInitialize(process,"C:\\Destiny 2 Development;C:\\Destiny 2 Development\\bin\\x64",TRUE)!=FALSE;
    std::uintptr_t image{};DWORD tlsIndex=TLS_OUT_OF_INDEXES;
    HANDLE modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);MODULEENTRY32 moduleEntry{sizeof moduleEntry};
    if(modules!=INVALID_HANDLE_VALUE) {
        if(Module32First(modules,&moduleEntry)) image=reinterpret_cast<std::uintptr_t>(moduleEntry.modBaseAddr);
        CloseHandle(modules);
    }
    SIZE_T copied{};
    if(image) ReadProcessMemory(process,reinterpret_cast<void*>(image+0x20BBB30),&tlsIndex,sizeof tlsIndex,&copied);
    struct Basic {LONG status;void* teb;ULONG_PTR process,thread,affinity;LONG priority,basePriority;};
    using Query=LONG(NTAPI*)(HANDLE,ULONG,void*,ULONG,ULONG*);
    const auto query=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationThread"));
    HANDLE threads=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);THREADENTRY32 entry{sizeof entry};
    if(threads==INVALID_HANDLE_VALUE) {CloseHandle(process);return 4;}
    for(bool more=Thread32First(threads,&entry)!=FALSE;more;more=Thread32Next(threads,&entry)!=FALSE) {
        if(entry.th32OwnerProcessID!=pid) continue;
        HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_SUSPEND_RESUME|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
        if(!thread) continue;
        if(entry.th32ThreadID==64520 && query) {
            Basic basic{};std::uintptr_t tls{},expansion{};
            if(query(thread,0,&basic,sizeof basic,nullptr)>=0 && tlsIndex!=TLS_OUT_OF_INDEXES) {
                auto slot=reinterpret_cast<std::uintptr_t>(basic.teb)+0x1480+8*tlsIndex;
                if(tlsIndex>=64) {
                    ReadProcessMemory(process,reinterpret_cast<char*>(basic.teb)+0x1780,&expansion,sizeof expansion,&copied);
                    slot=expansion+8*(tlsIndex-64);
                }
                ReadProcessMemory(process,reinterpret_cast<void*>(slot),&tls,sizeof tls,&copied);
                std::array<unsigned long long,16> values{};
                if(ReadProcessMemory(process,reinterpret_cast<void*>(tls),values.data(),sizeof values,&copied)) {
                    std::printf("THREAD_ALLOCATOR tid=%lu index=%lu tls=%llX\n",entry.th32ThreadID,tlsIndex,tls);
                    for(unsigned i=0;i<values.size();++i) std::printf("  ACTUAL_TLS+%02X=%llX\n",i*8,values[i]);
                }
            }
        }
        CONTEXT context{};context.ContextFlags=CONTEXT_FULL;
        if(SuspendThread(thread)==DWORD(-1)) {CloseHandle(thread);continue;}
        const bool captured=GetThreadContext(thread,&context)!=FALSE;
        // StackWalk reads the process only. Threads are hung and their stacks
        // remain stable; do not hold a suspension across DbgHelp's locks/I/O.
        ResumeThread(thread);
        std::printf("THREAD %lu rip=%llX rsp=%llX\n",entry.th32ThreadID,context.Rip,context.Rsp);
        if(captured) {
            STACKFRAME64 frame{};frame.AddrPC={context.Rip,0,AddrModeFlat};
            frame.AddrStack={context.Rsp,0,AddrModeFlat};frame.AddrFrame={context.Rbp,0,AddrModeFlat};
            for(unsigned depth=0;depth<40;++depth) {
                const auto pc=frame.AddrPC.Offset;if(!pc) break;
                IMAGEHLP_MODULE64 module{};module.SizeOfStruct=sizeof module;
                const bool found=symbols && SymGetModuleInfo64(process,pc,&module);
                alignas(SYMBOL_INFO) std::array<unsigned char,sizeof(SYMBOL_INFO)+1024> buffer{};
                auto* symbol=reinterpret_cast<SYMBOL_INFO*>(buffer.data());symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=1023;
                DWORD64 displacement{};const bool named=symbols && SymFromAddr(process,pc,&displacement,symbol);
                std::printf("  %02u %016llX %s+%llX %s+%llX\n",depth,pc,found?module.ModuleName:"?",
                    found?pc-module.BaseOfImage:pc,named?symbol->Name:"?",named?displacement:0);
                if(found && pc-module.BaseOfImage==0x98D8C) {
                    std::printf("  FAULT_CONTEXT rax=%llX rcx=%llX rdx=%llX rbx=%llX rsp=%llX\n",
                        context.Rax,context.Rcx,context.Rdx,context.Rbx,context.Rsp);
                    std::array<unsigned long long,16> tls{};SIZE_T copied{};
                    if(ReadProcessMemory(process,reinterpret_cast<void*>(context.Rax),tls.data(),sizeof tls,&copied))
                        for(unsigned i=0;i<tls.size();++i) std::printf("  TLS+%02X=%llX\n",i*8,tls[i]);
                }
                if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr)) break;
            }
        }
        CloseHandle(thread);
    }
    CloseHandle(threads);if(symbols) SymCleanup(process);CloseHandle(process);return 0;
}
