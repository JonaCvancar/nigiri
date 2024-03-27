#pragma once

#include "nigiri/common/interval.h"
#include "nigiri/routing/tripbased/settings.h"
#include "date/date.h"

#ifndef VBB_FROM
#define VBB_FROM (2023_y / December / 14)
#endif
#ifndef VBB_TO
#define VBB_TO (2024_y / December / 14)
#endif

#ifndef GERMANY_FROM
#define GERMANY_FROM (2022_y / November / 26)
#endif
#ifndef GERMANY_TO
#define GERMANY_TO (2022_y / December / 10)
#endif

#ifndef AACHEN_FROM
#define AACHEN_FROM (2023_y / June / 11)
#endif
#ifndef AACHEN_TO
#define AACHEN_TO (2024_y / June / 8)
#endif

namespace nigiri::routing::tripbased::onetoall {

constexpr interval<std::chrono::sys_days> aachen_period_oa() {
  using namespace date;
  constexpr auto const from = (AACHEN_FROM).operator sys_days();
  constexpr auto const to = (AACHEN_TO).operator sys_days();
  return {from, to};
}
constexpr interval<std::chrono::sys_days> vbb_period_oa() {
  using namespace date;
  constexpr auto const from = (VBB_FROM).operator sys_days();
  constexpr auto const to = (VBB_TO).operator sys_days();
  return {from, to};
}
constexpr interval<std::chrono::sys_days> germany_period_oa() {
  using namespace date;
  constexpr auto const from = (GERMANY_FROM).operator sys_days();
  constexpr auto const to = (GERMANY_TO).operator sys_days();
  return {from, to};
}
}  // namespace nigiri::routing::tripbased::performance