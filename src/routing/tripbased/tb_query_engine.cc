#include <ranges>

#include "nigiri/routing/tripbased/tb_query_engine.h"
#include "nigiri/special_stations.h"

using namespace nigiri;
using namespace nigiri::routing::tripbased;

void tb_query_engine::execute(unixtime_t const start_time,
                              std::uint8_t const max_transfers,
                              unixtime_t const worst_time_at_dest,
                              pareto_set<journey>& results) {

  auto const day_idx_mam_pair = tt_.day_idx_mam(state_.start_time_);
  // day index of start day
  auto const d = day_idx_mam_pair.first;
  // minutes after midnight on the start day
  auto const tau = day_idx_mam_pair.second;

  // fill Q_0
  auto create_q0_entry = [&tau, this, &d](footpath const& fp) {
    // arrival time after walking the footpath
    auto const alpha = delta{tau + fp.duration()};
    // iterate routes at target stop of footpath
    for (auto const route_idx : tt_.location_routes_[fp.target()]) {
      // iterate stop sequence of route, skip last stop
      for (std::uint16_t i = 0U;
           i < tt_.route_location_seq_[route_idx].size() - 1; ++i) {
        auto const q =
            stop{tt_.route_location_seq_[route_idx][i]}.location_idx();
        if (q == fp.target()) {
          // departure times of this route at this q
          auto const event_times =
              tt_.event_times_at_stop(route_idx, i, event_type::kDep);
          // iterator to departure time of connecting transport at this
          // q
          auto tau_dep_t_i = std::lower_bound(
              event_times.begin(), event_times.end(), alpha,
              [&](auto&& x, auto&& y) { return x.mam() < y.mam(); });
          // shift amount due to walking the footpath
          auto sigma = day_idx_t{alpha.days()};
          // no departure found on the day of alpha
          if (tau_dep_t_i == event_times.end()) {
            // start looking at the following day
            ++sigma;
            tau_dep_t_i = event_times.begin();
          }
          // iterate departures until maximum waiting time is reached
          while (sigma <= 1) {
            // shift amount due to travel time of transport
            auto const sigma_t = day_idx_t{tau_dep_t_i->days()};
            // day index of the transport segment
            auto const d_seg = d + sigma - sigma_t;
            // offset of connecting transport in route_transport_ranges
            auto const k = static_cast<std::size_t>(
                std::distance(event_times.begin(), tau_dep_t_i));
            // transport_idx_t of the connecting transport
            auto const t = tt_.route_transport_ranges_[route_idx][k];
            // bitfield of the connecting transport
            auto const& beta_t = tt_.bitfields_[tt_.transport_traffic_days_[t]];

            if (beta_t.test(d_seg.v_)) {
              // enqueue segment if alpha matching bit is found
              state_.q_.enqueue(d_seg, t, i, 0U, TRANSFERRED_FROM_NULL);
              break;
            }
            // passing midnight?
            if (tau_dep_t_i + 1 == event_times.end()) {
              ++sigma;
              // start with the earliest transport on the next day
              tau_dep_t_i = event_times.begin();
            } else {
              ++tau_dep_t_i;
            }
          }
        }
      }
    }
  };
  // virtual reflexive footpath
  create_q0_entry(footpath{state_.start_location_, duration_t{0U}});
  // iterate outgoing footpaths of source location
  for (auto const fp_q :
       tt_.locations_.footpaths_out_[state_.start_location_]) {
    create_q0_entry(fp_q);
  }

  // process all Q_n in ascending order, i.e., transport segments reached after
  // n transfers
  for (std::uint8_t n = 0U; n != state_.q_.start_.size() && n <= max_transfers;
       ++n) {
    // iterate trip segments in Q_n
    for (auto q_cur = state_.q_.start_[n]; q_cur != state_.q_.end_[n];
         ++q_cur) {
      // the current transport segment
      auto seg = state_.q_[q_cur];
      // departure time at the start of the transport segment
      auto const tau_dep_t_b_delta = tt_.event_mam(
          seg.get_transport_idx(), seg.stop_idx_start_, event_type::kDep);
      // the day index of the segment
      auto const d_seg = seg.get_transport_day(base_).v_;
      // departure time at start of current transport segment in minutes after
      // midnight on the day of this earliest arrival query
      auto const tau_d =
          duration_t{(d_seg +
                      static_cast<std::uint16_t>(tau_dep_t_b_delta.days()) -
                      d.v_) *
                     1440U} +
          duration_t{tau_dep_t_b_delta.mam()};

      // check if target location is reached from current transport segment
      for (auto const& le : state_.l_) {
        if (le.route_idx_ == tt_.transport_route_[seg.get_transport_idx()] &&
            seg.stop_idx_start_ < le.stop_idx_ &&
            le.stop_idx_ <= seg.stop_idx_end_) {
          // the time it takes to travel on this transport segment
          auto const travel_time_seg =
              tt_.event_mam(seg.get_transport_idx(), le.stop_idx_,
                            event_type::kArr) -
              tau_dep_t_b_delta;
          // the time at which the target location is reached by using the
          // current transport segment
          auto const t_cur = tt_.to_unixtime(
              d, tau_d + travel_time_seg.as_duration() + le.time_);
          if (t_cur < state_.t_min_[n]) {
            state_.t_min_[n] = t_cur;
            // add journey without reconstructing yet
            journey j{};
            j.start_time_ = start_time;
            j.dest_time_ = t_cur;
            j.dest_ = stop{tt_.route_location_seq_[le.route_idx_][le.stop_idx_]}
                          .location_idx();
            j.transfers_ = n;
            // add journey to pareto set (removes dominated entries)
            results.add(std::move(j));
          }
        }
      }

      // the time it takes to travel to the next stop of the transport segment
      auto const travel_time_next =
          tt_.event_mam(seg.get_transport_idx(), seg.stop_idx_start_ + 1,
                        event_type::kArr) -
          tau_dep_t_b_delta;

      // the unix time at the next stop of the transport segment
      auto const unix_time_next =
          tt_.to_unixtime(d, tau_d + travel_time_next.as_duration());

      // transfer out of current transport segment?
      if (unix_time_next < state_.t_min_[n] &&
          unix_time_next < worst_time_at_dest) {
        // iterate stops of the current transport segment
        for (auto i{seg.stop_idx_start_ + 1U}; i <= seg.stop_idx_end_; ++i) {
          // get transfers for this transport/stop
          auto const& transfers =
              state_.ts_.data_.at(seg.get_transport_idx().v_, i);
          // iterate transfers from this stop
          for (auto const& transfer_cur : transfers) {
            // bitset specifying the days on which the transfer is possible
            // from the current transport segment
            auto const& theta = tt_.bitfields_[transfer_cur.get_bitfield_idx()];
            // enqueue if transfer is possible
            if (theta.test(d_seg)) {
              // arrival time at start location of transfer
              auto const tau_arr_t_i_delta =
                  tt_.event_mam(seg.get_transport_idx(), i, event_type::kArr);
              // departure time at end location of transfer
              auto const tau_dep_u_j_delta =
                  tt_.event_mam(transfer_cur.get_transport_idx_to(),
                                transfer_cur.stop_idx_to_, event_type::kDep);

              auto const d_tr = seg.get_transport_day(base_) +
                                day_idx_t{tau_arr_t_i_delta.days()} -
                                day_idx_t{tau_dep_u_j_delta.days()} +
                                transfer_cur.get_passes_midnight();
              state_.q_.enqueue(d_tr, transfer_cur.get_transport_idx_to(),
                                transfer_cur.get_stop_idx_to(), n + 1U, q_cur);
            }
          }
        }
      }
    }
  }
}

