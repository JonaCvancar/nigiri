#pragma once

#include <chrono>
#include <set>
#include <vector>

#include "cista/reflection/comparable.h"

#include "nigiri/routing/query.h"
#include "nigiri/types.h"

namespace nigiri {
struct timetable;
struct rt_timetable;
}  // namespace nigiri

namespace nigiri::routing {

struct onetoall_start {
  // Overload the == operator
  bool operator==(const onetoall_start& other) const {
    return (time_at_start_ == other.time_at_start_) &&
           (time_at_stop_ == other.time_at_stop_) &&
           (stop_ == other.stop_);
  }

  // Overload the != operator
  bool operator!=(const onetoall_start& other) const {
    return !(*this == other);
  }

  //ISTA_FRIEND_COMPARABLE(onetoall_start)
  unixtime_t time_at_start_;
  unixtime_t time_at_stop_;
  location_idx_t stop_;
  bitfield bitfield_;
};

void onetoall_get_starts(direction,
                timetable const&,
                rt_timetable const*,
                start_time_t const& start_time,
                std::vector<offset> const& station_offsets,
                location_match_mode,
                bool use_start_footpaths,
                std::vector<onetoall_start>&,
                bool add_ontrip);

}  // namespace nigiri::routing
