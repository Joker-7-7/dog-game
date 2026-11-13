#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <set>
#include <utility>

namespace {



template <typename Range>
struct IsPermutationMatcher : Catch::Matchers::MatcherGenericBase {
    IsPermutationMatcher(Range range)
        : range_{std::move(range)} {
        std::sort(std::begin(range_), std::end(range_));
    }
    IsPermutationMatcher(IsPermutationMatcher&&) = default;

    template <typename OtherRange>
    bool match(OtherRange other) const {
        using std::begin;
        using std::end;

        std::sort(begin(other), end(other));
        return std::equal(begin(range_), end(range_), begin(other), end(other));
    }

    std::string describe() const override {
        // Description of the property checked by the matcher:
        return "Is permutation of: " + Catch::rangeToString(range_);
    }

private:
    Range range_;
};

template<typename Range>
IsPermutationMatcher<Range> IsPermutation(Range&& range) {
    return IsPermutationMatcher<Range>{std::forward<Range>(range)};
} 

class Provider : public collision_detector::ItemGathererProvider {
public:
    size_t ItemsCount() const override {
        return items_.size();
    }
    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }
    size_t GatherersCount() const override {
        return gatherers_.size();
    }
    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

    std::vector<collision_detector::Gatherer> gatherers_;
    std::vector<collision_detector::Item> items_;
};

}  // namespace

TEST_CASE("FindGatherEvents detects all collision events") {
    using namespace collision_detector;

    Provider provider;
    provider.gatherers_ = {
        Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.2},
        Gatherer{{0.0, 2.0}, {10.0, 2.0}, 1.2},
    };
    provider.items_ = {
        Item{{3.0, 0.0}, 0.0},  // crosses the trajectory of gatherer 0 only
        Item{{6.0, 2.0}, 0.0},  // crosses the trajectory of gatherer 1 only
        Item{{5.0, 1.0}, 0.0},  // at distance 1 <= 1.2 from both trajectories
    };

    const auto events = FindGatherEvents(provider);

    std::set<std::pair<size_t, size_t>> actual;
    for (const auto& event : events) {
        actual.emplace(event.item_id, event.gatherer_id);
    }

    const std::set<std::pair<size_t, size_t>> expected = {
        {0, 0},
        {1, 1},
        {2, 0},
        {2, 1},
    };
    CHECK(events.size() == expected.size());  // catch duplicate events too
    CHECK(actual == expected);
}

TEST_CASE("FindGatherEvents detects no extra events") {
    using namespace collision_detector;

    Provider provider;
    provider.gatherers_ = {
        Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.2},
        Gatherer{{0.0, 0.0}, {10.0, 10.0}, 1.2},
    };
    provider.items_ = {
        Item{{4.0, 0.0}, 0.0},   // lies on the path of gatherer 0 — the only event
        Item{{0.0, 5.0}, 0.0},   // zero offset along the x axis from the start of the diagonal,
                                 // but the perpendicular distance ~3.5 > 1.2
        Item{{-3.0, 0.0}, 0.0},  // before the start of gatherer 0's trajectory
        Item{{13.0, 0.0}, 0.0},  // after the end of gatherer 0's trajectory
        Item{{5.0, 1.3}, 0.0},   // distance 1.3 > 1.2 + 0 — not collected
    };

    const auto events = FindGatherEvents(provider);

    std::set<std::pair<size_t, size_t>> actual;
    for (const auto& event : events) {
        actual.emplace(event.item_id, event.gatherer_id);
    }

    const std::set<std::pair<size_t, size_t>> expected = {
        {0, 0},
    };
    CHECK(events.size() == expected.size());  // catch duplicate events too
    CHECK(actual == expected);
}

TEST_CASE("FindGatherEvents returns events in chronological order") {
    using namespace collision_detector;

    Provider provider;
    provider.gatherers_ = {
        Gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.2},  // events on this trajectory are late
        Gatherer{{0.0, 2.0}, {10.0, 2.0}, 1.2},  // events on this trajectory are early
    };
    provider.items_ = {
        Item{{7.0, 0.0}, 0.0},  // time = 0.7 on the trajectory of gatherer 0
        Item{{9.0, 0.0}, 0.0},  // time = 0.9 on the trajectory of gatherer 0
        Item{{2.0, 2.0}, 0.0},  // time = 0.2 on the trajectory of gatherer 1
        Item{{4.0, 2.0}, 0.0},  // time = 0.4 on the trajectory of gatherer 1
    };

    const auto events = FindGatherEvents(provider);

    // Expected order is by ascending time, not by gatherer_id:
    // {2,1}(0.2), {3,1}(0.4), {0,0}(0.7), {1,0}(0.9)
    const std::vector<std::pair<size_t, size_t>> expected = {
        {2, 1},
        {3, 1},
        {0, 0},
        {1, 0},
    };
    std::vector<std::pair<size_t, size_t>> actual;
    for (const auto& event : events) {
        actual.emplace_back(event.item_id, event.gatherer_id);
    }
    CHECK(events.size() == expected.size());  // catch duplicate events too
    CHECK(actual == expected);                // vector: order matters
}

TEST_CASE("FindGatherEvents returns events with correct data") {
    using namespace collision_detector;
    using Catch::Matchers::WithinAbs;

    Provider provider;
    provider.gatherers_ = {
        Gatherer{{0.0, 0.0}, {10.0, 0.0}, 0.2},
    };
    provider.items_ = {
        Item{{8.0, 0.0}, 0.0},  // item 0: time = 0.8, sq_distance = 0
        Item{{2.0, 0.0}, 0.0},  // item 1: time = 0.2, sq_distance = 0
        Item{{4.0, 1.0}, 0.9},  // item 2: time = 0.4, sq_distance = 1
                                // (collected thanks to its width: 0.2 + 0.9 >= 1)
    };

    const auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 3);

    // Events are sorted by time: 0.2 (item 1), 0.4 (item 2), 0.8 (item 0).
    {
        const auto& e = events[0];
        CHECK(e.item_id == 1);
        CHECK(e.gatherer_id == 0);
        CHECK_THAT(e.time, WithinAbs(0.2, 1e-10));
        CHECK_THAT(e.sq_distance, WithinAbs(0.0, 1e-10));
    }
    {
        const auto& e = events[1];
        CHECK(e.item_id == 2);
        CHECK(e.gatherer_id == 0);
        CHECK_THAT(e.time, WithinAbs(0.4, 1e-10));
        CHECK_THAT(e.sq_distance, WithinAbs(1.0, 1e-10));
    }
    {
        const auto& e = events[2];
        CHECK(e.item_id == 0);
        CHECK(e.gatherer_id == 0);
        CHECK_THAT(e.time, WithinAbs(0.8, 1e-10));
        CHECK_THAT(e.sq_distance, WithinAbs(0.0, 1e-10));
    }
}