void tb_query_engine::reconstruct(query const& q, journey& j) const {
  TBDL << "Beginning reconstruction of journey:\n";
  j.print(std::cout, tt_, true);

  // find journey end
  auto je = reconstruct_journey_end(q, j);
  if (!je.has_value()) {
    std::cerr
        << "Journey reconstruction failed: Could not find a journey end\n";
    return;
  }

  if (q.dest_match_mode_ == location_match_mode::kIntermodal) {
    // set destination to END if routing to intermodal
    j.dest_ = get_special_station(special_station::kEnd);
  } else {
    // set reconstructed destination if routing to station
    j.dest_ = je->last_location_;
  }

  // add final footpath
  add_final_footpath(q, j, je.value());

  // init current segment with last segment
  auto seg = state_.q_.segments_[je->seg_idx_];

  // set end stop of current segment according to l_entry
  seg.stop_idx_end_ = je->le_.stop_idx_;

  // journey leg for last segment
  add_segment_leg(j, seg);

  // add segments and transfers until first segment is reached
  while (seg.transferred_from_ != TRANSFERRED_FROM_NULL) {
    // get previous segment
    seg = state_.q_.segments_[seg.transferred_from_];

    // reconstruct transfer to following segment
    auto const stop_idx_exit = reconstruct_transfer(j, seg);
    if (!stop_idx_exit.has_value()) {
      std::cerr
          << "Journey reconstruction failed: Could not reconstruct transfer\n";
    }

    // set end stop of current segment to stop idx from reconstruct_transfer
    seg.stop_idx_end_ = stop_idx_exit.value();

    // add journey leg for current segment
    add_segment_leg(j, seg);
  }

  // add initial footpath
  add_initial_footpath(q, j);

  // reverse order of journey legs
  std::reverse(j.legs_.begin(), j.legs_.end());
}

