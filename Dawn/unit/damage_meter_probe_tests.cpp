// Contracts for the outgoing-damage probe that classifies B7E3C0 summaries.
#include "../src/client/hooks/bootflow/damage_meter_probe.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace probe = dawn::client::hooks::bootflow::damage_meter_probe;
using probe::Direction;

static void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}

// A handle is a 13-bit pool row plus recycle salt. These share a row and differ in salt.
static constexpr std::uint32_t kLocal = 0x0004'1234U;
static constexpr std::uint32_t kSameRow = 0x0009'1234U;
static constexpr std::uint32_t kOther = 0x0004'0777U;
static constexpr std::uint32_t kNone = UINT32_MAX;

static void classification() {
    check(probe::classify(kLocal, kOther, kNone) == Direction::unknown,
          "an unresolved local player classifies as unknown, never as a match");
    check(probe::classify(kLocal, kOther, kLocal) == Direction::outgoing,
          "an exact attacker match is outgoing");
    check(probe::classify(kOther, kLocal, kLocal) == Direction::incoming,
          "an exact target match is incoming");
    check(probe::classify(kLocal, kLocal, kLocal) == Direction::self,
          "both ends local is self, and is not counted as outgoing");
    check(probe::classify(kOther, kOther, kLocal) == Direction::other,
          "neither end local is other");
    check(probe::classify(kSameRow, kOther, kLocal) == Direction::outgoingRow,
          "a shared pool row under a different salt is reported separately, not accepted");
    check(probe::classify(kOther, kSameRow, kLocal) == Direction::incomingRow,
          "the same holds for a target that shares the row");
    // row(UINT32_MAX) is 0x1FFF, so an absent handle must not match a local player
    // that genuinely occupies that row.
    check(probe::classify(kNone, kOther, 0x0000'1FFFU) == Direction::other,
          "an absent attacker never row-matches a local player in the last pool row");
    check(probe::classify(kOther, kNone, 0x0000'1FFFU) == Direction::other,
          "an absent target never row-matches it either");
    check(probe::classify(kNone, kNone, kNone) == Direction::unknown,
          "an absent everything is unknown");
}

static void naming() {
    check(std::string_view(probe::text(Direction::outgoing)) == "out", "outgoing has a stable name");
    check(std::string_view(probe::text(Direction::outgoingRow)) == "out_row",
          "a row-only match is named distinctly from an exact one");
    check(std::string_view(probe::text(Direction::count)) == "unknown",
          "a direction out of range names the unknown bucket rather than reading past the table");
}

static void totals() {
    probe::Window window{};
    check(window.count(Direction::outgoing) == 0 && window.count(Direction::count) == 0,
          "a fresh window totals nothing and is safe to index out of range");

    probe::observe(window, Direction::outgoing, false, false, true, 120.0F);
    probe::observe(window, Direction::outgoing, true, true, false, 80.0F);
    probe::observe(window, Direction::other, false, false, true, 5.0F);
    check(window.calls == 3 && window.count(Direction::outgoing) == 2
              && window.count(Direction::other) == 1,
          "every summary is counted once, under its own direction");
    check(window.killed == 1 && window.modeSet == 1 && window.regionsPresent == 2,
          "the native killed, mode and regions arguments are counted as given");
    check(window.finite == 3 && window.nonFinite == 0 && window.nonPositive == 0,
          "three finite positive amounts");
    check(std::abs(window.amountSum - 205.0) < 1e-6, "the sum covers every finite amount");
    check(std::abs(window.outgoingSum - 200.0) < 1e-6,
          "the outgoing sum covers only the local player's own hits");
    check(window.amountMin == 5.0F && window.amountMax == 120.0F, "the extremes track finite amounts");
}

static void unusable_amounts() {
    probe::Window window{};
    probe::observe(window, Direction::outgoing, false, false, false,
                   std::numeric_limits<float>::quiet_NaN());
    probe::observe(window, Direction::outgoing, false, false, false,
                   std::numeric_limits<float>::infinity());
    check(window.calls == 2 && window.count(Direction::outgoing) == 2,
          "an unusable amount still counts as an observed summary");
    check(window.nonFinite == 2 && window.finite == 0,
          "and is counted as unusable rather than dropped silently");
    check(window.amountSum == 0.0 && window.outgoingSum == 0.0,
          "a non-finite amount never reaches a sum");
    check(window.amountMin == std::numeric_limits<float>::infinity()
              && window.amountMax == -std::numeric_limits<float>::infinity(),
          "nor an extreme, so finite==0 is the only honest report of them");

    // A zero or negative amount is the shape an immune or absorbed hit would take. It is
    // counted so a capture shows whether B7E3C0 reports those at all.
    probe::observe(window, Direction::incoming, false, false, false, 0.0F);
    probe::observe(window, Direction::incoming, false, false, false, -3.0F);
    check(window.finite == 2 && window.nonPositive == 2,
          "zero and negative amounts are finite and separately counted");
    check(std::abs(window.amountSum + 3.0) < 1e-6 && window.outgoingSum == 0.0,
          "they reach the overall sum but not the outgoing one");
}

static void row_matches_are_outgoing_for_totals() {
    probe::Window window{};
    probe::observe(window, Direction::outgoingRow, false, false, false, 10.0F);
    check(window.outgoingSum == 10.0 && window.count(Direction::outgoing) == 0,
          "a row-only attacker contributes to the outgoing sum while staying its own bucket, "
          "so a capture shows what salt handling would cost");
}

int main() {
    classification();
    naming();
    totals();
    unusable_amounts();
    row_matches_are_outgoing_for_totals();
    std::puts("PASS damage probe classification, totals and unusable-amount handling");
}
