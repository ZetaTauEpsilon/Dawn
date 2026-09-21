#pragma once
#include <mutex>
#include "lifecycle_service.h"
#include "objective_delivery.h"
namespace dawn::state::activity::coo::waypoint_delivery {
// Recovered 0.1.5.2 registry (43CA10). A physical directive gets a fresh ring
// generation only after its content is ready. Old runs/incarnations cannot win.
struct Ticket {Generation owner{};std::uint32_t definition{};bool valid() const noexcept {return owner.valid() && definition && definition!=UINT32_MAX;}friend bool operator==(Ticket,Ticket)=default;};
class Registry {
    struct Entry {Ticket ticket{};ObjectiveDelivery delivery{};};
    std::array<Entry,8> entries_{};std::uint64_t run_{};
public:
    ObjectiveState project(Ticket ticket,ObjectiveState value) noexcept {
        const auto reject=[&] {value.published=false;return value;};
        if(!ticket.valid() || ticket.owner.run<run_)return reject();
        if(ticket.owner.run!=run_) {entries_={};run_=ticket.owner.run;}
        Entry* selected{};
        for(auto& e:entries_)if(e.ticket.valid() && e.ticket.definition==ticket.definition) {selected=&e;break;}
        if(!selected)for(auto& e:entries_)if(!e.ticket.valid()) {selected=&e;break;}
        if(!selected || selected->ticket.owner.value>ticket.owner.value)return reject();
        selected->ticket=ticket;return selected->delivery.project(ticket.owner,value);
    }
    Ticket lookup(std::uint64_t run,std::uint32_t definition) const noexcept {
        for(const auto& e:entries_)if(e.ticket.valid() && e.ticket.owner.run==run && e.ticket.definition==definition)return e.ticket;
        return {};
    }
    bool observe(Ticket ticket,std::uint32_t handle,std::uintptr_t component,std::uintptr_t content,bool ready) noexcept {
        for(auto& e:entries_)if(e.ticket==ticket && ticket.valid())return e.delivery.observe(ticket.owner,handle,component,content,ready);
        return false;
    }
};
inline std::mutex mutex;inline Registry registry;
inline ObjectiveState project(Ticket t,ObjectiveState s) noexcept {const std::lock_guard lock(mutex);return registry.project(t,s);}
inline Ticket lookup(std::uint64_t run,std::uint32_t definition) noexcept {const std::lock_guard lock(mutex);return registry.lookup(run,definition);}
inline void observe(Ticket t,std::uint32_t h,std::uintptr_t c,std::uintptr_t content,bool ready) noexcept {const std::lock_guard lock(mutex);registry.observe(t,h,c,content,ready);}
}