std::optional<tb_query_engine::journey_end>
tb_query_engine::reconstruct_journey_end(query const& q,
                                         journey const& j) const {
  TBDL << "Attempting to reconstruct journey end\n";

  // iterate transport segments in queue with matching number of transfers
  for (auto q_cur = state_.q_.start_[j.transfers_];
       q_cur != state_.q_.end_[j.transfers_]; ++q_cur) {
    // the transport segment currently processed
    auto const& tp_seg = state_.q_[q_cur];

    TBDL << "Examining segment"

        // the route index of the current segment
        auto const route_idx = tt_.transport_route_[tp_seg.get_transport_idx()];

    // find matching entry in l_
    for (auto const& le : state_.l_) {
      // check if route and stop indices match
      if (le.route_idx_ == route_idx && tp_seg.stop_idx_start_ < le.stop_idx_ &&
          le.stop_idx_ <= tp_seg.stop_idx_end_) {
        // transport day of the segment
        auto const transport_day = tp_seg.get_transport_day(base_);
        // transport time at destination
        auto const transport_time =
            tt_.event_mam(tp_seg.get_transport_idx(), le.stop_idx_,
                          event_type::kArr)
                .as_duration() +
            le.time_;
        // unix_time of segment at destination
        auto const seg_unix_dest =
            tt_.to_unixtime(transport_day, transport_time);
        // check if time at destination matches
        if (seg_unix_dest == j.dest_time_) {
          // the location specified by the l_entry
          auto const le_location_idx =
              stop{tt_.route_location_seq_[le.route_idx_][le.stop_idx_]}
                  .location_idx();

          // to station
          if (q.dest_match_mode_ != location_match_mode::kIntermodal) {
            if (le.time_ == duration_t{0} && is_dest_[le_location_idx.v_]) {
              // either the location of the l_entry is itself a destination
              return journey_end{q_cur, le, le_location_idx, le_location_idx};
            } else {
              // or there exists a footpath with matching duration to a
              // destination
              for (auto const& fp :
                   tt_.locations_.footpaths_out_[le_location_idx]) {
                if (fp.duration() == le.time_ && is_dest_[fp.target().v_]) {
                  return journey_end{q_cur, le, le_location_idx, fp.target()};
                }
              }
            }
            // to intermodal
          } else {
            if (le.time_ == duration_t{dist_to_dest_[le_location_idx.v_]}) {
              // either the location of the l_entry itself has matching
              // dist_to_dest
              return journey_end{q_cur, le, le_location_idx, le_location_idx};
            } else {
              // or there exists a footpath such that footpath duration and
              // dist_to_dest of footpath target match the time of the l_entry
              for (auto const& fp :
                   tt_.locations_.footpaths_out_[le_location_idx]) {
                if (fp.duration() + duration_t{dist_to_dest_[fp.target().v_]} ==
                    le.time_) {
                  return journey_end{q_cur, le, le_location_idx, fp.target()};
                }
              }
            }
          }
        }
      }
    }
  }
  // if no journey end could be found
  return std::nullopt;
}

