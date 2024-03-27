#pragma once

#include "nigiri/routing/tripbased/preprocessing/preprocessor.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_q_n.h"
#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/types.h"

namespace nigiri {
    struct timetable;
}

namespace nigiri::routing::tripbased {

    struct oneToAll_start {
      oneToAll_start(location_idx_t const l, unixtime_t const t, bitfield const bf)
          : location_(l), time_(t), bitfield_(bf) {}

        location_idx_t location_;
        unixtime_t time_;
        bitfield bitfield_;
    };

    struct oneToAll_tMin {
      bool dominates(oneToAll_tMin const& o) const {
        return time_ < o.time_;
      }

      void set_bitfield(bitfield bf) {
        bitfield_ = bf;
      }

#if defined(EQUAL_JOURNEY)
      bool equal(oneToAll_tMin const& o ) {
        return time_ == o.time_;
      }
#endif

      unixtime_t time_;
      bitfield bitfield_;
    };

    struct oneToAll_state {
        oneToAll_state() = delete;

#ifdef TB_ONETOALL_BITFIELD_IDX
        oneToAll_state(timetable& tt, transfer_set const& ts)
                : ts_{ts},
              r_{tt},
              q_n_{r_, tt}
#else
        oneToAll_state(timetable const& tt, transfer_set const& ts)
            : ts_{ts},
              r_{tt},
              q_n_{r_}
#endif
        {
            t_min_.resize(tt.n_locations());

            bitfield bf_1 = bitfield();
            for(unsigned int bf_idx = 0; bf_idx < bf_1.size(); bf_idx++) {
              bf_1.set(size_t{bf_idx}, true);
            }

            for(unsigned int i = 0; i < tt.n_locations(); ++i) {
              //t_min_[i].resize(kNumTransfersMax, unixtime_t::max());
              t_min_[i].resize(kNumTransfersMax);
              for(unsigned int j = 0; j < kNumTransfersMax; j++) {
                t_min_[i][j].add_bitfield(oneToAll_tMin{unixtime_t::max(), bf_1});
              }
            }
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

        bool check_if_better_t_min(location_idx_t const loc,
                                   std::uint8_t const n,
                                   unixtime_t const time,
                                   bitfield const operating_days) {
          for(auto& t_min : t_min_[loc.v_][n]) {
            if( (t_min.bitfield_ & operating_days).any() ) {
              if(time < t_min.time_) {
                return true;
              }
            }
          }
          return false;
        }

        // transfer set built by preprocessor
        transfer_set const& ts_;

        // reached stops per transport
        onetoall_reached r_;

        // minimum arrival times per number of transfers
        //std::vector<std::vector<unixtime_t>> t_min_;
        std::vector<std::vector<pareto_set<oneToAll_tMin>>> t_min_;

        // queues of transport segments
        onetoall_q_n q_n_;

        std::vector<oneToAll_start> query_starts_;
    };

}  // namespace nigiri::routing::tripbased