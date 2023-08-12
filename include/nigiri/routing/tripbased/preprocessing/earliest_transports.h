#pragma once

#include "nigiri/types.h"

namespace nigiri::routing::tripbased {

struct earliest_transports {

#ifdef TB_MIN_WALK
  struct earliest_transport {
    std::uint32_t otid_;
    std::uint16_t walk_time_;
    bitfield bf_;
  };

  void update_walk(stop_idx_t j,
                   std::uint32_t otid_new,
                   std::uint16_t walk_time_new,
                   bitfield& bf_new);

  void reset_walk(std::size_t num_stops) noexcept;
#else
  struct earliest_transport {
    std::int8_t shift_amount_;
    std::uint16_t start_time_;
    bitfield bf_;
  };

  void update(stop_idx_t j,
              std::int8_t shift_amount_new,
              std::uint16_t start_time_new,
              bitfield& bf_new);

  void reset(std::size_t num_stops) noexcept;
#endif
  mutable_fws_multimap<std::uint32_t, earliest_transport> transports_;
};

}  // namespace nigiri::routing::tripbased