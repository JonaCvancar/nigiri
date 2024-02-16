#include "nigiri/routing/tripbased/AllToAll/onetoall_reached.h"

using namespace nigiri;
using namespace nigiri::routing::tripbased;

void onetoall_reached::reset() {
    for (auto& ps : data_) {
        ps.clear();
    }
}

// ToDo check if operating days needed for reached
void onetoall_reached::update(transport_segment_idx_t const transport_segment_idx,
                     bitfield const operating_days,
                     std::uint16_t const stop_idx,
                     std::uint16_t const n_transfers) {
    // Reached Dominates in add
    auto const route_idx =
        tt_.transport_route_[transport_idx(transport_segment_idx)];

    bool segment_exists = false;
    for (auto const& re : data_[route_idx.v_]) {

    }

    if(!segment_exists) {
      data_[tt_.transport_route_[transport_idx(transport_segment_idx)].v_].add(
          onetoall_reached_entry{transport_segment_idx, operating_days, stop_idx, n_transfers});
    }

}

std::vector<std::tuple<std::uint16_t, bitfield>> onetoall_reached::query(transport_segment_idx_t const transport_segment_idx,
                                      std::uint16_t const n_transfers,
                                      bitfield const operating_days) {
    std::vector<std::tuple<std::uint16_t, bitfield>> res;

    auto const route_idx =
            tt_.transport_route_[transport_idx(transport_segment_idx)];

    auto stop_idx_max =
        static_cast<uint16_t>(tt_.route_location_seq_[route_idx].size() - 1);
    auto stop_idx_min = stop_idx_max;
    // find minimal stop index among relevant entries
    bool reached_found = false;
    for (auto const& re : data_[route_idx.v_]) {
        // only entries with less or equal n_transfers and less or equal
        // transport_segment_idx are relevant
        if( (re.transport_segment_idx_ == transport_segment_idx) && ((operating_days & ~re.operating_days_) > bitfield()) ) {
          res.push_back(std::make_tuple(stop_idx_max, (operating_days & ~re.operating_days_)));
          if(!reached_found) {
            reached_found = true;
          }
        }

        if (re.n_transfers_ <= n_transfers &&
            re.transport_segment_idx_ <= transport_segment_idx &&
            re.stop_idx_ < stop_idx_min) {
          stop_idx_min = re.stop_idx_;
          res.push_back(std::make_tuple(re.stop_idx_, (operating_days & re.operating_days_)));
          if(!reached_found) {
            reached_found = true;
          }
        }
    }

    if(!reached_found) {
      res.push_back(std::make_tuple(stop_idx_max, operating_days));
    }

    return res;
}
