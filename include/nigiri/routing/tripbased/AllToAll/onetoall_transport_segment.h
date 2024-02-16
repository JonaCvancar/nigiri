#pragma once

#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/types.h"
#include "nigiri/routing/tripbased/query/transport_segment.h"

#define TRANSFERRED_FROM_NULL std::numeric_limits<std::uint32_t>::max()

namespace nigiri {
    struct timetable;
}  // namespace nigiri

namespace nigiri::routing::tripbased {

    struct onetoall_transport_segment {
        onetoall_transport_segment(transport_segment_idx_t transport_segment_idx,
                          stop_idx_t stop_idx_start,
                          stop_idx_t stop_idx_end,
                          std::uint32_t transferred_from,
                          bitfield operating_days)
                : transport_segment_idx_(transport_segment_idx),
                  stop_idx_start_(stop_idx_start),
                  stop_idx_end_(stop_idx_end),
                  operating_days_(operating_days),
                  transferred_from_(transferred_from)
                  {}

        day_idx_t get_transport_day(day_idx_t const base) const {
            return transport_day(base, transport_segment_idx_);
        }

        transport_idx_t get_transport_idx() const {
            return transport_idx(transport_segment_idx_);
        }

        stop_idx_t get_stop_idx_start() const {
            return static_cast<stop_idx_t>(stop_idx_start_);
        }

        stop_idx_t get_stop_idx_end() const {
            return static_cast<stop_idx_t>(stop_idx_end_);
        }

        void print(std::ostream&, timetable const&) const;

        // store day offset of the instance in upper bits of transport idx
        transport_segment_idx_t transport_segment_idx_;

        std::uint32_t stop_idx_start_ : STOP_IDX_BITS;
        std::uint32_t stop_idx_end_ : STOP_IDX_BITS;

        // Operating Days of trip segment
        bitfield operating_days_;

        // queue index of the segment from which we transferred to this segment
        std::uint32_t transferred_from_;

#ifdef TB_CACHE_PRESSURE_REDUCTION
        union {
    unixtime_t time_prune_;
    bool no_prune_;
  };
#endif
    };

}  // namespace nigiri::routing::tripbased