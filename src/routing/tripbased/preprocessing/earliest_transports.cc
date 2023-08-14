#include "nigiri/routing/tripbased/settings.h"

#ifdef TB_PREPRO_LB_PRUNING
#include "nigiri/routing/tripbased/preprocessing/earliest_transports.h"

#ifdef TB_MIN_WALK
#include "nigiri/routing/tripbased/preprocessing/dominates.h"
#endif

using namespace nigiri;
using namespace nigiri::routing::tripbased;

#ifdef TB_MIN_WALK
void earliest_transports::update_walk(stop_idx_t j,
                                      std::uint32_t otid_new,
                                      std::uint16_t walk_time_new,
                                      bitfield& bf_new) {
  for (auto& entry : transports_[j]) {
    if (bf_new.none()) {
      break;
    }
    if (entry.bf_.any()) {
      auto const dom =
          dominates(otid_new, walk_time_new, entry.otid_, entry.walk_time_);
      if (dom < 0) {
        entry.bf_ &= ~bf_new;
      } else if (!(otid_new < entry.otid_ ||
                   walk_time_new < entry.walk_time_)) {
        bf_new &= ~entry.bf_;
      }
    }
  }
}

void earliest_transports::reset_walk(std::size_t num_stops) noexcept {
  transports_.clear();
  for (stop_idx_t j = 0; j < num_stops; ++j) {
    transports_[j].emplace_back(
        max_otid(), std::numeric_limits<std::uint16_t>::max(), bitfield::max());
  }
}
#else
void earliest_transports::update(stop_idx_t j,
                                 std::int8_t shift_amount_new,
                                 std::uint16_t start_time_new,
                                 bitfield& bf_new) {
  for (auto& entry : transports_[j]) {
    if (bf_new.none()) {
      break;
    }
    if (entry.bf_.any()) {
      if (shift_amount_new < entry.shift_amount_ ||
          (shift_amount_new == entry.shift_amount_ &&
           start_time_new < entry.start_time_)) {
        entry.bf_ &= ~bf_new;
      } else {
        bf_new &= ~entry.bf_;
      }
    }
  }
}

void earliest_transports::reset(std::size_t num_stops) noexcept {
  transports_.clear();
  for (stop_idx_t j = 0; j < num_stops; ++j) {
    transports_[j].emplace_back(std::numeric_limits<std::int8_t>::max(),
                                std::numeric_limits<std::uint16_t>::max(),
                                bitfield::max());
  }
}
#endif

#endif