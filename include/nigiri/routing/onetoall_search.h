#pragma once

#include "utl/enumerate.h"
#include "utl/equal_ranges_linear.h"
#include "utl/erase_if.h"
#include "utl/timing.h"
#include "utl/to_vec.h"

#include "nigiri/routing/debug.h"
#include "nigiri/routing/dijkstra.h"
#include "nigiri/routing/for_each_meta.h"
#include "nigiri/routing/get_fastest_direct.h"
#include "nigiri/routing/journey.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/onetoall_query.h"
#include "nigiri/routing/start_times.h"
#include "nigiri/timetable.h"
#include "nigiri/types.h"

namespace nigiri::routing {

    struct onetoall_search_state {
        onetoall_search_state() = default;
        onetoall_search_state(onetoall_search_state const&) = delete;
        onetoall_search_state& operator=(onetoall_search_state const&) = delete;
        onetoall_search_state(onetoall_search_state&&) = default;
        onetoall_search_state& operator=(onetoall_search_state&&) = default;
        ~onetoall_search_state() = default;

        std::vector<start> starts_;
        pareto_set<journey> results_;
    };

    struct onetoall_search_stats {
        std::uint64_t lb_time_{0ULL};
        std::uint64_t search_iterations_{0ULL};
        std::uint64_t interval_extensions_{0ULL};
    };

    template <typename AlgoStats>
    struct routing_result {
        pareto_set<journey> const* journeys_{nullptr};
        interval<unixtime_t> interval_;
        onetoall_search_stats search_stats_;
        AlgoStats algo_stats_;
    };

    template <direction SearchDir, typename Algo>
    struct onetoall_search {
        using algo_state_t = typename Algo::algo_state_t;
        using algo_stats_t = typename Algo::algo_stats_t;
        static constexpr auto const kFwd = (SearchDir == direction::kForward);
        static constexpr auto const kBwd = (SearchDir == direction::kBackward);

        Algo init(algo_state_t& algo_state) {

            UTL_START_TIMING(lb);

            UTL_STOP_TIMING(lb);
            stats_.lb_time_ = static_cast<std::uint64_t>(UTL_TIMING_MS(lb));


            return Algo{
                    tt_,
                    rtt_,
                    algo_state,
                    day_idx_t{std::chrono::duration_cast<date::days>(
                            search_interval_.from_ - tt_.internal_interval().from_)
                                      .count()}};
        }

        onetoall_search(timetable const& tt,
               rt_timetable const* rtt,
               onetoall_search_state& s,
               algo_state_t& algo_state,
               onetoall_query q)
                : tt_{tt},
                  rtt_{rtt},
                  state_{s},
                  q_{std::move(q)},
                  search_interval_{std::visit(
                          utl::overloaded{
                                  [](interval<unixtime_t> const start_interval) {
                                      return start_interval;
                                  },
                                  [](unixtime_t const start_time) {
                                      return interval<unixtime_t>{start_time, start_time};
                                  }},
                          q_.start_time_)},
                  algo_{init(algo_state)} {}

