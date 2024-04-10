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
#ifdef TB_OA_CHECK_PREVIOUS_N
  stats_.tb_oa_check_previous_n_ = true;
#endif
#ifdef TB_OA_COLLECT_STATS
  stats_.tb_oa_collect_stats_ = true;
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
#ifdef TB_OA_CHECK_PREVIOUS_N
  stats_.tb_oa_check_previous_n_ = true;
#endif
#ifdef TB_OA_COLLECT_STATS
  stats_.tb_oa_collect_stats_ = true;
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

//#ifndef NDEBUG
  TBDL << "Executing with start_time: " << unix_dhhmm(tt_, start_time)
       << ", max_transfers: " << std::to_string(max_transfers)
       << ", worst_time_at_dest: " << unix_dhhmm(tt_, worst_time_at_dest)
       << ", Initializing Q_0...\n";
/*#else
  (void)start_time;
#endif*/

  // init Q_0
  for (auto const& qs : state_.query_starts_) {
    handle_start(start_time, qs, worst_time_at_dest, results);
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
    if(stats_.n_largest_queue_size < state_.q_n_.size()) {
      stats_.n_largest_queue_size = state_.q_n_.size();
    }
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
  if(stats_.n_largest_queue_size < state_.q_n_.size()) {
    stats_.n_largest_queue_size = state_.q_n_.size();
  }
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
  auto const day_offset = nigiri::routing::tripbased::compute_day_offset(base_.v_, static_cast<uint16_t>(d_seg));
  auto const tau_d = (d_seg + tau_dep_t_b_d - base_.v_) * 1440 + tau_dep_t_b_tod;

#ifdef TB_ONETOALL_BITFIELD_IDX
  bitfield operating_days = tt_.bitfields_[seg.operating_days_];
#else
  bitfield operating_days = seg.operating_days_;
#endif

  if(day_offset < 5) {
    operating_days = operating_days << static_cast<size_t>(5 - day_offset);
  } else if(day_offset > 5 && day_offset < 7) {
    operating_days = operating_days >> static_cast<size_t>(day_offset - 5);
  }

  for (stop_idx_t i = seg.get_stop_idx_start() + 1U;
       i <= seg.get_stop_idx_end(); ++i) {
    auto const travel_time_seg =
        tt_.event_mam(seg.get_transport_idx(), i, event_type::kArr).count() - tau_dep_t_b;
    auto const tau_d_temp = tau_d + travel_time_seg;
    auto const stop_temp = stop{
        tt_.route_location_seq_[tt_.transport_route_[seg.get_transport_idx()]][i]};
    auto const location_id = stop_temp.location_idx();

    auto const t_cur =
        tt_.to_unixtime(base_, minutes_after_midnight_t{tau_d_temp});

    if (stop_temp.out_allowed() && t_cur < worst_time_at_dest) {

      auto [non_dominated_tmin, tmin_begin, tmin_end, tmin_comparisons] =
          state_.t_min_[location_id.v_][n].add_bitfield(
              oneToAll_tMin{t_cur, operating_days});

#ifdef TB_OA_COLLECT_STATS
      stats_.n_tmin_comparisons_ += tmin_comparisons;
#endif

      if (non_dominated_tmin) {
        journey_bitfield j{};
        j.start_time_ = start_time;
        j.dest_time_ = t_cur;
        j.dest_ = location_id;
        j.transfers_ = n;
        j.bitfield_ = operating_days;
#ifdef TB_OA_DEBUG_TRIPS
        j.trip_names_ = seg.trip_names_;
#endif

#ifdef TB_OA_COLLECT_STATS
        auto [non_dominated_res, res_begin, res_end, res_comparisons] = results[location_id.v_].add_bitfield(std::move(j));
        stats_.n_results_comparisons_ += res_comparisons;
#else
        results[location_id.v_].add_bitfield(std::move(j));
#endif
        ++stats_.n_journeys_found_;
      } else {
        stats_.n_segments_pruned_++;
        continue;
      }

      // CheckFootpaths
      for (auto const fp : tt_.locations_.footpaths_out_[location_id]) {
        if (t_cur + i32_minutes{fp.duration_} > worst_time_at_dest) {
          continue;
        }

        auto [non_dominated_fp, fp_begin, fp_end, fp_comparisons] =
            state_.t_min_[fp.target().v_][n].add_bitfield(
                oneToAll_tMin{t_cur + i32_minutes{fp.duration_},
                              operating_days});

#ifdef TB_OA_COLLECT_STATS
        stats_.n_tmin_comparisons_ += fp_comparisons;
#endif

        if (non_dominated_fp) {
          journey_bitfield j_fp{};
          j_fp.start_time_ = start_time;
          j_fp.dest_time_ = t_cur + i32_minutes{fp.duration_};
          j_fp.dest_ = fp.target();
          j_fp.transfers_ = n;
          j_fp.bitfield_ = operating_days;
#ifdef TB_OA_DEBUG_TRIPS
          j_fp.trip_names_ = seg.trip_names_;
#endif

#ifdef TB_OA_COLLECT_STATS
          auto [non_dominated_res_fp, res_fp_begin, res_fp_end, res_fp_comparisons] = results[fp.target_].add_bitfield(std::move(j_fp));
          stats_.n_results_comparisons_ += res_fp_comparisons;
#else
          results[fp.target_].add_bitfield(std::move(j_fp));
#endif
          ++stats_.n_journeys_found_;
        }
      }

      // CheckTransfers
      if (state_.ts_.n_transfers_ > 0 && ((n + 1) < kMaxTransfers)) {
        auto const& transfers =
            state_.ts_.data_.at(seg.get_transport_idx().v_, i);
        for (auto const& transfer : transfers) {
          auto const& theta = tt_.bitfields_[transfer.get_bitfield_idx()];

          auto const tau_arr_t_i =
              tt_.event_mam(seg.get_transport_idx(), i, event_type::kArr)
                  .count();
          auto const tau_dep_u_j =
              tt_.event_mam(transfer.get_transport_idx_to(),
                            transfer.stop_idx_to_, event_type::kDep)
                  .count();

          auto const d_tr = d_seg + tau_arr_t_i / 1440 - tau_dep_u_j / 1440 +
                            transfer.passes_midnight_;
          auto const new_day_offset =
              nigiri::routing::tripbased::compute_day_offset(
                  base_.v_, static_cast<uint16_t>(d_tr));

#ifdef TB_ONETOALL_BITFIELD_IDX
          bitfield new_operating_days = theta & tt_.bitfields_[seg.operating_days_];
#else
          bitfield new_operating_days = theta & seg.operating_days_;
#endif

          if(new_day_offset > day_offset) {
            new_operating_days = new_operating_days << static_cast<size_t>(new_day_offset - day_offset);
          } else if(new_day_offset < day_offset) {
            new_operating_days = new_operating_days >> static_cast<size_t>(day_offset - new_day_offset);
          }

          if(new_operating_days.none()) {
            stats_.n_segments_pruned_++;
            continue;
          }

#ifdef TB_OA_CHECK_PREVIOUS_N
          auto const travel_time_next =
              tt_.event_mam(transfer.get_transport_idx_to(),
                            transfer.stop_idx_to_ + 1, event_type::kArr)
                  .count() -
              tau_dep_u_j;

          auto const unix_time_next = tt_.to_unixtime(
              base_, minutes_after_midnight_t{tau_d_temp + travel_time_next});

          auto const stop_next =
              stop{tt_.route_location_seq_
                       [tt_.transport_route_[transfer.get_transport_idx_to()]]
                       [transfer.stop_idx_to_ + 1]};
          auto const location_next_id = stop_next.location_idx();

          for (std::uint8_t n_temp = 0; n_temp < n + 1; n_temp++) {
            auto non_dominated_tmin_n_check =
                state_.t_min_[location_next_id.v_][n_temp].check(
                    oneToAll_tMin{unix_time_next, new_operating_days});

            if (!non_dominated_tmin_n_check) {
              stats_.n_segments_pruned_++;
              continue;
            }
          }
#endif

#ifdef TB_OA_DEBUG_TRIPS
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

          auto [enqueued, n_comparison] = state_.q_n_.enqueue(static_cast<std::uint16_t>(new_day_offset),
                              transfer.get_transport_idx_to(),
                              transfer.get_stop_idx_to(), n + 1U, q_cur,
                              new_operating_days, trip_names);
          if(!enqueued) {
            stats_.n_enqueue_prevented_by_reached_++;
          }
#ifdef TB_OA_COLLECT_STATS
          stats_.n_reached_comparisons_ += n_comparison;
#endif
#else
          auto [enqueued, n_comparison] = state_.q_n_.enqueue(static_cast<std::uint16_t>(new_day_offset),
                              transfer.get_transport_idx_to(),
                              transfer.get_stop_idx_to(), n + 1U, q_cur,
                              new_operating_days);
          if(!enqueued) {
            stats_.n_enqueue_prevented_by_reached_++;
          }
#ifdef TB_OA_COLLECT_STATS
          stats_.n_reached_comparisons_ += n_comparison;
#endif
#endif
        }
      }
    }
  }
}

