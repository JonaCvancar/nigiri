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
#include "nigiri/routing/journey_bitfield.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/onetoall_query.h"
#include "nigiri/routing/onetoall_start_times.h"
#include "nigiri/timetable.h"
#include "nigiri/types.h"
#include "nigiri/routing/tripbased/dbg.h"

namespace nigiri::routing {

    struct onetoall_search_state {
        onetoall_search_state() = default;
        onetoall_search_state(onetoall_search_state const&) = delete;
        onetoall_search_state& operator=(onetoall_search_state const&) = delete;
        onetoall_search_state(onetoall_search_state&&) = default;
        onetoall_search_state& operator=(onetoall_search_state&&) = default;
        ~onetoall_search_state() = default;

        std::vector<onetoall_start> starts_;

        std::vector<pareto_set<journey_bitfield> > results_;
    };

    struct onetoall_search_stats {
        std::uint32_t n_results_in_interval{0U};
    };

    template <typename AlgoStats>
    struct routing_result_oa {
        std::vector< pareto_set<journey_bitfield>> const* journeys_{nullptr};
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

#ifdef TB_ONETOALL_BITFIELD_IDX
        Algo init(algo_state_t& algo_state) {
            return Algo{
                    tt_,
                    rtt_,
                    algo_state,
                    day_idx_t{std::chrono::duration_cast<date::days>(
                            search_interval_.from_ - tt_.internal_interval().from_)
                                      .count()},
                    bitfield_to_bitfield_idx_};
        }
#else
        Algo init(algo_state_t& algo_state) {
          return Algo{
              tt_,
              rtt_,
              algo_state,
              day_idx_t{std::chrono::duration_cast<date::days>(
                            search_interval_.from_ - tt_.internal_interval().from_)
                            .count()}};
        }
#endif

#ifdef TB_ONETOALL_BITFIELD_IDX
        onetoall_search(timetable& tt,
               rt_timetable const* rtt,
               onetoall_search_state& s,
               algo_state_t& algo_state,
               onetoall_query q)
#else
        onetoall_search(timetable const& tt,
                        rt_timetable const* rtt,
                        onetoall_search_state& s,
                        algo_state_t& algo_state,
                        onetoall_query q)
#endif
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

        routing_result_oa<algo_stats_t> execute() {
            state_.results_.clear();
            state_.results_.resize(tt_.n_locations());

            state_.starts_.clear();
            // checks for lines departing from starts location and adds their start times to state starts
#ifdef TB_OA_ADD_ONTRIP
            add_start_labels(q_.start_time_, true);
#else
            add_start_labels(q_.start_time_, false);
#endif

#ifdef TB_ONETOALL_BITFIELD_IDX
            for (bitfield_idx_t bfi{0U}; bfi < tt_.bitfields_.size();
                 ++bfi) {  // bfi: bitfield index
              bitfield_to_bitfield_idx_.emplace(tt_.bitfields_[bfi], bfi);
            }
#endif

            search_interval();

            if (is_pretrip()) {
              for(auto& stop : state_.results_) {
                utl::erase_if(stop, [&](journey_bitfield const& j) {
                  return !search_interval_.contains(j.start_time_) ||
                         j.travel_time() > kMaxTravelTime;
                });
                utl::sort(stop, [](journey_bitfield const& a, journey_bitfield const& b) {
                  return std::tie(a.start_time_, a.dest_time_) < std::tie(b.start_time_, b.dest_time_);
                });
              }
            }

            stats_.n_results_in_interval = n_results_in_interval();

            rusage r_usage;
            getrusage(RUSAGE_SELF, &r_usage);
            algo_.set_peak_memory(static_cast<double>(r_usage.ru_maxrss) / 1e6);

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
          unsigned count = 0;
          for(unsigned long idx = 0U; idx < state_.results_.size(); idx++) {
            count += state_.results_[idx].size();
          }
            return count;
        }

        void add_start_labels(start_time_t const& start_interval,
                              bool const add_ontrip) {
            onetoall_get_starts(SearchDir, tt_, rtt_, start_interval, q_.start_,
                       q_.start_match_mode_, q_.use_start_footpaths_, state_.starts_,
                       add_ontrip);
        }

        void search_interval() {
            utl::equal_ranges_linear(
                    state_.starts_,
                    [](onetoall_start const& a, onetoall_start const& b) {
                        return a.time_at_start_ == b.time_at_start_;
                    },
                    [&](auto&& from_it, auto&& to_it) {
                        algo_.next_start_time();
                        auto const start_time = from_it->time_at_start_;
                        for (auto const& s : it_range{from_it, to_it}) {
                            trace("init: time_at_start={}, time_at_stop={} at {}\n",
                                  s.time_at_start_, s.time_at_stop_, location_idx_t{s.stop_});

                            algo_.add_start(s.stop_, s.time_at_stop_, s.bitfield_);
                        }

                        auto const worst_time_at_dest =
                                start_time +
                                (kFwd ? 1 : -1) * kMaxTravelTime;
#ifdef TB_ONETOALL_BITFIELD_IDX
                        algo_.execute(start_time, q_.max_transfers_, worst_time_at_dest,
                                     state_.results_, bitfield_to_bitfield_idx_);
#else
                        algo_.execute(start_time, q_.max_transfers_, worst_time_at_dest,
                                      state_.results_);
#endif
                    });
        }

#ifdef TB_ONETOALL_BITFIELD_IDX
        timetable& tt_;
#else
        timetable const& tt_;
#endif
        rt_timetable const* rtt_;
        onetoall_search_state& state_;
        onetoall_query q_;
        interval<unixtime_t> search_interval_;
        onetoall_search_stats stats_;
        Algo algo_;

#ifdef TB_ONETOALL_BITFIELD_IDX
        hash_map<bitfield, bitfield_idx_t> bitfield_to_bitfield_idx_{};
#endif
    };

}  // namespace nigiri::routing
