#include "nigiri/loader/hrd/load_timetable.h"

#include <execution>

#include "wyhash.h"

#include "utl/enumerate.h"
#include "utl/helpers/algorithm.h"
#include "utl/pipes.h"
#include "utl/progress_tracker.h"

#include "nigiri/loader/hrd/service/service_builder.h"
#include "nigiri/loader/hrd/stamm/stamm.h"

namespace nigiri::loader::hrd {

bool applicable(config const& c, dir const& d) {
  return utl::all_of(
      c.required_files_, [&](std::vector<std::string> const& alt) {
        return alt.empty() || utl::any_of(alt, [&](std::string const& file) {
                 auto const exists = d.exists(c.core_data_ / file);
                 if (!exists) {
                   log(log_lvl::info, "loader.hrd",
                       "missing file for config {}: {}", c.version_.view(),
                       (c.core_data_ / file));
                 }
                 return exists;
               });
      });
}

std::uint64_t hash(config const& c, dir const& d, std::uint64_t const seed) {
  auto h = seed;
  for (auto const& f : stamm::load_files(c, d)) {
    if (!f.has_value()) {
      h = wyhash64(h, _wyp[0]);
    } else {
      auto const data = f.data();
      h = wyhash(data.data(), data.size(), h, _wyp);
    }
  }
  for (auto const& path : d.list_files(c.fplan_)) {
    auto const f = d.get_file(path);
    auto const data = f.data();
    h = wyhash(data.data(), data.size(), h, _wyp);
  }
  return h;
}

void load_timetable(source_idx_t const src,
                    config const& c,
                    dir const& d,
                    timetable& tt) {
  auto st = stamm{c, tt, d};

  auto progress_tracker = utl::get_active_progress_tracker();
  progress_tracker->status("Read Services")
      .in_high(utl::all(d.list_files(c.fplan_))  //
               | utl::transform([&](auto&& f) { return d.file_size(f); })  //
               | utl::sum());
  auto total_bytes_processed = std::uint64_t{0U};

  auto sb = service_builder{st, tt};
  for (auto const& path : d.list_files(c.fplan_)) {
    if (path.filename().generic_string().starts_with(".")) {
      continue;
    }

    log(log_lvl::info, "loader.hrd.services", "loading {}", path);
    auto const file = d.get_file(path);
    sb.add_services(
        c, relative(path, c.fplan_).string().c_str(), file.data(),
        [&](std::size_t const bytes_processed) {
          progress_tracker->update(total_bytes_processed + bytes_processed);
        });
    sb.write_services(src);
    total_bytes_processed += file.data().size();
  }
}

}  // namespace nigiri::loader::hrd
