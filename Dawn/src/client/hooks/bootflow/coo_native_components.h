#pragma once
#include <array>
#include <cstdint>
#include <span>
namespace dawn::client::hooks::bootflow::coo_native {
// Read-only counterpart of native resource iterator 591290/59A350. A component
// must resolve back to itself and the expected entity. Reflected base/interface
// rows may alias that same component; only distinct matching components conflict.
template<class Read,std::size_t MaximumRows=256> bool component(Read& read,std::uint32_t bundle,std::uint32_t entity,
                                   std::uint32_t kind,std::uintptr_t& result,std::uint32_t definition=0) noexcept {
    static_assert(MaximumRows<=1024);
    result=0;std::array<std::uint32_t,64> visited{};std::size_t used{};
    while(bundle!=UINT32_MAX && used<visited.size()) {
        for(std::size_t i=0;i<used;++i) { if(visited[i]==bundle) { return false; } }visited[used++]=bundle;
        std::uintptr_t base{},allocation{};std::uint32_t flags{},type{};
        if(!read.resolve(bundle,base,&allocation) || !read.value(base,flags)) { return false; }
        if((flags&2U)==0) {
            std::uintptr_t metadata{};std::uint64_t count{};std::int64_t relative{};
            if(!read.value(base+4,type) || !read.resolve(type,metadata)
                || !read.value(metadata+0x68,count) || count>MaximumRows || !read.value(metadata+0x70,relative)) { return false; }
            if(count && (!relative || relative>0x1000000 || relative < -0x1000000 || metadata>UINTPTR_MAX-0x1000080 || (relative<0 && metadata+0x80<static_cast<std::uintptr_t>(-relative)))) { return false; }
            const auto rows=count==0?std::uintptr_t{}:relative<0?metadata+0x80-static_cast<std::uintptr_t>(-relative):metadata+0x80+static_cast<std::uintptr_t>(relative);
            for(std::uint64_t i=0;i<count;++i) {
                std::int32_t offset{};std::uint32_t actual{},owner{},self{};
                if(!read.value(rows+i*24+0x14,offset) || offset<0 || offset>0x400000 || base>UINTPTR_MAX-static_cast<std::uintptr_t>(offset)-0x30) { return false; }
                const auto address=base+static_cast<std::uintptr_t>(offset);
                // Interface rows can alias one already validated component.
                // Re-resolving hundreds of aliases exhausted the bounded reader
                // before it reached the end of vehicle/player resource bundles.
                if(result && address==result) { continue; }
                if(!read.value(address+4,actual)) { return false; }if(actual!=kind) { continue; }
                if(definition) {if(!read.value(address,actual)) {return false;}if(actual!=definition) {continue;}}
                std::uintptr_t resolved{};
                if(!read.value(address+0x24,self) || self==UINT32_MAX || !read.value(address+0x2C,owner) || owner!=entity
                    || !read.resolve(self,resolved) || resolved!=address || (result && result!=address)) { return false; }
                result=address;
            }
        }
        if(!read.value(allocation+0x18,bundle)) { return false; }
    }
    return bundle==UINT32_MAX && result!=0;
}
}
