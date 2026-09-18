#pragma once
#include "../../../../middleware/bap/activity_message/squad_sense.h"
#include "../../../../middleware/bap/activity_message/combatant_sense.h"
#include "../../../../middleware/bap/activity_message/source_sense.h"
#include "../../../../middleware/bap/activity_message/actor_sense.h"

namespace dawn::state::activity::vanilla::one_au {
// Adapt the production decoders' sparse receipts to the imported mission contract.
// Do not change the shared parser or the receipts delivered to existing missions.
inline middleware::bap::activity_message::source_sense::Output source_output(
    const middleware::bap::activity_message::squad_sense::Output& value) noexcept {
    middleware::bap::activity_message::source_sense::Output out{};
    out.costs=value.cost;out.costMask=value.costMask;
    if(value.hasRevision) {out.present|=2U;out.counters[1]=value.revision;}
    if(value.hasAlive) {out.present|=8U;out.counters[3]=value.alive;}
    return out;
}
inline middleware::bap::activity_message::actor_sense::Delta actor_output(
    const middleware::bap::activity_message::combatant_sense::Output& value) noexcept {
    return {value.spawnRevision,value.programRevision,value.programState,value.deliveryRevision,
        value.deliveryState,value.hasSpawnRevision,value.hasProgramRevision,value.hasProgramState,
        value.hasDeliveryRevision,value.detached};
}
}
