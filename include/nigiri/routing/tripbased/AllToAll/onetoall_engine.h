#pragma once

#include <cinttypes>

//#include "boost/functional/hash.hpp"
//#include "nigiri/routing/tripbased/preprocessing/preprocessor.h"
//#include "nigiri/routing/tripbased/preprocessing/expanded_transfer.h"
//#include "nigiri/routing/tripbased/preprocessing/ordered_transport_id.h"
//#include "nigiri/routing/tripbased/preprocessing/reached_line_based.h"
//#include "nigiri/routing/tripbased/settings.h"
//#include "nigiri/routing/tripbased/transfer.h"
//#include "nigiri/routing/tripbased/transfer_set.h"
//#include "nigiri/timetable.h"

#include "nigiri/routing/journey_bitfield.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/query.h"
#include "nigiri/routing/tripbased/dbg.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_state.h"

namespace nigiri {
    struct timetable;
}  // namespace nigiri

namespace nigiri::routing::tripbased {

    struct oneToAll_stats {
        bool  equal_journey_{false};
        bool  tb_queue_handling_{false};
        bool  tb_onetoall_bitfield_idx_{false};
        bool  tb_oa_check_previous_n_{false};
        bool  tb_oa_collect_stats_{false};
        std::uint64_t n_largest_queue_size{0U};
        std::uint64_t n_segments_enqueued_{0U};
        std::uint64_t n_segments_pruned_{0U};
        std::uint64_t n_enqueue_prevented_by_reached_{0U};
        std::uint64_t n_journeys_found_{0U};
        std::uint64_t empty_n_{0U};

#ifdef TB_OA_COLLECT_STATS
        std::uint64_t n_reached_comparisons_{0U};
        std::uint64_t n_tmin_comparisons_{0U};
        std::uint64_t n_results_comparisons_{0U};
#endif

        bool max_transfers_reached_{false};
        double pre_memory_usage_ = 0;
        double peak_memory_usage_ = 0;
    };

    struct oneToAll_engine {
        using algo_state_t = oneToAll_state;
        using algo_stats_t = oneToAll_stats;

        static constexpr bool kUseLowerBounds = false;

#ifdef TB_ONETOALL_BITFIELD_IDX
        oneToAll_engine(timetable& tt,
                        rt_timetable const* rtt,
                        oneToAll_state& state,
                        day_idx_t const base,
                        hash_map<bitfield, bitfield_idx_t>& bitfieldMap);
#else
        oneToAll_engine(timetable const& tt,
                        rt_timetable const* rtt,
                        oneToAll_state& state,
                        day_idx_t const base);
#endif

        algo_stats_t get_stats() const { return stats_; }

        void set_peak_memory(double peak_memory) { stats_.peak_memory_usage_ = peak_memory; }

        algo_state_t& get_state() { return state_; }

        void reset_arrivals() {
#ifndef NDEBUG
            TBDL << "reset_arrivals\n";
#endif
            state_.r_.reset();
            //std::fill(state_.t_min_.begin(), state_.t_min_.end(), unixtime_t::max());
        }

        void next_start_time() {
#ifndef NDEBUG
            TBDL << "next_start_time\n";
#endif
            state_.q_n_.reset(base_);
            state_.query_starts_.clear();
        }

        void add_start(location_idx_t const l, unixtime_t const t, bitfield const bf) {
/*#ifndef NDEBUG
            TBDL << "add_start: " << tt_.locations_.names_.at(l).view() << ", "
                 << dhhmm(unix_tt(tt_, t)) << "\n";
#endif*/
            state_.query_starts_.emplace_back(l, t, bf);
        }

#ifdef TB_ONETOALL_BITFIELD_IDX
        void execute(unixtime_t const start_time,
                     std::uint8_t const max_transfers,
                     unixtime_t const worst_time_at_dest,
                     std::vector<pareto_set<journey_bitfield>>& results,
                     hash_map<bitfield, bitfield_idx_t>& bitfieldMap
                     );
#else
        void execute(unixtime_t const start_time,
                     std::uint8_t const max_transfers,
                     unixtime_t const worst_time_at_dest,
                     std::vector<pareto_set<journey_bitfield>>& results
        );
#endif

    private:
        void handle_start(unixtime_t const start_time,
                        oneToAll_start const&,
                        unixtime_t const,
                        std::vector<pareto_set<journey_bitfield>>&);

        void add_start_footpath_journey(unixtime_t const start_time, std::int32_t const, std::int32_t const, footpath const, std::vector<pareto_set<journey_bitfield>>&);

        void handle_start_footpath(std::int32_t const,
                                   std::int32_t const,
                                   bitfield const,
                                   footpath const,
                                   unixtime_t const);

        void handle_segment(unixtime_t const start_time,
                            unixtime_t const worst_time_at_dest,
                            std::vector<pareto_set<journey_bitfield>>& results,
                            std::uint8_t const n,
                            queue_idx_t const q_cur);

        void add_segment_leg(journey_bitfield& j, transport_segment const& seg) const;

        // reconstruct the transfer from the given segment to the last journey leg
        // returns the stop idx at which the segment is exited
        std::optional<transport_segment> reconstruct_transfer(
                journey_bitfield& j, transport_segment const& seg_next, std::uint8_t n) const;

        void add_initial_footpath(query const& q, journey_bitfield& j) const;

        bool is_start_location(query const&, location_idx_t const) const;

#ifdef TB_ONETOALL_BITFIELD_IDX
        timetable& tt_;
#else
        timetable const& tt_;
#endif
        rt_timetable const* rtt_;
        oneToAll_state& state_;
        day_idx_t const base_;
        oneToAll_stats stats_;
#ifdef TB_ONETOALL_BITFIELD_IDX
        hash_map<bitfield, bitfield_idx_t>& bitfield_to_bitfield_idx_;
#endif
    };
}