        routing_result<algo_stats_t> execute() {
            state_.results_.clear();

            state_.starts_.clear();
            add_start_labels(q_.start_time_, true);

            while (true) {
                trace("start_time={}\n", search_interval_);

                search_interval();

                if (is_ontrip() || max_interval_reached() ||
                    n_results_in_interval() >= q_.min_connection_count_) {
                    trace(
                            "  finished: is_ontrip={}, max_interval_reached={}, "
                            "extend_earlier={}, extend_later={}, initial={}, interval={}, "
                            "timetable={}, number_of_results_in_interval={}\n",
                            is_ontrip(), max_interval_reached(), q_.extend_interval_earlier_,
                            q_.extend_interval_later_,
                            std::visit(
                                    utl::overloaded{
                                            [](interval<unixtime_t> const& start_interval) {
                                                return start_interval;
                                            },
                                            [](unixtime_t const start_time) {
                                                return interval<unixtime_t>{start_time, start_time};
                                            }},
                                    q_.start_time_),
                            search_interval_, tt_.external_interval(), n_results_in_interval());
                    break;
                } else {
                    trace(
                            "  continue: max_interval_reached={}, extend_earlier={}, "
                            "extend_later={}, initial={}, interval={}, timetable={}, "
                            "number_of_results_in_interval={}\n",
                            max_interval_reached(), q_.extend_interval_earlier_,
                            q_.extend_interval_later_,
                            std::visit(
                                    utl::overloaded{
                                            [](interval<unixtime_t> const& start_interval) {
                                                return start_interval;
                                            },
                                            [](unixtime_t const start_time) {
                                                return interval<unixtime_t>{start_time, start_time};
                                            }},
                                    q_.start_time_),
                            search_interval_, tt_.external_interval(), n_results_in_interval());
                }

                state_.starts_.clear();

                auto const new_interval = interval{
                        q_.extend_interval_earlier_ ? tt_.external_interval().clamp(
                                search_interval_.from_ - 60_minutes)
                                                    : search_interval_.from_,
                        q_.extend_interval_later_
                        ? tt_.external_interval().clamp(search_interval_.to_ + 60_minutes)
                        : search_interval_.to_};
                trace("interval adapted: {} -> {}\n", search_interval_, new_interval);

                if (new_interval.from_ != search_interval_.from_) {
                    add_start_labels(interval{new_interval.from_, search_interval_.from_},
                                     kBwd);
                    if constexpr (kBwd) {
                        trace("dir=BWD, interval extension earlier -> reset state\n");
                        algo_.reset_arrivals();
                        remove_ontrip_results();
                    }
                }

                if (new_interval.to_ != search_interval_.to_) {
                    add_start_labels(interval{search_interval_.to_, new_interval.to_},
                                     kFwd);
                    if constexpr (kFwd) {
                        trace("dir=BWD, interval extension later -> reset state\n");
                        algo_.reset_arrivals();
                        remove_ontrip_results();
                    }
                }

                search_interval_ = new_interval;

                ++stats_.search_iterations_;
            }

            if (is_pretrip()) {
                utl::erase_if(state_.results_, [&](journey const& j) {
                    return !search_interval_.contains(j.start_time_) ||
                           j.travel_time() > kMaxTravelTime;
                });
                utl::sort(state_.results_, [](journey const& a, journey const& b) {
                    return a.start_time_ < b.start_time_;
                });
            }

            return {.journeys_ = &state_.results_,
                    .interval_ = search_interval_,
                    .search_stats_ = stats_,
                    .algo_stats_ = algo_.get_stats()};
        }

    private:
        bool is_ontrip() const {
            return holds_alternative<unixtime_t>(q_.start_time_);
        }

        bool is_pretrip() const { return !is_ontrip(); }

        unsigned n_results_in_interval() const {
            if (holds_alternative<interval<unixtime_t>>(q_.start_time_)) {
                auto count = utl::count_if(state_.results_, [&](journey const& j) {
                    return search_interval_.contains(j.start_time_);
                });
                return static_cast<unsigned>(count);
            } else {
                return static_cast<unsigned>(state_.results_.size());
            }
        }

        bool max_interval_reached() const {
            auto const can_search_earlier =
                    q_.extend_interval_earlier_ &&
                    search_interval_.from_ != tt_.external_interval().from_;
            auto const can_search_later =
                    q_.extend_interval_later_ &&
                    search_interval_.to_ != tt_.external_interval().to_;
            return !can_search_earlier && !can_search_later;
        }

        void add_start_labels(start_time_t const& start_interval,
                              bool const add_ontrip) {
            get_starts(SearchDir, tt_, rtt_, start_interval, q_.start_,
                       q_.start_match_mode_, q_.use_start_footpaths_, state_.starts_,
                       add_ontrip);
        }

        void remove_ontrip_results() {
            utl::erase_if(state_.results_, [&](journey const& j) {
                return !search_interval_.contains(j.start_time_);
            });
        }

        void search_interval() {
            utl::equal_ranges_linear(
                    state_.starts_,
                    [](start const& a, start const& b) {
                        return a.time_at_start_ == b.time_at_start_;
                    },
                    [&](auto&& from_it, auto&& to_it) {
                        algo_.next_start_time();
                        auto const start_time = from_it->time_at_start_;
                        for (auto const& s : it_range{from_it, to_it}) {
                            trace("init: time_at_start={}, time_at_stop={} at {}\n",
                                  s.time_at_start_, s.time_at_stop_, location_idx_t{s.stop_});

                            algo_.add_start(s.stop_, s.time_at_stop_);
                        }

                        auto const worst_time_at_dest =
                                start_time +
                                (kFwd ? 1 : -1) * kMaxTravelTime;
                        algo_.execute(start_time, q_.max_transfers_, worst_time_at_dest,
                                     state_.results_);

                        /*
                        for (auto& j : state_.results_) {
                          if (j.legs_.empty() &&
                              (is_ontrip() || search_interval_.contains(j.start_time_)) &&
                              j.travel_time() < fastest_direct_) {
                            algo_.reconstruct(q_, j);
                          }
                        }
                         */
                    });
        }

        timetable const& tt_;
        rt_timetable const* rtt_;
        onetoall_search_state& state_;
        onetoall_query q_;
        interval<unixtime_t> search_interval_;
        onetoall_search_stats stats_;
        Algo algo_;
    };

}  // namespace nigiri::routing
