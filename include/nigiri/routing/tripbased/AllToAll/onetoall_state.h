#pragma once

#include "nigiri/routing/tripbased/preprocessing/preprocessor.h"
#include "nigiri/routing/tripbased/query/q_n.h"
#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/types.h"

namespace nigiri {
    struct timetable;
}

namespace nigiri::routing::tripbased {

    struct oneToAll_start {
        oneToAll_start(location_idx_t const l, unixtime_t const t)
                : location_(l), time_(t) {}

        location_idx_t location_;
        unixtime_t time_;
    };

    struct oneToAll_state {
        oneToAll_state() = delete;
        oneToAll_state(timetable const& tt, transfer_set const& ts)
                : ts_{ts}, r_{tt}, q_n_{r_} {
            t_min_.resize(tt.locations_.coordinates_.size());
            q_n_.start_.reserve(kNumTransfersMax);
            q_n_.end_.reserve(kNumTransfersMax);
            q_n_.segments_.reserve(10000);
            query_starts_.reserve(20);
        }

        void reset(day_idx_t new_base) {
            //std::fill(t_min_.begin(), t_min_.end(), unixtime_t::max());
            r_.reset();
            q_n_.reset(new_base);
        }

        // transfer set built by preprocessor
        transfer_set const& ts_;


        // reached stops per transport
        reached r_;


        // minimum arrival times per number of transfers
        vecvec<stop_idx_t, unixtime_t> t_min_;


        // queues of transport segments
        q_n q_n_;

        std::vector<oneToAll_start> query_starts_;
    };

}  // namespace nigiri::routing::tripbased