void tb_query_engine::add_final_footpath(query const& q,
                                         journey& j,
                                         journey_end const& je) const {

  if (q.dest_match_mode_ == location_match_mode::kIntermodal) {
    // add MUMO to END in case of intermodal routing
    for (auto const& offset : q.destination_) {
      // find offset for last location
      if (offset.target() == je.last_location_) {
        journey::leg leg_mumo_end{direction::kForward,
                                  je.last_location_,
                                  j.dest_,
                                  j.dest_time_ - offset.duration(),
                                  j.dest_time_,
                                  offset};
        j.add(std::move(leg_mumo_end));
        break;
      }
    }
    // add footpath if location of l_entry and journey end destination differ
    if (je.le_location_ != je.last_location_) {
      for (auto const fp : tt_.locations_.footpaths_out_[je.le_location_]) {
        if (fp.target() == je.last_location_) {
          unixtime_t const fp_time_end = j.legs_.back().dep_time_;
          unixtime_t const fp_time_start = fp_time_end - fp.duration();
          journey::leg leg_fp{direction::kForward, je.le_location_,
                              je.last_location_,   fp_time_start,
                              fp_time_end,         fp};
          j.add(std::move(leg_fp));
          break;
        }
      }
    }
  } else {
    // to station routing
    if (je.le_location_ == je.last_location_) {
      // add footpath with duration = 0 if destination is reached directly
      footpath const fp{je.last_location_, duration_t{0}};
      journey::leg leg_fp{direction::kForward, je.last_location_,
                          je.last_location_,   j.dest_time_,
                          j.dest_time_,        fp};
      j.add(std::move(leg_fp));
    } else {
      // add footpath between location of l_entry and destination
      for (auto const fp : tt_.locations_.footpaths_out_[je.le_location_]) {
        if (fp.target() == je.last_location_) {
          journey::leg leg_fp{direction::kForward, je.le_location_,
                              je.last_location_,   j.dest_time_ - fp.duration(),
                              j.dest_time_,        fp};
          j.add(std::move(leg_fp));
          break;
        }
      }
    }
  }
}

void tb_query_engine::add_segment_leg(journey& j,
                                      transport_segment const& seg) const {
  auto const from =
      stop{
          tt_.route_location_seq_[tt_.transport_route_[seg.get_transport_idx()]]
                                 [seg.stop_idx_start_]}
          .location_idx();
  auto const to =
      stop{
          tt_.route_location_seq_[tt_.transport_route_[seg.get_transport_idx()]]
                                 [seg.stop_idx_end_]}
          .location_idx();
  auto const dep_time =
      tt_.to_unixtime(seg.get_transport_day(base_),
                      tt_.event_mam(seg.get_transport_idx(),
                                    seg.stop_idx_start_, event_type::kDep)
                          .as_duration());
  auto const arr_time =
      tt_.to_unixtime(seg.get_transport_day(base_),
                      tt_.event_mam(seg.get_transport_idx(), seg.stop_idx_end_,
                                    event_type::kArr)
                          .as_duration());
  transport const t{seg.get_transport_idx(), seg.get_transport_day(base_)};
  journey::transport_enter_exit const tee{t, seg.stop_idx_start_,
                                          seg.stop_idx_end_};
  journey::leg leg_seg{direction::kForward, from, to, dep_time, arr_time, tee};
  j.add(std::move(leg_seg));
}

