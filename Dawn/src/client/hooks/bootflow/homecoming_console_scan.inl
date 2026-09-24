namespace homecoming_scan {
namespace hc=state::activity::vanilla::homecoming;
void update(std::uintptr_t sensor) noexcept {
    const auto wanted=hc::console_scan_request();if(!wanted.enabled()) { return; }
    Read read{image};hc::console_native::Identity native{};
    const auto result=hc::console_native::retain(read,image,sensor,wanted,
        []() noexcept { return hc::console_scan_request(); },
        [](std::uint32_t owner) noexcept {
            reinterpret_cast<void(__fastcall*)(std::uint32_t,std::uint8_t) noexcept>(image+hc::console_native::kAuthoritySetter)(owner,1U);
        },native);
    if(result!=hc::console_native::Result::granted) { return; }
    std::array<char,224> line{};
    const int n=std::snprintf(line.data(),line.size(),
        "ev=homecoming stage=console_authority run=%llu generation=%u entity=%08X controller=%08X result=local",
        static_cast<unsigned long long>(wanted.owner.run),wanted.generation,native.entity.handle,native.controller.handle);
    if(n>0 && static_cast<std::size_t>(n)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
    }
}
}
