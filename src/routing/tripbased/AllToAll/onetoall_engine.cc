#include <ranges>

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

oneToAll_engine::oneToAll_engine(timetable const& tt,
                                 rt_timetable const* rtt,
                                 oneToAll_state& state,
                                 day_idx_t const base)
        : tt_{tt},
          rtt_{rtt},
          state_{state},
          base_{base} {

    // reset state for new query
    state_.reset(base);
}

void oneToAll_engine::execute(unixtime_t const start_time,
                              std::uint8_t const max_transfers,
                              unixtime_t const worst_time_at_dest,
                              std::vector<pareto_set<journey_bitfield>>& results
) {
    (void)results;

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
        handle_start(qs);
    }

    // process all Q_n in ascending order, i.e., transport segments reached after
    // n transfers
    std::uint8_t n = 0U;
    for (; n != state_.q_n_.start_.size() && n < max_transfers; ++n) {
/*#ifndef NDEBUG
        TBDL << "Indices for n: " << std::to_string(n) << " From: " << state_.q_n_.start_[n] << " To: " << state_.q_n_.end_[n] << "\n";
#endif*/

        // iterate trip segments in Q_n
        for (auto q_cur = state_.q_n_.start_[n]; q_cur != state_.q_n_.end_[n];
             ++q_cur) {
            handle_segment(start_time, worst_time_at_dest, results, n, q_cur);
        }
    }

    stats_.n_segments_enqueued_ += state_.q_n_.size();
    stats_.empty_n_ = n;
    stats_.max_transfers_reached_ = n == max_transfers;
}

