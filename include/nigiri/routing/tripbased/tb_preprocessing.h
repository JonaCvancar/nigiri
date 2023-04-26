#pragma once

#include "nigiri/timetable.h"
#include "tb.h"

namespace nigiri::routing::tripbased {

struct tb_preprocessing {

  struct earliest_time {
    constexpr earliest_time(const location_idx_t& location,
                            const duration_t& time,
                            const bitfield_idx_t& bf_idx)
        : location_idx_(location), time_(time), bf_idx_(bf_idx) {}

    location_idx_t location_idx_{};
    duration_t time_{};
    bitfield_idx_t bf_idx_{};
  };

  struct earliest_times {
    earliest_times() = delete;
    explicit earliest_times(tb_preprocessing& tbp) : tbp_(tbp) {}

    bool update(location_idx_t li_new, duration_t time_new, bitfield const& bf);

    constexpr unsigned long size() const noexcept { return data_.size(); }

    constexpr earliest_time& operator[](unsigned long pos) {
      return data_[pos];
    }

    constexpr void clear() noexcept { data_.clear(); }

    tb_preprocessing& tbp_;
    std::vector<earliest_time> data_{};
  };

  tb_preprocessing() = delete;
  explicit tb_preprocessing(timetable& tt, int sa_w_max = 1)
      : tt_(tt), sa_w_max_(sa_w_max) {}

  void build_transfer_set(bool uturn_removal = true, bool reduction = true);

  // load precomputed transfer set from file
  // also needs to load the corresponding timetable from file since bitfields
  // of the transfers are stored in the timetable
  void load_transfer_set(/* file name */);

  // map a bitfield to its bitfield_idx
  // init with bitfields of timetable
  hash_map<bitfield, bitfield_idx_t> bitfield_to_bitfield_idx_{};

  bitfield_idx_t get_or_create_bfi(bitfield const& bf);

  timetable& tt_;
  int const sa_w_max_{};  // look-ahead
  hash_transfer_set ts_{};
};

}  // namespace nigiri::routing::tripbased