#pragma once

#include <cinttypes>
#include <iosfwd>
#include <variant>
#include <vector>

#include "nigiri/common/interval.h"
#include "nigiri/footpath.h"
#include "nigiri/routing/query.h"
#include "nigiri/rt/run.h"
#include "nigiri/types.h"

#include "nigiri/routing/tripbased/settings.h"

namespace nigiri {
struct timetable;
struct rt_timetable;
}  // namespace nigiri

namespace nigiri::routing {

struct journey {
  struct run_enter_exit {
    run_enter_exit(rt::run r, stop_idx_t const a, stop_idx_t const b)
        : r_{std::move(r)},
          stop_range_{std::min(a, b),
                      static_cast<stop_idx_t>(std::max(a, b) + 1U)} {}
    rt::run r_;
    interval<stop_idx_t> stop_range_;
  };

  struct leg {
    template <typename T>
    leg(direction const d,
        location_idx_t const a,
        location_idx_t const b,
        unixtime_t const tima_at_a,
        unixtime_t const time_at_b,
        T&& uses)
        : from_{d == direction::kForward ? a : b},
          to_{d == direction::kForward ? b : a},
          dep_time_{d == direction::kForward ? tima_at_a : time_at_b},
          arr_time_{d == direction::kForward ? time_at_b : tima_at_a},
          uses_{std::forward<T>(uses)} {}

    void print(std::ostream&,
               timetable const&,
               rt_timetable const* = nullptr,
               unsigned n_indent = 0U,
               bool debug = false) const;

    location_idx_t from_, to_;
    unixtime_t dep_time_, arr_time_;
    std::variant<run_enter_exit, footpath, offset> uses_;
    bitfield_idx_t bitfield_idx_;
  };

#ifdef TB_MIN_WALK
  bool dominates(journey const& o) const {
    if (start_time_ <= dest_time_) {
      return transfers_ <= o.transfers_ && start_time_ >= o.start_time_ &&
             dest_time_ <= o.dest_time_ && time_walk_ <= o.time_walk_;
    } else {
      return transfers_ <= o.transfers_ && start_time_ <= o.start_time_ &&
             dest_time_ >= o.dest_time_ && time_walk_ <= o.time_walk_;
    }
  }
#elifdef TB_TRANSFER_CLASS
  bool dominates(journey const& o) const {
    if (start_time_ <= dest_time_) {
      return transfers_ <= o.transfers_ && start_time_ >= o.start_time_ &&
             dest_time_ <= o.dest_time_ &&
             transfer_class_max_ <= o.transfer_class_max_ &&
             transfer_class_sum_ <= o.transfer_class_sum_;
    } else {
      return transfers_ <= o.transfers_ && start_time_ <= o.start_time_ &&
             dest_time_ >= o.dest_time_ &&
             transfer_class_max_ <= o.transfer_class_max_ &&
             transfer_class_sum_ <= o.transfer_class_sum_;
    }
  }
#else
  bool dominates(journey const& o) const {
    if (start_time_ <= dest_time_) {
      return transfers_ <= o.transfers_ && start_time_ >= o.start_time_ &&
             dest_time_ <= o.dest_time_;
    } else {
      return transfers_ <= o.transfers_ && start_time_ <= o.start_time_ &&
             dest_time_ >= o.dest_time_;
    }
  }
#endif

  void add(leg&& l) { legs_.emplace_back(l); }

  duration_t travel_time() const {
    return duration_t{std::abs((dest_time_ - start_time_).count())};
  }

  void print(std::ostream&,
             timetable const&,
             rt_timetable const* = nullptr,
             bool debug = false) const;

  std::vector<leg> legs_;
  unixtime_t start_time_;
  unixtime_t dest_time_;
  location_idx_t dest_;
  std::uint8_t transfers_{0U};
#ifdef TB_MIN_WALK
  std::uint16_t time_walk_{0U};
#elifdef TB_TRANSFER_CLASS
  std::uint8_t transfer_class_max_{0U};
  std::uint8_t transfer_class_sum_{0U};
#endif
};

}  // namespace nigiri::routing
