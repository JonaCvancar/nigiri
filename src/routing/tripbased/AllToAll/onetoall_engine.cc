#include <ranges>

#include "utl/get_or_create.h"

#include "nigiri/routing/for_each_meta.h"
#include "nigiri/routing/journey_bitfield.h"
#include "nigiri/routing/tripbased/dbg.h"
#include "nigiri/routing/tripbased/settings.h"

#include "nigiri/routing/tripbased/AllToAll/onetoall_engine.h"
#include "nigiri/rt/frun.h"
#include "nigiri/special_stations.h"

using namespace nigiri;
using namespace nigiri::routing;
using namespace nigiri::routing::tripbased;

#ifdef TB_ONETOALL_BITFIELD_IDX
oneToAll_engine::oneToAll_engine(timetable& tt,
                                 rt_timetable const* rtt,
                                 oneToAll_state& state,
                                 day_idx_t const base,
                                 hash_map<bitfield, bitfield_idx_t>& bitfieldMap)
    : tt_{tt},
      rtt_{rtt},
      state_{state},
      base_{base},
      bitfield_to_bitfield_idx_{bitfieldMap}
{
#ifdef EQUAL_JOURNEY
  stats_.equal_journey_ = true;
#endif
#ifdef TB_QUEUE_HANDLING
  stats_.tb_queue_handling_ = true;
#endif
#ifdef TB_ONETOALL_BITFIELD_IDX
  stats_.tb_onetoall_bitfield_idx_ = true;
#endif
#ifdef TB_OA_ADD_ONTRIP
  stats_.tb_oa_add_ontrip_ = true;
#endif

  rusage r_usage;
  getrusage(RUSAGE_SELF, &r_usage);
  stats_.pre_memory_usage_ = static_cast<double>(r_usage.ru_maxrss) / 1e6;

  // reset state for new query
  state_.reset(base);
  state_.q_n_.set_bitfield_to_bitfield_idx(bitfieldMap);
}
#else
oneToAll_engine::oneToAll_engine(timetable const& tt,
                                 rt_timetable const* rtt,
                                 oneToAll_state& state,
                                 day_idx_t const base)
    : tt_{tt},
      rtt_{rtt},
      state_{state},
      base_{base}
{
#ifdef EQUAL_JOURNEY
  stats_.equal_journey_ = true;
#endif
#ifdef TB_QUEUE_HANDLING
  stats_.tb_queue_handling_ = true;
#endif
#ifdef TB_ONETOALL_BITFIELD_IDX
  stats_.tb_onetoall_bitfield_idx_ = true;
#endif
#ifdef TB_OA_ADD_ONTRIP
  stats_.tb_oa_add_ontrip_ = true;
#endif

  rusage r_usage;
  getrusage(RUSAGE_SELF, &r_usage);
  stats_.pre_memory_usage_ = static_cast<double>(r_usage.ru_maxrss) / 1e6;

  // reset state for new query
  state_.reset(base);
}
#endif

#ifdef TB_ONETOALL_BITFIELD_IDX
void oneToAll_engine::execute(unixtime_t const start_time,
                              std::uint8_t const max_transfers,
                              unixtime_t const worst_time_at_dest,
                              std::vector<pareto_set<journey_bitfield>>& results,
                              hash_map<bitfield, bitfield_idx_t>& bitfieldMap)
#else
void oneToAll_engine::execute(unixtime_t const start_time,
                              std::uint8_t const max_transfers,
                              unixtime_t const worst_time_at_dest,
                              std::vector<pareto_set<journey_bitfield>>& results)
