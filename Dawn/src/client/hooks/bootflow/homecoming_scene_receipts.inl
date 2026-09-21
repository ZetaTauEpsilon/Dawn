// Read-only observation at the existing, signature-checked B438B0 Scene tick.
void observe_homecoming_scene(std::uintptr_t component) noexcept {
    namespace hc=state::activity::vanilla::homecoming;
    Read read;Ref ref{};std::uintptr_t definition{},root{};Weak weak{},after{};Diag diag{};
    if(!read.value(component,ref) || ref.kind!=0x80806266U || ref.offset!=0x368) {return;}
    const auto wanted=hc::playback_request(ref.handle);if(!wanted.owner.valid()) {return;}
    std::array<std::byte,8> scope{};std::uint32_t generation{},self{};std::uint8_t complete{};
    if(!read.resolve(ref,definition) || !read.copy(definition+0x30,scope)
        || at<std::uint32_t>(scope.data())!=wanted.scene.registry || at<std::uint16_t>(scope.data()+4)!=43
        || at<std::uint16_t>(scope.data()+6)!=wanted.scene.slot
        || !read.value(component+0x254,generation) || generation!=wanted.generation
        || !read.value(component+0x258,complete) || complete>1
        || !read.value(component+0x2E8,weak) || !read.weak(weak,root,diag)) {return;}
    Ref header{};
    if(!read.value(root,header) || header.kind!=0x80806384U || !read.value(root+0x24,self) || self!=weak.handle) {return;}
    hc::PlaybackReceipt receipt{wanted,weak.handle,weak.serial};
    const auto& door=hc::kShaxxDoorOpening;
    if(wanted.scene==door.scene && header.handle==door.graph && header.offset==door.root) {
        Ref node{},childHeader{};Weak child{},childAfter{};std::uintptr_t performer{};
        std::uint8_t state{};std::int32_t stops{};
        // Child 25 is the closed-door idle used during early staging. Only
        // child 26 starts the opening performance after Shaxx's entry input.
        receipt.entryCue=read.value(root+door.node,node) && node.handle==door.graph
            && node.kind==0x808062FEU && node.offset==door.definition
            && read.value(root+door.node+0x98,state) && read.value(root+door.node+0x1A0,stops)
            && hc::door_opening_started(state,stops)
            && read.value(root+door.node+0x1B0,child) && read.weak(child,performer,diag)
            && read.value(performer,childHeader) && childHeader.handle==door.child
            && childHeader.kind==0x808084E9U && childHeader.offset==door.childRoot
            && read.value(root+door.node+0x1B0,childAfter)
            && childAfter.handle==child.handle && childAfter.serial==child.serial;
    }
    if(const auto* hold=hc::combat_hold_for(wanted.scene);hold && header.handle==hold->graph && header.offset==hold->root) {
        Ref node{};std::uintptr_t data{};std::uint8_t state{};std::uint32_t input{};
        receipt.combatHeld=read.value(root+hold->node,node) && node.handle==hold->graph
            && node.kind==0x80806278U && node.offset==hold->definition
            && read.value(root+hold->node+0x98,state) && state==1 && read.resolve(node,data)
            && read.value(data+0x68,input) && input==hold->input;
    }
    if(const auto* selected=hc::performance_for(wanted);selected && header.handle==selected->graph && header.offset==selected->root) {
        const auto& p=*selected;
        Ref node{};std::uint8_t state{};std::int32_t stops{};
        if(read.value(root+p.node,node) && node.handle==p.graph && node.kind==p.kind && node.offset==p.definition
            && read.value(root+p.node+0x98,state) && state<=2) {
            if(p.kind==0x808062FEU && read.value(root+p.node+0x1A0,stops)) {
                receipt.performanceFinished=hc::natural_performance(state,stops);
            } else if(p.kind==0x80806297U) {
                std::uintptr_t action{};std::uint32_t parameter{},flag{};
                receipt.performanceFinished=state==2 && read.resolve(node,action)
                    && read.value(action+0x58,parameter) && parameter==0
                    && read.value(action+0x5C,flag) && flag==0x1526DC06U;
            } else if(p.kind==0x80806307U && wanted.revival) {
                Ref idle{};std::uint8_t idleState{};std::uintptr_t action{},idleAction{};
                std::uint32_t parameter{},animation{},idleParameter{},idleAnimation{},idleInput{};
                receipt.performanceFinished=read.value(root+hc::kRevivalIdleNode,idle)
                    && idle.handle==p.graph && idle.kind==p.kind && idle.offset==hc::kRevivalIdleDefinition
                    && read.value(root+hc::kRevivalIdleNode+0x98,idleState) && hc::revival_handoff(state,idleState)
                    && read.resolve(node,action) && read.value(action+0x58,parameter) && parameter==0
                    && read.value(action+0x5C,animation) && animation==hc::kRevivalAnimation
                    && read.resolve(idle,idleAction) && read.value(idleAction+0x58,idleParameter) && idleParameter==0
                    && read.value(idleAction+0x5C,idleAnimation) && idleAnimation==hc::kRevivalIdleAnimation
                    && read.value(idleAction+0x48,idleInput) && idleInput==56;
            }
            if(p.scene==hc::asset(hc::kBoulevard,43,16) && state==1 && stops==0) {
                // 2D5973BE -> input80 at prototype13D0: entry-effect cue, not the ship beat.
                Ref signal{},childHeader{};Weak child{},actor{};std::uintptr_t performer{},actorAddress{};
                std::uint32_t count{};std::uint8_t flag{};
                if(read.value(root+0x1340,signal) && signal.handle==p.graph && signal.kind==0x80806358U
                    && signal.offset==0xD230 && read.value(root+0x13A0,count) && count>0
                    && read.value(root+p.node+0x1B0,child) && read.weak(child,performer,diag)
                    && read.value(performer,childHeader) && childHeader.handle==0x80B3A848U
                    && childHeader.kind==0x808084E9U && childHeader.offset==0x1E98
                    && read.value(performer+0x290,actor) && read.weak(actor,actorAddress,diag)
                    && read.value(performer+0x29D,flag) && flag<=1) {receipt.entryCue=true;}
            }
        }
    }
    // Package-validated watches replace a full action-table walk before AND
    // after every Scene tick. Each watched runtime header/state is read once;
    // selector/run/generation/serial revalidation below is unchanged.
    if(const auto* plan=hc::scene_action_plan(header.handle,static_cast<std::uint32_t>(header.offset));
        plan && header.offset==plan->root) {
        for(const auto& action:plan->actions) {
            std::array<std::byte,0xA0> bytes{};
            if(!read.copy(root+action.node,bytes)) continue;
            const auto node=at<Ref>(bytes.data());const auto state=at<std::uint8_t>(bytes.data()+0x98);
            const auto kind=action.kind==hc::SceneActionKind::speech?0x808062F6U:0x80806297U;
            if(node.handle!=plan->graph || node.kind!=kind || node.offset!=action.definition) continue;
            if(action.kind==hc::SceneActionKind::speech) {
                if(state==1 || state==2) receipt.speech.set(action.value);
            } else if(state==2) {
                const auto bit=static_cast<std::uint16_t>(1U<<action.value);
                if(action.kind==hc::SceneActionKind::combat) receipt.combatReleased|=bit;
                else receipt.damageReleased|=bit;
            }
        }
    }
    std::uint32_t afterGeneration{};std::uintptr_t afterRoot{};Ref afterHeader{};
    if(!read.value(component+0x2E8,after) || after.handle!=weak.handle || after.serial!=weak.serial
        || !read.weak(after,afterRoot,diag) || afterRoot!=root
        || !read.value(root,afterHeader) || afterHeader.handle!=header.handle
        || afterHeader.kind!=header.kind || afterHeader.offset!=header.offset
        || !read.value(component+0x254,afterGeneration) || afterGeneration!=generation) {return;}
    hc::observe_playback(receipt);
}
