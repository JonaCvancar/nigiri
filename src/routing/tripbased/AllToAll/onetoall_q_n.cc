#include "nigiri/routing/tripbased/AllToAll/onetoall_q_n.h"
#include "nigiri/routing/tripbased/dbg.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_transport_segment.h"
#include "nigiri/routing/tripbased/settings.h"

#include "utl/get_or_create.h"

using namespace nigiri;
using namespace nigiri::routing::tripbased;

void onetoall_q_n::reset(day_idx_t new_base) {
/*#ifndef NDEBUG
    TBDL << "Resetting transport segment queue\n";
#endif*/
  base_ = new_base;
  start_.clear();
  start_.emplace_back(0U);
  end_.clear();
  end_.emplace_back(0U);
  segments_.clear();
}

#ifdef TB_ONETOALL_BITFIELD_IDX
bitfield_idx_t onetoall_q_n::get_or_create_bfi(bitfield const& bf) {
  return utl::get_or_create(*bitfield_to_bitfield_idx_, bf, [&bf, this]() {
    auto const bfi = tt_.register_bitfield(bf);
    bitfield_to_bitfield_idx_->emplace(bf, bfi);
    return bfi;
  });
}
#endif

bool onetoall_q_n::enqueue(std::uint16_t const transport_day,
                           transport_idx_t const transport_idx,
                           std::uint16_t const stop_idx,
                           std::uint16_t const n_transfers,
                           std::uint32_t const transferred_from,
                           bitfield operating_days,
                           std::vector<std::string_view> trip_names)
{
  assert(segments_.size() < std::numeric_limits<queue_idx_t>::max());
  assert(base_.has_value());

  auto const day_offset = compute_day_offset(base_->v_, transport_day);
  // query day has offset = 5, we disregard segments with offset > 6, since we
  // are only interested in journeys with max. 24h travel time
  if (0 <= day_offset && day_offset < 7) {

    // compute transport segment index
    auto const transport_segment_idx =
        embed_day_offset(day_offset, transport_idx);

    // look-up the earliest stop index reached R(L)-LookUp
    auto const r_query_res = r_.query(transport_segment_idx, n_transfers, operating_days);
    for(const auto& tuple : r_query_res) {
      if (stop_idx < std::get<0>(tuple)) {
        // new n?
        if (n_transfers == start_.size()) {
          start_.emplace_back(segments_.size());
          end_.emplace_back(segments_.size());
        }

        if(std::get<1>(tuple).any()) {
          // add transport segment
#ifdef TB_ONETOALL_BITFIELD_IDX
          auto idx = get_or_create_bfi(std::get<1>(tuple));

          segments_.emplace_back(transport_segment_idx, stop_idx, std::get<0>(tuple),
                                 transferred_from, idx, trip_names);
#else
          segments_.emplace_back(transport_segment_idx, stop_idx, std::get<0>(tuple),
                                 transferred_from, std::get<1>(tuple));
#endif

#ifndef NDEBUG
              TBDL << "Enqueued transport segment: ";
              print(TBDL, static_cast<queue_idx_t>(segments_.size() - 1));
#endif
          // increment index
          ++end_[n_transfers];

          // update reached
          r_.update(transport_segment_idx, std::get<1>(tuple), stop_idx,
                    n_transfers);
          //return true;
        }
      }
    }
    if(!r_query_res.empty()) {
      return true;
    }
  }
  return false;
}

void onetoall_q_n::print(std::ostream& out, queue_idx_t const q_idx) {
  out << "q_idx: " << std::to_string(q_idx) << ", segment of ";
  segments_[q_idx].print(out, r_.tt_);
}
