#pragma once

#include "nigiri/routing/tripbased/preprocessing/preprocessor.h"
#include "nigiri/routing/tripbased/query/q_n.h"
#include "nigiri/routing/tripbased/settings.h"
#include "nigiri/types.h"
#include "reached.h"

namespace nigiri {
struct timetable;
}

namespace nigiri::routing::tripbased {

// a route that reaches the destination
struct route_dest {
  route_dest(std::uint16_t stop_idx, std::uint16_t time)
      : stop_idx_(stop_idx), time_(time) {}
  // the stop index at which the route reaches the target location
  std::uint16_t stop_idx_;
  // the time in it takes after exiting the route until the target location is
  // reached
  std::uint16_t time_;
};

struct query_start {
  query_start(location_idx_t const l, unixtime_t const t)
      : location_(l), time_(t) {}

  location_idx_t location_;
  unixtime_t time_;
};

struct query_state {
  query_state() = delete;
  query_state(timetable const& tt, transfer_set const& ts)
      : ts_{ts}, r_{tt}, q_n_{r_} {
    route_dest_.reserve(128);
    t_min_.resize(kNumTransfersMax, unixtime_t::max());
    q_n_.start_.reserve(kNumTransfersMax);
    q_n_.end_.reserve(kNumTransfersMax);
    q_n_.segments_.reserve(10000);
    query_starts_.reserve(20);
    route_dest_.resize(tt.n_routes());
  }

  void reset(day_idx_t new_base) {
    std::fill(t_min_.begin(), t_min_.end(), unixtime_t::max());
    r_.reset();
    q_n_.reset(new_base);
    for (auto& inner_vec : route_dest_) {
      inner_vec.clear();
    }
  }

  // transfer set built by preprocessor
  transfer_set const& ts_;

  // routes that reach the target stop
  std::vector<std::vector<route_dest>> route_dest_;

  // reached stops per transport
  reached r_;

  // minimum arrival times per number of transfers
  std::vector<unixtime_t> t_min_;

  // queues of transport segments
  q_n q_n_;

  std::vector<query_start> query_starts_;
};

}  // namespace nigiri::routing::tripbased