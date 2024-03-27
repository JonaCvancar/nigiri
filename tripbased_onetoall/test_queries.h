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
#include "nigiri/routing/tripbased/AllToAll/onetoall_engine.h"
#include "nigiri/routing/tripbased/AllToAll/onetoall_queries.h"

#include "nigiri/routing/tripbased/query/query_state.h"
#include "nigiri/routing/tripbased/query/query_engine.h"

#include "nigiri/routing/raptor/raptor.h"
#include "nigiri/routing/search.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace nigiri::routing::tripbased::onetoall {

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

std::vector<std::string> get_locations(std::string file_path, char delimiter) {
  std::ifstream file(file_path); // Change "input.txt" to your file name

  std::vector<std::string> firstColumn;

  if (!file.is_open()) {
    std::cerr << "Error opening file!" << std::endl;
    return firstColumn;
  }

  bool first_line = true; // Flag to skip the first line

  std::string line;
  while (std::getline(file, line)) {
    if (first_line) {
      first_line = false;
      continue; // Skip the first line
    }
    std::istringstream iss(line);
    std::string element;
    if (std::getline(iss, element, delimiter)) {
      firstColumn.push_back(element);
    }
  }

  return firstColumn;
}

routing_result_oa<oneToAll_stats> tripbased_onetoall(
//std::vector<pareto_set<routing::journey_bitfield>> tripbased_onetoall(
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

  /*
  return *(routing::onetoall_search<direction::kForward, algo_oa_t>{
      tt, nullptr, search_state_oa, algo_state_oa, std::move(q)}
               .execute()
               .journeys_);
               */
  return (routing::onetoall_search<direction::kForward, algo_oa_t>{
      tt, nullptr, search_state_oa, algo_state_oa, std::move(q)}
               .execute());
}

void tripbased_onetoall_query(
    timetable& tt,
    pareto_set<routing::journey_bitfield>& oa,
    pareto_set<routing::journey>& results,
    routing::start_time_t time) {
  routing::tripbased::tripbased_onetoall_query(tt, oa, results, time);
}

pareto_set<routing::journey> tripbased_search(timetable& tt,
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

template <direction SearchDir>
pareto_set<routing::journey> raptor_search(timetable const& tt,
                                           rt_timetable const* rtt,
                                           routing::query q) {
  using algo_state_t = routing::raptor_state;
  static auto search_state = routing::search_state{};
  static auto algo_state = algo_state_t{};

  if (rtt == nullptr) {
    using algo_t = routing::raptor<SearchDir, false>;
    return *(routing::search<SearchDir, algo_t>{tt, rtt, search_state,
                                                algo_state, std::move(q)}
                 .execute()
                 .journeys_);
  } else {
    using algo_t = routing::raptor<SearchDir, true>;
    return *(routing::search<SearchDir, algo_t>{tt, rtt, search_state,
                                                algo_state, std::move(q)}
                 .execute()
                 .journeys_);
  }
}

pareto_set<routing::journey> raptor_search(timetable const& tt,
                                           rt_timetable const* rtt,
                                           routing::query q,
                                           direction const search_dir) {
  if (search_dir == direction::kForward) {
    return raptor_search<direction::kForward>(tt, rtt, std::move(q));
  } else {
    return raptor_search<direction::kBackward>(tt, rtt, std::move(q));
  }
}

pareto_set<routing::journey> raptor_search(timetable const& tt,
                                           rt_timetable const* rtt,
                                           location_idx_t from,
                                           location_idx_t to,
                                           routing::start_time_t time,
                                           direction const search_dir = direction::kForward) {
  auto const src = source_idx_t{0};
  auto q = routing::query{
      .start_time_ = time,
      .start_ = {{from, 0_minutes,
                  0U}},
      .destination_ = {
          {to, 0_minutes, 0U}}};
  return raptor_search(tt, rtt, std::move(q), search_dir);
}

}
