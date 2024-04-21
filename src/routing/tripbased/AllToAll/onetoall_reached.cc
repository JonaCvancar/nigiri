#include "nigiri/routing/tripbased/AllToAll/onetoall_reached.h"
#include "nigiri/routing/tripbased/dbg.h"

using namespace nigiri;
using namespace nigiri::routing::tripbased;

void onetoall_reached::reset() {
    for (auto& ps : data_) {
        ps.clear();
    }
}

void onetoall_reached::update(transport_segment_idx_t const transport_segment_idx,
                     bitfield const operating_days,
                     std::uint16_t const stop_idx,
                     std::uint16_t const n_transfers) {
    // Reached Dominates in add
#ifdef TB_OA_COLLECT_STATS
      data_[tt_.transport_route_[transport_idx(transport_segment_idx)].v_].add_bitfield(
          onetoall_reached_entry{transport_segment_idx, operating_days, stop_idx, n_transfers});
#else
    data_[tt_.transport_route_[transport_idx(transport_segment_idx)].v_].add_bitfield(
        onetoall_reached_entry{transport_segment_idx, operating_days, stop_idx, n_transfers});
#endif
}

std::tuple<std::vector<std::tuple<std::uint16_t, bitfield>>, uint16_t> onetoall_reached::query(transport_segment_idx_t const transport_segment_idx,
                                      std::uint16_t const n_transfers,
                                      bitfield const operating_days) {
    std::vector<std::tuple<std::uint16_t, bitfield>> res;
    bitfield not_reached_days = operating_days;

#ifdef TB_OA_COLLECT_STATS
    uint16_t n_comparisons = 0U;
#endif

    auto const route_idx =
            tt_.transport_route_[transport_idx(transport_segment_idx)];

    auto stop_idx_max =
        static_cast<uint16_t>(tt_.route_location_seq_[route_idx].size() - 1);
    auto stop_idx_min = stop_idx_max;

    // find minimal stop index among relevant entries
    for (auto const& re : data_[route_idx.v_]) {
      if( (operating_days & re.bitfield_).any() ){
        // only entries with less or equal n_transfers and less or equal
        // transport_segment_idx are relevant
#ifdef TB_OA_COLLECT_STATS
        n_comparisons += 1;
#endif
        if (re.n_transfers_ <= n_transfers &&
            re.transport_segment_idx_ <= transport_segment_idx &&
            re.stop_idx_ < stop_idx_min) {
          bitfield common_days = not_reached_days & re.bitfield_;
          res.emplace_back(re.stop_idx_, common_days);
          not_reached_days = not_reached_days & ~common_days;
        }
      }
      if(not_reached_days.none()) {
        break;
      }
    }

    if( not_reached_days.any() ) {
      res.emplace_back(stop_idx_max, not_reached_days);
    }

#ifdef TB_OA_COLLECT_STATS
    return std::make_tuple(res, n_comparisons);
#else
    return std::make_tuple(res, 0U);
#endif
}
