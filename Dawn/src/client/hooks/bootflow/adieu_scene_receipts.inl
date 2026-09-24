// Runs after the original Scene tick. Every address stays inside the validated
// live selector; recheck its weak handle and generation before publishing.
void observe_adieu_scene(std::uintptr_t component) noexcept {
    namespace ad=state::activity::vanilla::adieu;
    Read read;Ref ref{};std::uintptr_t definition{},root{};Weak weak{},after{};Diag diag{};
    if(!read.value(component,ref) || ref.kind!=0x80806266U || ref.offset!=0x368) return;
    const auto wanted=ad::playback_request(ref.handle);if(!wanted.owner.valid()) return;
    const auto index=ad::Presentation::scene_index(wanted.scene);if(index>=std::size(ad::kScenes)) return;
    const auto& scene=ad::kScenes[index];
    std::array<std::byte,8> scope{};std::uint32_t generation{},self{};std::uint8_t complete{};
    if(!read.resolve(ref,definition) || !read.copy(definition+0x30,scope)
        || at<std::uint32_t>(scope.data())!=wanted.scene.registry || at<std::uint16_t>(scope.data()+4)!=43
        || at<std::uint16_t>(scope.data()+6)!=wanted.scene.slot
        || !read.value(component+0x254,generation) || generation!=wanted.generation
        || !read.value(component+0x258,complete) || complete>1
        || !read.value(component+0x2E8,weak)) return;
    if(complete==1 && weak.handle==UINT32_MAX) {
        // A final action can run and destroy the selector inside the original
        // tick. The Scene component retains completion and generation afterward.
        Ref afterRef{};std::uint32_t afterGeneration{};std::uint8_t afterComplete{};
        if(!read.value(component,afterRef) || afterRef.handle!=ref.handle || afterRef.kind!=ref.kind || afterRef.offset!=ref.offset
            || !read.value(component+0x254,afterGeneration) || afterGeneration!=generation
            || !read.value(component+0x258,afterComplete) || afterComplete!=complete
            || !read.value(component+0x2E8,after) || after.handle!=weak.handle || after.serial!=weak.serial) return;
        const auto receipt=ad::completed_playback(wanted,generation,complete,weak.handle);
        if(receipt.request.owner.valid()) ad::observe_playback(receipt);
        return;
    }
    if(!read.weak(weak,root,diag)) return;
    Ref header{};
    if(!read.value(root,header) || header.kind!=0x80806384U || header.handle!=scene.graph || header.offset!=scene.root
        || !read.value(root+0x24,self) || self!=weak.handle) return;
    ad::PlaybackReceipt receipt{wanted,weak.handle,weak.serial};
    for(const auto& p:ad::kActions) if(p.scene==wanted.scene && header.handle==p.graph && header.offset==p.root) {
        Ref node{};std::uint8_t state{};std::int32_t stops{};
        if(!read.value(root+p.node,node) || node.handle!=p.graph || node.kind!=p.kind || node.offset!=p.definition
            || !read.value(root+p.node+0x98,state) || state>2) continue;
        if(p.scene==ad::kFinding && p.node==0x18A0 && p.kind==0x808062FEU && read.value(root+p.node+0x1A0,stops))
            receipt.performanceFinished=state==2 && stops==0;
        if(p.kind==0x808062E4U && p.event && state==2)
            for(std::size_t n=0;n<scene.events.size();++n) if(scene.events[n]==p.event) receipt.outputs.set(n);
    }
    std::uint64_t count{};std::int64_t relative{};
    if(read.value(root+0x38,count) && count<=128 && read.value(root+0x40,relative) && relative>0 && relative<0x20000) {
        const auto table=root+0x40+static_cast<std::uintptr_t>(relative)+0x10;
        for(std::size_t i=0;i<count;++i) {
            const auto field=table+i*0x30+0x20;std::int64_t displacement{};std::uintptr_t address{},data{};Ref node{};std::uint8_t state{};
            if(!read.value(field,displacement) || !add(field,displacement,address) || !read.value(address,node)
                || node.handle!=header.handle || node.kind!=0x808062F6U || !read.value(address+0x98,state)
                || (state!=1 && state!=2) || !read.resolve(node,data)) continue;
            std::uint32_t selector{},bank{};
            if(read.value(data+0x78,selector) && read.value(data+0x7C,bank) && bank==ad::kBank)
                for(std::size_t row=0;row<std::size(ad::kDialogue);++row) if(ad::kDialogue[row].selector==selector) {
                    receipt.speech.set(row);
                    if(state==2) receipt.speechFinished.set(row);
                }
        }
    }
    std::uint32_t afterGeneration{};std::uintptr_t afterRoot{};Ref afterHeader{};
    if(!read.value(component+0x2E8,after) || after.handle!=weak.handle || after.serial!=weak.serial
        || !read.weak(after,afterRoot,diag) || afterRoot!=root || !read.value(root,afterHeader)
        || afterHeader.handle!=header.handle || afterHeader.kind!=header.kind || afterHeader.offset!=header.offset
        || !read.value(component+0x254,afterGeneration) || afterGeneration!=generation) return;
    ad::observe_playback(receipt);
}
