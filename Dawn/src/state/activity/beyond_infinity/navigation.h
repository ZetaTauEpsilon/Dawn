#pragma once
#include "frame.h"
namespace dawn::state::activity::beyond_infinity::navigation {
// Supplied 0.1.5.2, RVAs 26CE70 and 26CD30.
inline coo::MarkerTarget marker(std::uint32_t event,const Frame& f) noexcept {
    std::uint32_t registry{};std::uint16_t slot{};
    switch(event) {
    case 0x29BFCE5A:registry=0xDA02FEF1;break;
    case 0x2FDA4350:registry=0x233E7149;slot=f.lensExposed?36:34;break;
    case 0x5569523A:registry=0x0FF26BCC;slot=4;break;
    case 0xBE5F8E8B:registry=0xC7FB7155;slot=1;break;
    default:return {};
    }
    const auto* binding=find(registry,4,slot);if(!binding)return {};
    const auto i=asset_index(binding->asset);if(i>=f.native.size() || !f.native[i].managed || !f.native[i].active)return {};
    return {binding->asset,{0x811C9DC5,0,0,0}};
}
inline bool admitted(const Frame& f,std::uint32_t context) noexcept {
    const auto& p=f.navigation;
    switch(f.presentation.event) {
    case 0x29BFCE5A:return f.section==0 && (context==15 || (context==19 && f.wellEntered));
    case 0x2FDA4350:return context==19 && f.section<2;
    case 0xB1AD777D:return (context==19 || context==8) && (f.section==1 || f.section==2);
    case 0x9225AEF3:return f.section==3 && (context==8 || context==18) && p.forestPastComplete;
    case 0xD60FA7DF:return f.section==4 && context==18 && p.pastEncounter;
    case 0xBE5F8E8B:return f.section==4 && (context==18 || context==8) && p.pastEncounter;
    case 0x142EC956:return f.section==5 && ((context==18 && p.pastReturn)
        || (context==8 && (p.pastReturn || p.forestFutureComplete)) || (context==4 && p.forestFutureComplete));
    case 0x64B46F54:case 0x5569523A:return f.section==6 && context==4 && p.futureEntered;
    case 0x5AD156F5:return (f.section==6 || f.section==7) && context==4 && p.futureReturn;
    default:return false;
    }
}
}
