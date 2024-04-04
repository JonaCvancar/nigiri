#include "nigiri/routing/onetoall_start_times.h"

#include "nigiri/routing/for_each_meta.h"
#include "nigiri/rt/rt_timetable.h"
#include "nigiri/special_stations.h"
#include "utl/enumerate.h"
#include "utl/get_or_create.h"
#include "utl/overloaded.h"

namespace nigiri::routing {

constexpr auto const kTracing = false;

template <typename... Args>
void trace_start(char const* fmt_str, Args... args) {
  if constexpr (kTracing) {
    fmt::print(std::cout, fmt_str, std::forward<Args&&>(args)...);
  }
}

template <typename Collection, typename Less>
std::pair<typename Collection::iterator, bool> insert_sorted(
    Collection& v, typename Collection::value_type el, Less&& less) {
  using std::begin;
  using std::end;

  auto const it =
      std::lower_bound(begin(v), end(v), el, std::forward<Less>(less));
  if (it == std::end(v) || *it != el) {
    return {v.insert(it, std::move(el)), true};
  } else if( *it == el ){
    it->bitfield_ |= el.bitfield_;
    return {it, true};
  }
  return {it, false};
}

template <typename Less>
bitfield onetoall_add_start_times_at_stop(direction const search_dir,
                             timetable const& tt,
                             rt_timetable const* rtt,
                             route_idx_t const route_idx,
                             stop_idx_t const stop_idx,
                             location_idx_t const location_idx,
                             interval<unixtime_t> const& interval_with_offset,
                             duration_t const offset,
                             std::vector<onetoall_start>& starts,
                                      bool const add_ontrip,
                             Less&& less) {
  bitfield on_trip_days = bitfield();

  auto const first_day_idx = tt.day_idx_mam(interval_with_offset.from_).first;
  auto const last_day_idx = tt.day_idx_mam(interval_with_offset.to_).first;
  trace_start(
      "      add_start_times_at_stop(interval={}) - first_day_idx={}, "
      "last_day_idx={}, date_range={}\n",
      interval_with_offset, first_day_idx, last_day_idx, tt.date_range_);

  auto const& transport_range = tt.route_transport_ranges_[route_idx];
  for (auto t = transport_range.from_; t != transport_range.to_; ++t) {
    auto const& traffic_days =
        rtt == nullptr ? tt.bitfields_[tt.transport_traffic_days_[t]]
                       : rtt->bitfields_[rtt->transport_traffic_days_[t]];
    auto const stop_time =
        tt.event_mam(t, stop_idx,
                     (search_dir == direction::kForward ? event_type::kDep
                                                        : event_type::kArr));

    auto const day_offset =
        static_cast<std::uint16_t>(stop_time.count() / 1440);
    auto const stop_time_mam = duration_t{stop_time.count() % 1440};
    trace_start(
        "      interval=[{}, {}[, transport={}, name={}, stop_time={} "
        "(day_offset={}, stop_time_mam={})\n",
        interval_with_offset.from_, interval_with_offset.to_, t,
        tt.transport_name(t), stop_time, day_offset, stop_time_mam);
    if (interval_with_offset.contains(tt.to_unixtime(first_day_idx, stop_time_mam))) {
      auto const ev_time = tt.to_unixtime(first_day_idx, stop_time_mam);
      auto const [it, inserted] = insert_sorted(
          starts,
          onetoall_start{.time_at_start_ = search_dir == direction::kForward
                                           ? ev_time - offset
                                           : ev_time + offset,
              .time_at_stop_ = ev_time,
              .stop_ = location_idx,
              .bitfield_ = (traffic_days << day_offset)},
          std::forward<Less>(less));

      if(inserted){
        if(add_ontrip) {
          on_trip_days = on_trip_days | (traffic_days << day_offset);
        }
        trace_start(
            "        => ADD START: time_at_start={}, time_at_stop={}, "
            "stop={}, inserted={}\n",
            it->time_at_start_, it->time_at_stop_,
            location{tt, starts.back().stop_}, inserted);
      } else {
        trace_start(
            "        skip: day={}, day_offset={}, date={}, active={}, "
            "in_interval={}\n",
            first_day_idx, day_offset,
            tt.date_range_.from_ + to_idx(first_day_idx - day_offset) * 1_days,
            traffic_days.test(to_idx(first_day_idx)),
            interval_with_offset.contains(
                tt.to_unixtime(first_day_idx, stop_time.as_duration())));
      }
    } else {
      trace_start(
          "        skip: day={}, day_offset={}, date={}, active={}, "
          "in_interval={}\n",
          first_day_idx, day_offset,
          tt.date_range_.from_ + to_idx(first_day_idx - day_offset) * 1_days,
          traffic_days.test(to_idx(first_day_idx)),
          interval_with_offset.contains(
              tt.to_unixtime(first_day_idx, stop_time.as_duration())));
    }
  }

  return on_trip_days;
}

template <typename Less>
void onetoall_add_starts_in_interval(direction const search_dir,
                            timetable const& tt,
                            rt_timetable const* rtt,
                            interval<unixtime_t> const& interval,
                            location_idx_t const l,
                            duration_t const d,
                            std::vector<onetoall_start>& starts,
                            bool const add_ontrip,
                            Less&& cmp) {
  trace_start(
      "    add_starts_in_interval(interval={}, stop={}, duration={}): {} "
      "routes\n",
      interval, location{tt, l},  // NOLINT(clang-analyzer-core.CallAndMessage)
      d, tt.location_routes_.at(l).size());

  // Iterate routes visiting the location.
  for (auto const& r : tt.location_routes_.at(l)) {

    // Iterate the location sequence, searching the given location.
    auto const location_seq = tt.route_location_seq_.at(r);
    trace_start("  location_seq: {}\n", r);
    for (auto const [i, s] : utl::enumerate(location_seq)) {
      auto const stp = stop{s};
      if (stp.location_idx() != l) {
        continue;
      }

      // Ignore:
      // - in-allowed=false for forward search
      // - out-allowed=false for backward search
      // - entering at last stp for forward search
      // - exiting at first stp for backward search
      if ((search_dir == direction::kBackward &&
           (i == 0U || !stp.out_allowed())) ||
          (search_dir == direction::kForward &&
           (i == location_seq.size() - 1 || !stp.in_allowed()))) {
        trace_start("    skip: i={}, out_allowed={}, in_allowed={}\n", i,
                    stp.out_allowed(), stp.in_allowed());
        continue;
      }

      trace_start("    -> no skip -> add_start_times_at_stop()\n");
      bitfield on_trip_days = onetoall_add_start_times_at_stop(
          search_dir, tt, rtt, r, static_cast<stop_idx_t>(i),
          stop{s}.location_idx(),
          search_dir == direction::kForward ? interval + d : interval - d, d,
          starts, add_ontrip, std::forward<Less>(cmp));

      if(add_ontrip) {
        insert_sorted(starts,
                      onetoall_start{.time_at_start_ = search_dir == direction::kForward
                                                           ? interval.to_
                                                           : interval.from_ - 1_minutes,
                                     .time_at_stop_ = search_dir == direction::kForward
                                                          ? interval.to_ + d
                                                          : interval.from_ - 1_minutes - d,
                                     .stop_ = l,
                                     .bitfield_ = on_trip_days},
                      cmp);
      }
    }
  }

  /*
  // Add one earliest arrival query at the end of the interval. This is only
  // used to dominate journeys from the interval that are suboptimal
  // considering a journey from outside the interval (i.e. outside journey
  // departs later and arrives at the same time). These journeys outside the
  // interval will be filtered out before returning the result.
  //trace_start("Length of starts before on_trip(): {}\n", starts.size());
  trace_start("add_ontrip(): time_at_start={}, time_at_Stop= {} at {}\n", search_dir == direction::kForward
        ? interval.to_
        : interval.from_ - 1_minutes,
        search_dir == direction::kForward
        ? interval.to_ + d
        : interval.from_ - 1_minutes - d,
        l);
  bitfield bf_1 = bitfield();
  for(unsigned int bf_idx = 0; bf_idx < bf_1.size(); bf_idx++) {
    bf_1.set(size_t{bf_idx}, true);
  }

  if (add_ontrip) {
    insert_sorted(starts,
                  onetoall_start{.time_at_start_ = search_dir == direction::kForward
                                          ? interval.to_
                                          : interval.from_ - 1_minutes,
                      .time_at_stop_ = search_dir == direction::kForward
                                       ? interval.to_ + d
                                       : interval.from_ - 1_minutes - d,
                      .stop_ = l,
                      .bitfield_ = bf_1},
                  cmp);
  }
  */
  //trace_start("Length of starts after on_trip(): {}\n", starts.size());
}

void onetoall_get_starts(direction const search_dir,
                timetable const& tt,
                rt_timetable const* rtt,
                start_time_t const& start_time,
                std::vector<offset> const& station_offsets,
                location_match_mode const mode,
                bool const use_start_footpaths,
                std::vector<onetoall_start>& starts,
                bool const add_ontrip) {
  hash_map<location_idx_t, duration_t> shortest_start;

  auto const update = [&](location_idx_t const l, duration_t const d) {
    auto& val = utl::get_or_create(shortest_start, l, [d]() { return d; });
    val = std::min(val, d);
  };

  auto const fwd = search_dir == direction::kForward;
  for (auto const& o : station_offsets) {
    for_each_meta(tt, mode, o.target(), [&](location_idx_t const l) {
      update(l, o.duration());
      if (use_start_footpaths) {
        auto const footpaths = fwd ? tt.locations_.footpaths_out_[l]
                                   : tt.locations_.footpaths_in_[l];
        for (auto const& fp : footpaths) {
          update(fp.target(), o.duration() + fp.duration());
        }
      }
    });
  }

  auto const cmp = [&](onetoall_start const& a, onetoall_start const& b) {
    if (fwd) {
      return std::tie(b.time_at_start_, b.time_at_stop_, b.stop_) < std::tie(a.time_at_start_, a.time_at_stop_, a.stop_);
    } else {
      return std::tie(a.time_at_start_, a.time_at_stop_, a.stop_) < std::tie(b.time_at_start_, b.time_at_stop_, b.stop_);
    }
  };

  bitfield bf_1 = bitfield();
  for(unsigned int bf_idx = 0; bf_idx < bf_1.size(); bf_idx++) {
    bf_1.set(size_t{bf_idx}, true);
  }

  for (auto const& s : shortest_start) {
    auto const l = s.first;
    auto const o = s.second;
    std::visit(utl::overloaded{
                   [&](interval<unixtime_t> const interval) {
                     onetoall_add_starts_in_interval(search_dir, tt, rtt, interval, l, o,
                                            starts, add_ontrip, cmp);
                   },
                   [&](unixtime_t const t) {
                     insert_sorted(starts,
                                   onetoall_start{.time_at_start_ = t,
                                       .time_at_stop_ = fwd ? t + o : t - o,
                                       .stop_ = l,
                                       .bitfield_ = bf_1},
                                   cmp);
                   }},
               start_time);
  }

  /*
  for(auto& start : starts) {
    std::cout << "start: " << start.time_at_start_ << ", time_at_stop: " << start.time_at_stop_ << ", stop: " << start.stop_ << "\n";
    std::cout << start.bitfield_ << "\n";
  }
   */
}

}  // namespace nigiri::routing