void oneToAll_engine::handle_segment(unixtime_t const start_time,
                                     unixtime_t const worst_time_at_dest,
                                     std::vector<pareto_set<journey_bitfield>>& results,
                                     std::uint8_t const n,
                                     queue_idx_t const q_cur) {
    // the current transport segment
    auto seg = state_.q_n_[q_cur];

#ifndef NDEBUG
    TBDL << "Examining segment: ";
    state_.q_n_.print(std::cout, q_cur);
#endif

    // departure time at the start of the transport segment
    auto const tau_dep_t_b = tt_.event_mam(seg.get_transport_idx(),
                                           seg.stop_idx_start_,
                                           event_type::kDep).count();

    auto const tau_dep_t_b_d = tt_.event_mam(seg.get_transport_idx(),
                                             seg.stop_idx_start_,
                                             event_type::kDep).days();

    auto const tau_dep_t_b_tod = tt_.event_mam(seg.get_transport_idx(),
                                               seg.stop_idx_start_,
                                               event_type::kDep).mam();

    // the day index of the segment
    std::int32_t const d_seg = seg.get_transport_day(base_).v_;
    // departure time at start of current transport segment in minutes after
    // midnight on the day of the query
    auto const tau_d = (d_seg + tau_dep_t_b_d - base_.v_) * 1440 + tau_dep_t_b_tod;

    auto const seg_route_idx = tt_.transport_route_[seg.get_transport_idx()];

    for(stop_idx_t i = seg.get_stop_idx_start() + 1U; i <= seg.get_stop_idx_end(); ++i) {
        auto const travel_time_seg =
                tt_.event_mam(seg.get_transport_idx(), i, event_type::kArr).count() - tau_dep_t_b;
        auto const tau_d_temp = tau_d + travel_time_seg;
        auto const stop_temp = stop{tt_.route_location_seq_[tt_.transport_route_[seg.get_transport_idx()]][i]};
        auto const location_id = stop_temp.location_idx();

        auto const t_cur = tt_.to_unixtime(base_, minutes_after_midnight_t{tau_d_temp});

        if( (t_cur > worst_time_at_dest) && (t_cur < state_.t_min_[location_id.v_][n]) ) {
          ++stats_.n_segments_pruned_;
          break;
        }

        if (stop_temp.out_allowed()) {
#ifndef NDEBUG
          TBDL << "segment reaches stop " << i << " at "
               << dhhmm(tau_d_temp) << "\n";
#endif
            state_.t_min_[location_id.v_][n] = t_cur;
            // add journey without reconstructing yet
            journey_bitfield j{};
            j.start_time_ = start_time;
            j.dest_time_ = t_cur;
            j.dest_ = location_id;
            j.transfers_ = n;
            j.bitfield_ = seg.operating_days_;
            // add journey to pareto set (removes dominated entries)
#ifndef NDEBUG
            /*
            TBDL << "updating pareto set with new journey: ";
            j.print(std::cout, tt_);
            */
            auto [non_dominated, begin, end] = results[location_id.v_].add_bitfield(std::move(j));
            /*if (non_dominated) {
                TBDL << "new journey ending with this segment is non-dominated\n";
            } else {
                TBDL << "new journey ending with this segment is dominated\n";
            }*/
#else
            results[location_id.v_].add_bitfield(std::move(j));
            /*
            if(results[location_id.v_].size() > 1) {
                for(auto& element : results[location_id.v_]) {
                    element.print(std::cout, tt_);
                }
            }
             */

            ++stats_.n_journeys_found_;
#endif
        }
#ifndef NDEBUG
        else {
          TBDL << "Not allowed to get out at this stop.\n";
        }
#endif


        if(state_.ts_.n_transfers_ > 0) {
            auto const& transfers = state_.ts_.data_.at(seg.get_transport_idx().v_, i);
            for(auto const& transfer : transfers) {
                auto const& theta = tt_.bitfields_[transfer.get_bitfield_idx()];
                //if (theta.test(static_cast<std::size_t>(d_seg))) {
                    auto const tau_arr_t_i =
                            tt_.event_mam(seg.get_transport_idx(), i, event_type::kArr)
                                    .count();
                    // departure time at end location of transferl
                    auto const tau_dep_u_j =
                            tt_.event_mam(transfer.get_transport_idx_to(),
                                          transfer.stop_idx_to_, event_type::kDep)
                                    .count();

                    auto const d_tr = tau_arr_t_i / 1440 - tau_dep_u_j / 1440 +
                                      transfer.passes_midnight_;


                    bitfield new_operating_days;
                    if(transfer.passes_midnight_) {
                        new_operating_days = seg.operating_days_ & (theta << 1);
                    } else {
                        new_operating_days = seg.operating_days_ & theta;
                    }
#ifndef NDEBUG
                    TBDL << "Found a transfer to transport "
                         << transfer.get_transport_idx_to() << ": "
                         << tt_.transport_name(transfer.get_transport_idx_to())
                         << " at its stop " << transfer.stop_idx_to_ << ": "
                         << location_name(tt_,
                                          stop{tt_.route_location_seq_
                                               [tt_.transport_route_
                                                  [transfer.get_transport_idx_to()]]
                                               [transfer.get_stop_idx_to()]}
                                                  .location_idx())
                         << ", departing at "
                         << unix_dhhmm(tt_,
                                       tt_.to_unixtime(
                                               day_idx_t{d_tr},
                                               tt_.event_mam(transfer.get_transport_idx_to(),
                                                             transfer.get_stop_idx_to(),
                                                             event_type::kDep)
                                                       .as_duration()))
                         << "\n";
#endif

                    // New segment after transfer
                    state_.q_n_.enqueue(static_cast<std::uint16_t>(d_tr),
                                        transfer.get_transport_idx_to(),
                                        transfer.get_stop_idx_to(),
                                        n + 1U,
                                        q_cur,
                                        new_operating_days);
                //}
            }
        }
    }
}

