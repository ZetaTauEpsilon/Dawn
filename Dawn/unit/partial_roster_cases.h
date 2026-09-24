#pragma once
namespace partial_roster {
// An independent wire fixture builder, exercising the public parser including
// epoch, roster envelope, changed-group terminator and final padding.
struct Packet {
    std::vector<std::byte> data;std::size_t bits{};
    void put(std::uint8_t width,std::uint64_t value) {
        for(unsigned i=width;i>0;--i,++bits) {
            if(bits%8==0) data.push_back(std::byte{});
            data.back()|=static_cast<std::byte>(((value>>(i-1))&1)<<(7-bits%8));
        }
    }
    Packet(){put(64,UINT64_MAX);put(64,UINT64_MAX);put(1,0);put(1,1);}
    void body(bool top,bool present,unsigned fields,unsigned keys=2,unsigned states=2) {
        put(1,present);if(!present) return;
        const std::uint8_t width=top?9:7;
        put(1,fields&1?1:0);
        if(fields&1) {put(width,keys);for(unsigned i=0;i<keys;++i) put(32,0x12340000U+i);}
        put(1,fields&2?1:0);
        if(fields&2) for(unsigned i=0;i<(top?8U:3U);++i) put(32,i==0?1:0);
        put(1,fields&4?1:0);
        if(fields&4) {put(width,states);for(unsigned i=0;i<states;++i) put(8,0x80U+i%128);}
    }
    void finish(){put(1,0);put(1,0);}
};
inline void run() {
    auto storage=std::make_unique<sense::SenseUpdate>();auto& update=*storage;
    for(bool topPresent:{false,true}) for(unsigned top=0;top<8;++top)
    for(bool identity:{false,true}) for(bool bubblePresent:{false,true}) for(unsigned bubble=0;bubble<8;++bubble) {
        Packet packet;packet.body(true,topPresent,top);packet.put(1,1);packet.put(7,1);
        packet.put(1,identity);if(identity) packet.put(32,0x80000008U);
        packet.body(false,bubblePresent,bubble);packet.finish();
        std::size_t consumed{};
        CHECK(sense::parse_sense_update(packet.data,update,consumed));CHECK(consumed==packet.bits);
        const unsigned topCount=topPresent && top==7?2U:0U;
        const unsigned bubbleCount=identity && bubblePresent && bubble==7?2U:0U;
        CHECK(update.hasRosterAcknowledgement && update.bubbleBlockCount==1);
        CHECK(update.topLevelRosterCount==topCount && update.rosterEntryCount==topCount+bubbleCount);
        for(unsigned i=0;i<topCount+bubbleCount;++i) {
            CHECK(update.rosterEntries[i].registryKey==0x12340000U+i%2);
            CHECK(update.rosterEntries[i].state==0x80U+i%2);
            CHECK(update.rosterEntries[i].active==(i%2==0));
            CHECK(update.rosterEntries[i].bubble==(i<topCount?-1:8));
        }
    }
    const auto rejected=[&](Packet& p) {
        std::size_t consumed=123;CHECK(!sense::parse_sense_update(p.data,update,consumed));
        CHECK(consumed==0 && update.rosterEntryCount==0 && !update.hasRosterAcknowledgement);
    };
    for(bool top:{false,true}) for(unsigned fields:{5U,7U}) {
        Packet p;
        if(!top){p.body(true,false,0);p.put(1,1);p.put(7,1);p.put(1,1);p.put(32,0x80000008U);}
        p.body(top,true,fields,2,1);if(top)p.put(1,0);p.finish();rejected(p);
    }
    for(bool top:{false,true}) for(unsigned fields:{1U,4U,7U}) {
        Packet p;
        if(!top){p.body(true,false,0);p.put(1,1);p.put(7,1);p.put(1,1);p.put(32,0x80000008U);}
        p.body(top,true,fields,top?257:97,top?257:97);if(top)p.put(1,0);p.finish();rejected(p);
    }
    for(auto id:{0x7FFFFFFFU,0x80000040U}) {
        Packet p;p.body(true,false,0);p.put(1,1);p.put(7,1);p.put(1,1);p.put(32,id);p.body(false,true,7);p.finish();rejected(p);
    }
    Packet capacity;capacity.body(true,true,7,128,128);capacity.put(1,1);capacity.put(7,1);capacity.put(1,1);
    capacity.put(32,0x80000008U);capacity.body(false,true,7,1,1);capacity.finish();rejected(capacity);
    Packet tooMany;tooMany.body(true,false,0);tooMany.put(1,1);tooMany.put(7,65);tooMany.finish();rejected(tooMany);
    Packet full;full.body(true,true,7);full.put(1,1);full.put(7,1);full.put(1,1);full.put(32,0x80000008U);full.body(false,true,7);full.finish();
    while(!full.data.empty()) {full.data.pop_back();rejected(full);}
    Packet empty;empty.body(true,false,0);empty.put(1,0);empty.finish();std::size_t consumed{};
    CHECK(sense::parse_sense_update(empty.data,update,consumed) && consumed==empty.bits && update.rosterEntryCount==0);
}
}
