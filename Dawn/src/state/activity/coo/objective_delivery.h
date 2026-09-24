#pragma once
#include "objective_service.h"
namespace dawn::state::activity::coo {
// Native 1009C00 cannot install content that is still loading. A new physical
// directive or readiness recovery rotates the native three-record ring, so
// 1009C00 installs after an earlier body was copied before loading. Record+4 is
// an authored variant index and MUST stay zero; it is not a delivery generation.
class ObjectiveDelivery final {
public:
    void select(Generation owner) noexcept {
        if(owner_!=owner) {*this={};owner_=owner;}
    }
    bool observe(Generation owner,std::uint32_t handle,std::uintptr_t component,std::uintptr_t content,bool ready) noexcept {
        if(!owner.valid() || owner!=owner_ || handle==UINT32_MAX || component<0x10000)return false;
        ready=ready && content>=0x10000;
        const bool changed=handle_!=handle || component_!=component || content_!=content || ready_!=ready;
        if(!changed)return false;
        handle_=handle;component_=component;content_=content;ready_=ready;
        if(ready_)pending_=true;
        return true;
    }
    // 0.1.5.2 Hijacked: already-visible content acknowledges a readiness recovery
    // without replaying the same objective banner through another ring slot.
    void acknowledge(Generation owner,std::uint32_t logicalRevision,std::uint32_t row) noexcept {
        if(owner.valid() && owner==owner_ && ready_ && !exhausted_ && generation_!=0
            && logicalRevision==logicalRevision_ && row==(generation_-1U)%3U)pending_=false;
    }
    ObjectiveState project(Generation owner,ObjectiveState value) noexcept {
        select(owner);
        if(value.published && ready_ && (pending_ || value.revision!=logicalRevision_)) {
            logicalRevision_=value.revision;pending_=false;advance();
        }
        value.published=value.published && owner.valid() && ready_ && generation_!=0 && !exhausted_;
        value.revision=generation_;return value;
    }
private:
    void advance() noexcept {if(generation_==0x7FFFFFFFU) {exhausted_=true;ready_=false;}else ++generation_;}
    Generation owner_{};std::uint32_t handle_{UINT32_MAX},generation_{},logicalRevision_{UINT32_MAX};
    std::uintptr_t component_{},content_{};bool ready_{},exhausted_{},pending_{};
};
}