void oneToAll_engine::handle_start(oneToAll_start const& start) {

    // start day and time
    auto const day_idx_mam = tt_.day_idx_mam(start.time_);
    // start day
    std::int32_t const d = day_idx_mam.first.v_;
    // start time
    std::int32_t const tau = day_idx_mam.second.count();

#ifndef NDEBUG
    TBDL << "handle_start | start_location: "
         << location_name(tt_, start.location_)
         << " | start_time: " << dhhmm(d * 1440 + tau) << "\n";
#endif

    // virtual reflexive footpath
#ifndef NDEBUG
    TBDL << "Examining routes at start location: "
         << location_name(tt_, start.location_) << "\n";
#endif
    handle_start_footpath(d, tau, footpath{start.location_, duration_t{0U}});
    // iterate outgoing footpaths of source location
    for (auto const fp : tt_.locations_.footpaths_out_[start.location_]) {
#ifndef NDEBUG
        TBDL << "Examining routes at location: " << location_name(tt_, fp.target())
             << " reached after walking " << fp.duration() << " minutes"
             << "\n";
#endif
        handle_start_footpath(d, tau, fp);
    }
}


// Algorithm 18: l: 10-27
void oneToAll_engine::handle_start_footpath(std::int32_t const d,
                                            std::int32_t const tau,
                                            footpath const fp) {
    // arrival time after walking the footpath
    auto const alpha = tau + fp.duration().count();
    auto const alpha_d = alpha / 1440;
    auto const alpha_tod = alpha % 1440;

    // iterate routes at target stop of footpath
    for (auto const route_idx : tt_.location_routes_[fp.target()]) {
        // iterate stop sequence of route, skip last stop
        for (std::uint16_t i = 0U; i < tt_.route_location_seq_[route_idx].size() - 1; ++i) {
            auto const route_stop = stop{tt_.route_location_seq_[route_idx][i]};
            if (route_stop.location_idx() == fp.target() && route_stop.in_allowed()) {
/*#ifndef NDEBUG
                TBDL "Route " << route_idx << " serves " << location_name(tt_, fp.target())
                              << " at stop idx = " << i << "\n";
#endif*/
                // departure times of this route at this q
                auto const event_times =
                        tt_.event_times_at_stop(route_idx, i, event_type::kDep);
                // iterator to departure time of connecting transport at this
                // q
                auto tau_dep_t_i =
                        std::lower_bound(event_times.begin(), event_times.end(), alpha_tod,
                                         [&](auto&& x, auto&& y) { return x.mam() < y; });
                // shift amount due to walking the footpath
                auto sigma = alpha_d;
                // no departure found on the day of alpha
                if (tau_dep_t_i == event_times.end()) {
                    // start looking at the following day
                    ++sigma;
                    tau_dep_t_i = event_times.begin();
                }
                // iterate departures until maximum waiting time is reached
                bitfield beta_l;
                while (sigma <= 1) {
                    // shift amount due to travel time of transport
                    std::int32_t const sigma_t = tau_dep_t_i->days();
                    // day index of the transport segment
                    auto const d_seg = d + sigma - sigma_t;
                    // offset of connecting transport in route_transport_ranges
                    auto const k = static_cast<std::size_t>(
                            std::distance(event_times.begin(), tau_dep_t_i));
                    // transport_idx_t of the connecting transport
                    auto const t = tt_.route_transport_ranges_[route_idx][k];
                    // bitfield of the connecting transport
                    auto const& beta_t = tt_.bitfields_[tt_.transport_traffic_days_[t]];

                    auto const& beta_l_new = (beta_t << static_cast<size_t>(sigma - sigma_t)) & (~beta_l);
                    beta_l |= (beta_t << static_cast<size_t>(sigma - sigma_t));
                    //if (beta_t.test(static_cast<std::size_t>(d_seg))) {
                        // enqueue segment if matching bit is found
                    if(beta_l_new > bitfield()) {
                      //TBDL << "Found new operating days\n";
                      //ToDo which d_seg? only sigma or d_seg, additional parameter sigma? for shift of next segment?
                      state_.q_n_.enqueue(static_cast<std::uint16_t>(d_seg), t,
                                          i, 0U, TRANSFERRED_FROM_NULL,
                                          beta_l_new);
                    }
                    //break;
                    //}
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
}