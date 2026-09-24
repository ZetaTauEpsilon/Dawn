namespace one_au_scan {
namespace au=state::activity::vanilla::one_au;
void update(std::uintptr_t sensor) noexcept {
    const auto wanted=au::bridge_scan_request();if(!wanted.enabled()) { return; }
    Read read{image};au::bridge_native::Identity native{};
    const auto result=au::bridge_native::retain(read,image,sensor,wanted,
        []() noexcept { return au::bridge_scan_request(); },
        [](std::uint32_t owner) noexcept {
            reinterpret_cast<void(__fastcall*)(std::uint32_t,std::uint8_t) noexcept>(image+au::bridge_native::kAuthoritySetter)(owner,1U);
        },native);
    if(result!=au::bridge_native::Result::granted) { return; }
    std::array<char,224> line{};
    const int n=std::snprintf(line.data(),line.size(),
        "ev=one_au stage=bridge_authority run=%llu generation=%u entity=%08X controller=%08X result=local",
        static_cast<unsigned long long>(wanted.owner.run),wanted.generation,native.entity.handle,native.controller.handle);
    if(n>0 && static_cast<std::size_t>(n)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
    }
}
}
