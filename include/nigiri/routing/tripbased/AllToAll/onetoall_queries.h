#pragma once

namespace nigiri {
struct timetable;
}

namespace nigiri::routing::tripbased {

void tripbased_onetoall_query(
    timetable& tt,
    pareto_set<routing::journey_bitfield> const& oa,
    pareto_set<routing::journey>& results,
    routing::start_time_t time) {

  interval<unixtime_t> search_interval{std::visit(
      utl::overloaded{
          [](interval<unixtime_t> const start_interval) {
            return start_interval;
          },
          [](unixtime_t const start_time) {
            return interval<unixtime_t>{start_time, start_time};
          }},
      time)};

  auto const d_idx_mam_start = tt.day_idx_mam(search_interval.from_);
  auto const d_idx_mam_end = tt.day_idx_mam(search_interval.to_);
  auto const d_start = d_idx_mam_start.first.v_;
  auto const mam_start = d_idx_mam_start.second.count();
  auto const d_end = d_idx_mam_end.first.v_;
  auto const mam_end = d_idx_mam_end.second.count();

  if(d_idx_mam_start == d_idx_mam_end) {
    auto it = std::lower_bound(oa.begin(), oa.end(), mam_start,
                               [&](auto&& x, auto&& y) { return tt.day_idx_mam(x.start_time_).second.count() < y; });
    std::vector<routing::journey_bitfield> temp_oa;
    std::copy(it, oa.end(), std::back_inserter(temp_oa));
    utl::sort(temp_oa, [](journey_bitfield const& a, journey_bitfield const& b) {
      return std::tie(a.dest_time_, a.start_time_) < std::tie(b.dest_time_, b.start_time_);
    });

    int transfers = kMaxTransfers;
    for(auto const& journey : temp_oa) {
      if(journey.bitfield_.test(static_cast<std::size_t>(d_start))) {
        if(tt.day_idx_mam(journey.start_time_).second.count() > tt.day_idx_mam(journey.dest_time_).second.count()) {
          if(tt.day_idx_mam(journey.dest_time_).second.count() > mam_start) {
            continue;
          }
        }
        if(transfers > journey.transfers_) {
          routing::journey j{};
          j.start_time_ = journey.start_time_;
          j.dest_time_ = journey.dest_time_;
          j.dest_ = journey.dest_;
          j.transfers_ = journey.transfers_;
          results.add(std::move(j));
          transfers = journey.transfers_;
        }
        if(transfers == 0) {
          break;
        }
      }
    }
    utl::sort(results, [](journey const& a, journey const& b) {
      return a.transfers_ < b.transfers_;
    });

    //TBDL << "Earliest arrival query.\n";
  } else {
    //TBDL << "Profile query.\n";
    for(auto const& journey : oa) {
      if(journey.bitfield_.test(static_cast<std::size_t>(d_start))) {
        if(tt.day_idx_mam(journey.start_time_).second.count() >= mam_start && tt.day_idx_mam(journey.start_time_).second.count() < mam_end) {
          routing::journey j{};
          j.start_time_ = journey.start_time_;
          j.dest_time_ = journey.dest_time_;
          j.dest_ = journey.dest_;
          j.transfers_ = journey.transfers_;
          results.add(std::move(j));
        }
      }
    }
  }
}

}
