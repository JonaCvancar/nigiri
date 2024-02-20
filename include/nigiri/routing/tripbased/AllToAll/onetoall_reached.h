#pragma once

#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_transport_segment.h"
#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/timetable.h"
#include "nigiri/types.h"

namespace nigiri::routing::tripbased {

    struct onetoall_reached_entry {
        std::tuple<bitfield, bitfield> dominates(onetoall_reached_entry const& o) const {
          bitfield different_days{};
          bitfield same_days = (o.bitfield_ & bitfield_);
          if( same_days == bitfield() ) {
            return std::make_tuple(o.bitfield_, bitfield_);
          } else {
            different_days = o.bitfield_ & ~bitfield_;
          }

          bool dominates = false;
          if(transport_segment_idx_ < o.transport_segment_idx_) {
            dominates = stop_idx_ <= o.stop_idx_ && n_transfers_ <= o.n_transfers_;
          } else if(stop_idx_ < o.stop_idx_) {
            dominates = transport_segment_idx_ <= o.transport_segment_idx_ && n_transfers_ <= o.n_transfers_;
          }else if(n_transfers_ < o.n_transfers_) {
            dominates = transport_segment_idx_ <= o.transport_segment_idx_ &&
                                          stop_idx_ <= o.stop_idx_;
          }

          if(dominates) {
            return std::make_tuple(different_days, bitfield_);
          } else {
            return std::make_tuple(o.bitfield_, bitfield_ & ~same_days);
          }
        }
        transport_segment_idx_t transport_segment_idx_;
        bitfield bitfield_;
        std::uint16_t stop_idx_;
        std::uint16_t n_transfers_;
    };

    struct onetoall_reached {
        onetoall_reached() = delete;
        explicit onetoall_reached(timetable const& tt) : tt_(tt) {
            data_.resize(tt.n_routes());
        }

        void reset();

        void update(transport_segment_idx_t const,
                    bitfield const operating_days,
                    std::uint16_t const stop_idx,
                    std::uint16_t const n_transfers);

        std::vector<std::tuple<std::uint16_t, bitfield>> query(transport_segment_idx_t const,
                            std::uint16_t const n_transfers,
                            bitfield const operating_days);

        timetable const& tt_;

        // reached stops per route
        std::vector<pareto_set<onetoall_reached_entry>> data_;
    };

}  // namespace nigiri::routing::tripbased
