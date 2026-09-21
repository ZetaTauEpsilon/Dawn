#pragma once
#include <Windows.h>
#include "../../internal.h"
#include "../../queuez/queuez_state_validation.h"
#include "../../queuez/queuez_outcome_staging.h"
#include "../../../../../state/activity/vanilla/adieu/runtime.h"
#include "../../../../../state/build_data/collectibles/collectible_catalog.h"
#include "../../../../../state/runtime/runtime.h"
#include "../../../../../middleware/secure_channel/runtime.h"
#include "../../../../../core/logging/log.h"
#include <memory>
#include <cstdio>

namespace dawn::server::bap::encrypted::push::activity::adieu_inventory {
namespace mission=state::activity::vanilla::adieu;
inline bool consume(Session& session,Scratch& scratch,std::span<std::byte> response,
    std::size_t& written,bool& touchesScratch) noexcept {
    const auto requested=mission::grant_request();
    if(!requested.valid() || !session.authenticated || session.activity.joinedForeignSession
        || !session.queuez.family4Active || !lifecycle::authentication_key_is_current(session)
        || !queuez::valid(session.queuez)) {return false;}
    // Activity and inventory use different connections. The accepted object lease
    // authorizes the reward; the authenticated Family-4 peer delivers it.
    const auto account=state::account_snapshot();
    if(session.queuez.family4RootSoid!=account.primarySoid) {return false;}
    const state::CharacterState* character{};
    for(std::size_t i=0;i<account.characterCount;++i) {
        if(account.characters[i].selected) {character=&account.characters[i];break;}
    }
    if(!character) {return false;}
    bool resident{};
    for(std::size_t i=0;i<session.queuez.family4ResidentCount;++i)
        if(session.queuez.family4Residents[i].objectSoid==character->soid) {resident=true;break;}
    if(!resident) {return false;}
    static std::uint64_t next{},run{};const auto now=GetTickCount64();
    if(run==requested.binding.owner.run && now<next) {return false;}
    run=requested.binding.owner.run;next=now+250;
    constexpr std::uint32_t reward=1195725819U; // Sorrow MG2; catalog validates the installed item before acquisition.
    for(const auto& equipped:character->equipment.slots) {
        if(equipped && equipped->definitionHash==reward) {
            static_cast<void>(mission::observe_granted(requested,equipped->instanceSoid));return false;
        }
    }
    std::uint64_t owned{};
    for(std::size_t i=0;i<character->inventory.count;++i) {
        const auto& item=character->inventory.values[i];
        if(item.definitionHash==reward) {owned=item.instanceSoid;break;}
    }
    touchesScratch=true;std::size_t size{};auto nonce=session.sendNonce;const auto& key=state::bap().sessionKey;
    auto outcome=std::make_unique<ServiceOutcome>();
    if(owned) {
        auto& tx=outcome->transaction.emplace<EquipmentSwapTransaction>();
        if(!state::prepare_equipment_swap(owned,tx.pending)
            || !queuez::stage_equipment_swap(session.queuez,tx.pending.characterSoid,tx.update)) {return false;}
    } else {
        auto& tx=outcome->transaction.emplace<ItemAcquisitionTransaction>();
        if(!state::prepare_item_acquisition(state::build_data::collectibles::kNoCollectibleIndex,reward,
                tx.pending,state::AcquisitionSource::missionReward)
            || !queuez::stage_item_acquisition(session.queuez,tx.pending.accountSoid,tx.pending.characterSoid,
                tx.pending.acquiredInstanceSoid,tx.pending.profileChanged,tx.update,tx.pending.removedInstanceSoid)) {return false;}
    }
    queuez::StagedPublication publication{};
    const auto instance=owned?owned:std::get<ItemAcquisitionTransaction>(outcome->transaction).pending.acquiredInstanceSoid;
    // Stage the complete inventory/appearance publication before committing. A
    // revoked use lease or a short response buffer cannot leave a partial grant.
    if(!queuez::stage_service_outcome(scratch,session.queuez,*outcome,key,nonce,scratch.framed,size,publication)
        || !publication.hasState || !size || size>response.size()
        || !mission::commit_grant(requested,instance,owned!=0,outcome.get(),[](void* p) noexcept {
            auto& tx=*static_cast<ServiceOutcome*>(p);
            if(auto* equip=transaction_if<EquipmentSwapTransaction>(tx)) {return state::commit_equipment_swap(equip->pending);}
            auto* acquire=transaction_if<ItemAcquisitionTransaction>(tx);
            return acquire && state::commit_item_acquisition(acquire->pending);
        })) {return false;}
    std::copy_n(scratch.framed.begin(),size,response.begin());written=size;
    session.sendNonce=nonce;session.queuez=publication.after;session.accountMutationPublished=true;
    std::array<char,160> line{};
    std::snprintf(line.data(),line.size(),"ev=adieu stage=weapon_grant reward=%u action=%s connection=%u bytes=%zu",
        reward,owned?"equipped":"acquired",session.id,size);
    core::log::write(core::log::Channel::server,core::log::Level::info,line.data());
    return true;
}
}