void oneToAll_engine::handle_start(unixtime_t const start_time, oneToAll_start const& start, unixtime_t const worst_time_at_dest, std::vector<pareto_set<journey_bitfield>>& results)
{
  auto const day_idx_mam = tt_.day_idx_mam(start.time_);
  std::int32_t const d = day_idx_mam.first.v_;
  std::int32_t const tau = day_idx_mam.second.count();

  handle_start_footpath(d, tau, start.bitfield_, footpath{start.location_, duration_t{0U}}, worst_time_at_dest);
  add_start_footpath_journey(start_time, d, tau, footpath{start.location_, duration_t{0U}}, results);
  for (auto const fp : tt_.locations_.footpaths_out_[start.location_]) {
    handle_start_footpath(d, tau, start.bitfield_, fp, worst_time_at_dest);
    add_start_footpath_journey(start_time, d, tau, fp, results);
  }
}

void oneToAll_engine::add_start_footpath_journey(unixtime_t const start_time, std::int32_t const d, std::int32_t const tau, footpath const fp, std::vector<pareto_set<journey_bitfield>>& results) {
  bitfield all_set;
  for(size_t i = 0; i < all_set.size(); i++) {
    all_set.set(i);
  }

  auto [non_dominated_tmin, tmin_begin, tmin_end, n_comparisons_tmin] =
      state_.t_min_[fp.target_][0].add_bitfield(
          oneToAll_tMin{tt_.to_unixtime(day_idx_t{d}, minutes_after_midnight_t{tau + fp.duration_}), all_set});

#ifdef TB_OA_COLLECT_STATS
  stats_.n_tmin_comparisons_ += n_comparisons_tmin;
#endif

  if (non_dominated_tmin) {
    journey_bitfield j{};
    j.start_time_ = start_time;
    j.dest_time_ = tt_.to_unixtime(day_idx_t{d}, minutes_after_midnight_t{tau + fp.duration_});
    j.dest_ = fp.target();
    j.transfers_ = 0;
    j.bitfield_ = all_set;

#ifdef TB_OA_COLLECT_STATS
    auto [non_dominated_res, res_begin, res_end, res_comparisons] = results[fp.target().v_].add_bitfield(std::move(j));
    stats_.n_results_comparisons_ += res_comparisons;
#else
    results[fp.target_].add_bitfield(std::move(j_fp));
#endif

    results[fp.target().v_].add_bitfield(std::move(j));
    ++stats_.n_journeys_found_;
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

          if(shift < 0) {
            new_days = new_days >> static_cast<size_t>(std::abs(shift));
          } else {
            new_days = new_days << static_cast<size_t>(shift);
          }

#ifndef NDEBUG
          TBDL << "Attempting to enqueue a segment of transport " << t << ": "
               << tt_.transport_name(t) << ", departing at "
               << unix_dhhmm(tt_, tt_.to_unixtime(day_idx_t{d_seg},
                                                  tau_dep_t_i->as_duration()))
               << "\n";
#endif

          if(new_days.any()) {
#ifdef TB_OA_DEBUG_TRIPS
            std::vector<std::string_view> trip_names;
            trip_names.emplace_back(tt_.trip_id_strings_[tt_.trip_ids_[tt_.merged_trips_[tt_.transport_to_trip_section_[t].front()].front()].front()].view());

            auto [enqueued, n_comparison] = state_.q_n_.enqueue(static_cast<std::uint16_t>(d_seg), t, i, 0U, TRANSFERRED_FROM_NULL,
                                new_days, trip_names);
            if(!enqueued) {
              stats_.n_enqueue_prevented_by_reached_++;
            }
#ifdef TB_OA_COLLECT_STATS
            stats_.n_reached_comparisons_ += n_comparison;
#endif
#else
            auto [enqueued, n_comparison] = state_.q_n_.enqueue(static_cast<std::uint16_t>(d_seg), t, i, 0U, TRANSFERRED_FROM_NULL,
                                new_days);
            if(!enqueued) {
              stats_.n_enqueue_prevented_by_reached_++;
            }
#ifdef TB_OA_COLLECT_STATS
            stats_.n_reached_comparisons_ += n_comparison;
#endif
#endif
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