#endif
{
#ifdef TB_ONETOALL_BITFIELD_IDX
  bitfield_to_bitfield_idx_ = bitfieldMap;
#endif

#ifndef NDEBUG
  TBDL << "Executing with start_time: " << unix_dhhmm(tt_, start_time)
       << ", max_transfers: " << std::to_string(max_transfers)
       << ", worst_time_at_dest: " << unix_dhhmm(tt_, worst_time_at_dest)
       << ", Initializing Q_0...\n";
#else
  (void)start_time;
#endif

  // init Q_0
  for (auto const& qs : state_.query_starts_) {
    handle_start(qs, worst_time_at_dest);
  }

  // process all Q_n in ascending order, i.e., transport segments reached after
  // n transfers
  std::uint8_t n = 0U;
  for (; n != state_.q_n_.start_.size() && n < max_transfers; ++n) {
#ifndef NDEBUG
    TBDL << "Handle " << std::to_string(state_.q_n_.end_[n] - state_.q_n_.start_[n]) << " Segments after n=" << std::to_string(n) << " transfers.\n";
#endif

#ifdef TB_QUEUE_HANDLING
    stats_.n_segments_enqueued_ += state_.q_n_.size();
#endif
    // iterate trip segments in Q_n
    for (auto q_cur = state_.q_n_.start_[n]; q_cur != state_.q_n_.end_[n];
         ++q_cur) {
      handle_segment(start_time, worst_time_at_dest, results, n, q_cur);
    }

#ifdef TB_QUEUE_HANDLING
    state_.q_n_.erase(state_.q_n_.start_[n], state_.q_n_.end_[n]);

    std::uint8_t n2 = n + 1;
    std::uint32_t elements_deleted = state_.q_n_.end_[n] - state_.q_n_.start_[n];
    for(; n2 != state_.q_n_.start_.size(); ++n2) {
      state_.q_n_.start_[n2] -= elements_deleted;
      state_.q_n_.end_[n2] -= elements_deleted;
    }
    state_.q_n_.start_[n2] = 0;
    state_.q_n_.end_[n2] = 0;
#endif
  }

#ifndef NDEBUG
  int count = 0;
  for(const auto& element : results) {
    count += element.size();
  }

  TBDL <<  "Finished all segments. Results count: " << count << " Que length: " << state_.q_n_.size() << "\n";
#endif

#ifndef TB_QUEUE_HANDLING
  stats_.n_segments_enqueued_ += state_.q_n_.size();
#endif
  stats_.empty_n_ = n;
  stats_.max_transfers_reached_ = n == max_transfers;
}

