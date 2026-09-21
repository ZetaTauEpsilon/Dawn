namespace opening_fade_test {
namespace scope=dawn::client::hooks::bootflow::opening_fade_scope;
namespace profiles=dawn::state::activity::forced::prelaunch;
inline std::array<unsigned,3> arrivals{},queries{};
inline m::cinematics::Sequence staleHomecoming;
void launchpad_arrival() noexcept {++arrivals[0];}
void one_au_arrival() noexcept {++arrivals[1];}
void homecoming_arrival() noexcept {++arrivals[2];static_cast<void>(staleHomecoming.fly_in_complete({2,257}));}
bool launchpad_mask(std::uint64_t) noexcept {++queries[0];return true;}
bool one_au_mask(std::uint64_t) noexcept {++queries[1];return true;}
bool homecoming_mask(std::uint64_t now) noexcept {++queries[2];return staleHomecoming.state().masking_opening(now);}
constexpr std::array<scope::Source,3> sources{{
    {launchpad_arrival,launchpad_mask},{one_au_arrival,one_au_mask},{homecoming_arrival,homecoming_mask},
}};
auto destination(const profiles::Profile& profile) {
    dawn::state::activity::destination::DestinationSelection value{};
    value.activityIndex=profile.activity;value.packageNameLength=static_cast<std::uint8_t>(profile.package.size());
    std::memcpy(value.packageName.data(),profile.package.data(),profile.package.size());return value;
}
void run() {
    const auto exodus=destination(profiles::kAdieu);
    staleHomecoming.begin({2,257},1000);arrivals={};queries={};
    check(!scope::wanted(scope::owner(exodus),sources,1500,true)
        && !staleHomecoming.state().flyInComplete && arrivals==std::array<unsigned,3>{},
        "Exodus arrival cannot arm a Homecoming controller retained by a teardown roster");
    check(staleHomecoming.fly_in_complete({2,257}) && staleHomecoming.state().masking_opening(1500),
        "reproduce the stale Homecoming mask that previously held Exodus until the deadline");
    for(const auto now:{1500ULL,10000ULL,70000ULL})
        check(!scope::wanted(scope::owner(exodus),sources,now) && queries==std::array<unsigned,3>{},
            "joined Exodus immediately excludes all foreign opening masks, without waiting for a timeout");
    constexpr std::array valid{profiles::kLaunchpad,profiles::kOneAu,profiles::kTowerfall};
    for(std::size_t i=0;i<valid.size();++i) {
        arrivals={};queries={};const auto selected=destination(valid[i]);
        check(scope::wanted(scope::owner(selected),sources,2000,true),"each exact mission preserves its own opening mask");
        for(std::size_t j=0;j<valid.size();++j)
            check(arrivals[j]==(i==j?1U:0U) && queries[j]==(i==j?1U:0U),
                "native arrival and mask query reach only the joined mission's controller");
        auto wrong=selected;wrong.activityIndex=profiles::kAdieu.activity;
        check(scope::owner(wrong)==scope::Owner::none,"package alone cannot own an opening mask");
        wrong=exodus;wrong.activityIndex=valid[i].activity;
        check(scope::owner(wrong)==scope::Owner::none,"activity index alone cannot own an opening mask");
    }
    auto invalid=exodus;invalid.packageNameLength=255;
    check(scope::owner(invalid)==scope::Owner::none && scope::owner({})==scope::Owner::none,
        "invalid and missing destinations cannot acquire an opening mask");
    check(staleHomecoming.arrival({2,257},1,2100)
        && staleHomecoming.incident({2,257},5239,m::kMovies[0].registry,6,0,1,2200),"real Homecoming playback releases its own mask");
    check(!scope::wanted(scope::owner(destination(profiles::kTowerfall)),sources,2300),
        "matching identity does not prolong a mask after actual opening playback");
}
}
