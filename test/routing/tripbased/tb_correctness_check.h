#pragma once

#include "nigiri/routing/journey_bitfield.h"
#include "nigiri/routing/onetoall_search.h"
#include "nigiri/routing/journey.h"
#include "nigiri/routing/search.h"
#include "nigiri/timetable.h"

#include "nigiri/routing/debug.h"
#include "nigiri/routing/pareto_set.h"
#include "nigiri/routing/tripbased/preprocessing/preprocessor.h"
#include "nigiri/routing/tripbased/transfer_set.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_state.h"

namespace nigiri::test {

std::vector<std::string> get_locations(std::string file_path) {
  std::ifstream file(file_path); // Change "input.txt" to your file name

  std::vector<std::string> firstColumn;

  if (!file.is_open()) {
    std::cerr << "Error opening file!" << std::endl;
    return firstColumn;
  }

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string element;
    if (iss >> element) {
      firstColumn.push_back(element);
    }
  }

  return firstColumn;
}

void tripbased_onetoall_correctness(
//void tripbased_onetoall_correctness(
    timetable& tt,
    pareto_set<routing::journey_bitfield> const& oa,
    pareto_set<routing::journey>& results,
    routing::start_time_t time) {
  routing::tripbased::tripbased_onetoall_query(tt, oa, results, time);
}

routing::routing_result_oa<routing::tripbased::oneToAll_stats> tripbased_onetoall_query(
// std::vector<pareto_set<routing::journey_bitfield>> tripbased_onetoall_query(
    timetable& tt,
    routing::tripbased::transfer_set& ts,
    location_idx_t from,
    routing::start_time_t time) {
  using algo_oa_t = routing::tripbased::oneToAll_engine;
  using algo_state_oa_t = routing::tripbased::oneToAll_state;

  static auto search_state_oa = routing::onetoall_search_state{};
  auto algo_state_oa = algo_state_oa_t{tt, ts};

  auto const src = source_idx_t{0};
  routing::onetoall_query q;
  q = routing::onetoall_query{.start_time_ = time,
                              .start_ = {{from, 0_minutes, 0U}}};

  return (routing::onetoall_search<direction::kForward, algo_oa_t>{
      tt, nullptr, search_state_oa, algo_state_oa, std::move(q)}
               .execute());
               //.journeys_);
}

pareto_set<routing::journey> tripbased_search_correctness(timetable& tt,
                                              routing::tripbased::transfer_set& ts,
                                              location_idx_t from,
                                              location_idx_t to,
                                              routing::start_time_t time) {
  using algo_tb_t = routing::tripbased::query_engine;
  using algo_state_tb_t = routing::tripbased::query_state;

  static auto search_state_tb = routing::search_state{};
  auto algo_state_tb = algo_state_tb_t{tt, ts};

  auto q = routing::query{
      .start_time_ = time,
      .start_ = {{from, 0_minutes, 0U}},
      .destination_ = {{to, 0_minutes, 0U}}};

  return *(routing::search<direction::kForward, algo_tb_t>{
      tt, nullptr, search_state_tb, algo_state_tb, std::move(q)}
               .execute()
               .journeys_);
}

}