void oneToAll_engine::handle_segment(unixtime_t const start_time,
                                     unixtime_t const worst_time_at_dest,
                                     std::vector<pareto_set<journey_bitfield>>& results,
                                     std::uint8_t const n,
                                     queue_idx_t const q_cur) {
  auto seg = state_.q_n_[q_cur];

  auto const tau_dep_t_b = tt_.event_mam(seg.get_transport_idx(),
                                         seg.stop_idx_start_, event_type::kDep)
      .count();

  auto const tau_dep_t_b_d =
      tt_.event_mam(seg.get_transport_idx(), seg.stop_idx_start_,
                    event_type::kDep)
          .days();

  auto const tau_dep_t_b_tod =
      tt_.event_mam(seg.get_transport_idx(), seg.stop_idx_start_,
                    event_type::kDep)
          .mam();

  std::int32_t const d_seg = seg.get_transport_day(base_).v_;
  auto const day_offset = nigiri::routing::tripbased::compute_day_offset(
      base_.v_, static_cast<uint16_t>(d_seg));
  auto const tau_d =
      (d_seg + tau_dep_t_b_d - base_.v_) * 1440 + tau_dep_t_b_tod;
  bitfield shifted_operating_days = tt_.bitfields_[seg.operating_days_];

  for (stop_idx_t i = seg.get_stop_idx_start() + 1U;
       i <= seg.get_stop_idx_end(); ++i) {
    auto const travel_time_seg =
        tt_.event_mam(seg.get_transport_idx(), i, event_type::kArr).count() -
        tau_dep_t_b;
    auto const tau_d_temp = tau_d + travel_time_seg;
    auto const stop_temp = stop{
        tt_.route_location_seq_[tt_.transport_route_[seg.get_transport_idx()]]
        [i]};
    auto const location_id = stop_temp.location_idx();

    auto const t_cur =
        tt_.to_unixtime(base_, minutes_after_midnight_t{tau_d_temp});

    if (stop_temp.out_allowed() && t_cur < worst_time_at_dest) {

      /*
#ifdef TB_ONETOALL_BITFIELD_IDX
      auto [non_dominated_tmin, tmin_begin, tmin_end] =
          state_.t_min_[location_id.v_][n].add_bitfield(oneToAll_tMin{t_cur,
                                                                      tt_.bitfields_[seg.operating_days_]});
#else
      auto [non_dominated_tmin,
          tmin_begin, tmin_end] =
          state_.t_min_[location_id.v_][n].add_bitfield(oneToAll_tMin{t_cur,
                                                                      seg.operating_days_});
#endif

      if(non_dominated_tmin) {
*/
        journey_bitfield j{};
        j.start_time_ = start_time;
        j.dest_time_ = t_cur;
        j.dest_ = location_id;
        j.transfers_ = n;
#ifdef TB_ONETOALL_BITFIELD_IDX
        j.bitfield_ = shifted_operating_days;
#else
        j.bitfield_ = seg.operating_days_;
#endif
        j.trip_names_ = seg.trip_names_;

        if (j.travel_time() < kMaxTravelTime) {
          results[location_id.v_].add_bitfield(std::move(j));
          ++stats_.n_journeys_found_;
        }
      /*} else {
        stats_.n_segments_pruned_++;
        continue;
      }*/

      // CheckFootpaths
      for (auto const fp : tt_.locations_.footpaths_out_[location_id]) {
        if (((t_cur + i32_minutes{fp.duration_}) > worst_time_at_dest)) {
          continue;
        }

/*
#ifdef TB_ONETOALL_BITFIELD_IDX
        auto [non_dominated, begin, end] =
            state_.t_min_[fp.target().v_][n].add_bitfield(oneToAll_tMin{t_cur +
                                                                        i32_minutes{fp.duration_}, tt_.bitfields_[seg.operating_days_]});
#else
        auto [non_dominated, begin, end] =
  state_.t_min_[fp.target().v_][n].add_bitfield(oneToAll_tMin{t_cur +
  i32_minutes{fp.duration_}, seg.operating_days_});
#endif
        if(non_dominated) {
        */
          journey_bitfield j_fp{};
          j_fp.start_time_ = start_time;
          j_fp.dest_time_ = t_cur + i32_minutes{fp.duration_};
          j_fp.dest_ = fp.target();
          j_fp.transfers_ = n;
#ifdef TB_ONETOALL_BITFIELD_IDX
          j_fp.bitfield_ = shifted_operating_days;
#else
          j_fp.bitfield_ = seg.operating_days_;
#endif
          j_fp.trip_names_ = seg.trip_names_;

          if (j_fp.travel_time() < kMaxTravelTime) {
            results[fp.target_].add_bitfield(std::move(j_fp));
            ++stats_.n_journeys_found_;
          }
        //}
      }

      // CheckTransfers
      if (state_.ts_.n_transfers_ > 0 && ((n + 1) < kMaxTransfers)) {
        auto const& transfers =
            state_.ts_.data_.at(seg.get_transport_idx().v_, i);
        for (auto const& transfer : transfers) {
          auto const& theta = tt_.bitfields_[transfer.get_bitfield_idx()];

          auto const tau_arr_t_i =
              tt_.event_mam(seg.get_transport_idx(), i, event_type::kArr).count();
          auto const tau_dep_u_j =
              tt_.event_mam(transfer.get_transport_idx_to(),
                            transfer.stop_idx_to_, event_type::kDep)
                  .count();

          auto const d_tr = d_seg + tau_arr_t_i / 1440 - tau_dep_u_j / 1440 +
                            transfer.passes_midnight_;



          bitfield new_operating_days =
              tt_.bitfields_[seg.operating_days_] & theta;
          int shift = d_tr - d_seg;
          if (shift > 0) {
            if (!transfer.passes_midnight_) {
#ifdef TB_ONETOALL_BITFIELD_IDX
              new_operating_days = (tt_.bitfields_[seg.operating_days_] &
                                    (theta << static_cast<size_t>(shift)));
#else
              new_operating_days = seg.operating_days_ & theta;
#endif
            }
          }

          if (!new_operating_days.any()) {
            continue;
          }

          /*
          auto const travel_time_next =
              tt_.event_mam(transfer.get_transport_idx_to(),
                            transfer.stop_idx_to_ + 1, event_type::kArr)
                  .count() - tau_dep_u_j;

          auto const unix_time_next = tt_.to_unixtime(base_,
          minutes_after_midnight_t{tau_d_temp + travel_time_next});

          auto const stop_next =
          stop{tt_.route_location_seq_[tt_.transport_route_[transfer.get_transport_idx_to()]][transfer.stop_idx_to_
          + 1]}; auto const location_next_id = stop_next.location_idx();

          auto non_dominated_tmin =
              state_.t_min_[location_next_id.v_][n +
          1].check(oneToAll_tMin{unix_time_next, new_operating_days});

          if(!non_dominated_tmin) {
            stats_.n_segments_pruned_++;
            continue;
          }
          */

          std::vector<std::string_view> trip_names = seg.trip_names_;
          trip_names.emplace_back(
              tt_.trip_id_strings_
              [tt_.trip_ids_
              [tt_.merged_trips_[tt_.transport_to_trip_section_
                  [transfer.get_transport_idx_to()]
                      .front()]
                      .front()]
                  .front()]
                  .view());

          state_.q_n_.enqueue(static_cast<std::uint16_t>(d_tr),
                              transfer.get_transport_idx_to(),
                              transfer.get_stop_idx_to(), n + 1U, q_cur,
                              new_operating_days, trip_names);
        }
      }
    }
  }
}