std::optional<unsigned> tb_query_engine::reconstruct_transfer(
    journey& j, transport_segment const& seg) const {
  assert(std::holds_alternative<journey::transport_enter_exit>(
      j.legs_.back().uses_));

  // data on target of transfer
  auto const target_transport_idx =
      std::get<journey::transport_enter_exit>(j.legs_.back().uses_).t_.t_idx_;
  auto const target_stop_idx =
      std::get<journey::transport_enter_exit>(j.legs_.back().uses_)
          .stop_range_.from_;
  auto const target_location_idx = j.legs_.back().from_;

  // iterate stops of segment
  for (auto stop_idx_exit = seg.stop_idx_start_ + 1U;
       stop_idx_exit <= seg.stop_idx_end_; ++stop_idx_exit) {
    auto const exit_location_idx =
        stop{tt_.route_location_seq_
                 [tt_.transport_route_[seg.get_transport_idx()]][stop_idx_exit]}
            .location_idx();

    // iterate transfers at each stop
    for (auto const& transfer :
         state_.ts_.at(seg.get_transport_idx().v_, stop_idx_exit)) {
      auto const transfer_transport_idx = transfer.get_transport_idx_to();
      auto const transfer_stop_idx = transfer.get_stop_idx_to();
      auto const transfer_location_idx =
          stop{tt_.route_location_seq_
                   [tt_.transport_route_[transfer_transport_idx]]
                   [transfer_stop_idx]}
              .location_idx();
      if (transfer_transport_idx == target_transport_idx &&
          transfer_stop_idx == target_stop_idx &&
          transfer_location_idx == target_location_idx) {

        // reconstruct footpath
        std::optional<footpath> fp_transfer = std::nullopt;
        if (exit_location_idx == target_location_idx) {
          // exit and target are equal -> use minimum transfer time
          fp_transfer =
              footpath{exit_location_idx,
                       tt_.locations_.transfer_time_[exit_location_idx]};
        } else {
          for (auto const& fp :
               tt_.locations_.footpaths_out_[exit_location_idx]) {
            if (fp.target() == target_location_idx) {
              fp_transfer = fp;
            }
          }
        }
        if (!fp_transfer.has_value()) {
          std::cerr << "Journey reconstruction failed: Could not find footpath "
                       "for transfer\n";
          return std::nullopt;
        }

        // add footpath leg for transfer
        auto const fp_time_start =
            tt_.to_unixtime(seg.get_transport_day(base_),
                            tt_.event_mam(seg.get_transport_idx(),
                                          stop_idx_exit, event_type::kArr)
                                .as_duration());
        auto const fp_time_end = fp_time_start + fp_transfer->duration();
        journey::leg leg_fp{direction::kForward, exit_location_idx,
                            target_location_idx, fp_time_start,
                            fp_time_end,         fp_transfer.value()};
        j.add(std::move(leg_fp));

        // return the stop idx at which the segment is exited
        return stop_idx_exit;
      }
    }
  }

  return std::nullopt;
}

void tb_query_engine::add_initial_footpath(query const& q, journey& j) const {
  // check if first transport departs at start location
  if (j.legs_.back().from_ != state_.start_location_) {
    // first transport does not start at queried start location
    // -> add footpath leg
    std::optional<footpath> fp_initial = std::nullopt;
    for (auto const& fp :
         tt_.locations_.footpaths_out_[state_.start_location_]) {
      if (fp.target() == j.legs_.back().from_) {
        fp_initial = fp;
      }
    }
    if (!fp_initial.has_value()) {
      std::cerr << "Journey reconstruction failed: Could reconstruct initial "
                   "footpath\n";
      return;
    }
    journey::leg leg_fp{direction::kForward,
                        state_.start_location_,
                        fp_initial->target(),
                        j.legs_.back().dep_time_ - fp_initial->duration(),
                        j.legs_.back().dep_time_,
                        fp_initial.value()};
    j.add(std::move(leg_fp));
  }

  if (q.start_match_mode_ == location_match_mode::kIntermodal) {
    for (auto const& offset : q.start_) {
      // add MUMO from START to first journey leg
      if (offset.target() == j.legs_.back().from_) {
        unixtime_t const mumo_start_unix{j.legs_.back().dep_time_ -
                                         offset.duration()};
        journey::leg l_mumo_start{direction::kForward,
                                  get_special_station(special_station::kStart),
                                  offset.target(),
                                  mumo_start_unix,
                                  j.legs_.back().dep_time_,
                                  offset};
        j.add(std::move(l_mumo_start));
        return;
      }
    }
    std::cerr << "Journey reconstruction failed: Could not reconstruct initial "
                 "MUMO edge\n";
  }
}