void oneToAll_engine::handle_start(oneToAll_start const& start, unixtime_t const worst_time_at_dest)
{
  auto const day_idx_mam = tt_.day_idx_mam(start.time_);
  std::int32_t const d = day_idx_mam.first.v_;
  std::int32_t const tau = day_idx_mam.second.count();

  handle_start_footpath(d, tau, start.bitfield_, footpath{start.location_, duration_t{0U}}, worst_time_at_dest);
  for (auto const fp : tt_.locations_.footpaths_out_[start.location_]) {
    handle_start_footpath(d, tau, start.bitfield_, fp, worst_time_at_dest);
  }
}

void oneToAll_engine::handle_start_footpath(std::int32_t const d,
                                            std::int32_t const tau,
                                            bitfield const bf,
                                            footpath const fp,
                                            unixtime_t const worst_time_at_dest)
{
  auto const alpha = tau + fp.duration().count();
  auto const alpha_d = alpha / 1440;
  auto const alpha_tod = alpha % 1440;

  for (auto const route_idx : tt_.location_routes_[fp.target()]) {
#ifndef NDEBUG
    TBDL << "Route " << route_idx << "\n";
#endif
    for (std::uint16_t i = 0U; i < tt_.route_location_seq_[route_idx].size() - 1; ++i) {
      auto const route_stop = stop{tt_.route_location_seq_[route_idx][i]};
      if (route_stop.location_idx() == fp.target() && route_stop.in_allowed()) {
        bitfield found_days;

        auto const event_times =
            tt_.event_times_at_stop(route_idx, i, event_type::kDep);
        auto tau_dep_t_i =
            std::lower_bound(event_times.begin(), event_times.end(), alpha_tod,
                             [&](auto&& x, auto&& y) { return x.mam() < y; });
        auto sigma = alpha_d;
        if (tau_dep_t_i == event_times.end()) {
          ++sigma;
          tau_dep_t_i = event_times.begin();
        }

        while (sigma <= 1) {
          std::int32_t const sigma_t = tau_dep_t_i->days();
          auto const d_seg = d + sigma - sigma_t;

          if(tt_.to_unixtime(day_idx_t{d_seg}, tau_dep_t_i->as_duration()) > worst_time_at_dest) {
            break;
          }

          auto const k = static_cast<std::size_t>(std::distance(event_times.begin(), tau_dep_t_i));
          auto const t = tt_.route_transport_ranges_[route_idx][k];
          auto const& beta_t = tt_.bitfields_[tt_.transport_traffic_days_[t]];
          bitfield shift_beta_t = beta_t;
          int shift = sigma - sigma_t;
          if(shift < 0) {
            shift_beta_t = beta_t << static_cast<size_t>(std::abs(shift));
          } else {
            shift_beta_t = beta_t >> static_cast<size_t>(shift);
          }
          bitfield new_days = shift_beta_t & ~found_days;
          new_days = new_days & bf;
          found_days |= new_days;

#ifndef NDEBUG
          TBDL << "Attempting to enqueue a segment of transport " << t << ": "
               << tt_.transport_name(t) << ", departing at "
               << unix_dhhmm(tt_, tt_.to_unixtime(day_idx_t{d_seg},
                                                  tau_dep_t_i->as_duration()))
               << "\n";
#endif

          if(new_days.any()) {
            std::vector<std::string_view> trip_names;
            trip_names.emplace_back(tt_.trip_id_strings_[tt_.trip_ids_[tt_.merged_trips_[tt_.transport_to_trip_section_[t].front()].front()].front()].view());

            state_.q_n_.enqueue(static_cast<std::uint16_t>(d_seg), t, i, 0U, TRANSFERRED_FROM_NULL,
                                new_days, trip_names);
          }

          if(found_days == bf) {
            break;
          }

          if (tau_dep_t_i + 1 == event_times.end()) {
            ++sigma;
            tau_dep_t_i = event_times.begin();
          } else {
            ++tau_dep_t_i;
          }
        }
      }
    }
  }
}
