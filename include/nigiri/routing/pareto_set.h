#pragma once

#include <cinttypes>
#include <tuple>
#include <vector>
#include <iostream>
#include "nigiri/types.h"

namespace nigiri {

template <typename T>
struct pareto_set {
  using iterator = typename std::vector<T>::iterator;
  using const_iterator = typename std::vector<T>::const_iterator;

  std::size_t size() const { return els_.size(); }

  std::tuple<bool, iterator, iterator> add(T&& el) {
    auto n_removed = std::size_t{0};
    for (auto i = 0U; i < els_.size(); ++i) {
      if (els_[i].dominates(el)) {
        return {false, end(), std::next(begin(), i)};
      }
      if (el.dominates(els_[i])) {
        n_removed++;
        continue;
      }
      els_[i - n_removed] = els_[i];
    }
    els_.resize(els_.size() - n_removed + 1);
    els_.back() = std::move(el);
    return {true, std::next(begin(), static_cast<unsigned>(els_.size() - 1)),
            end()};
  }

  std::tuple<bool, iterator, iterator> add_bitfield(T&& el) {
    auto n_removed = std::size_t{0};
    for (auto i = 0U; i < els_.size(); ++i) {
      if (els_[i].dominates(el)) {
        return {false, end(), std::next(begin(), i)};
      }
      if (el.dominates(els_[i])) {
        n_removed++;
        continue;
      }
      els_[i - n_removed] = els_[i];
    }
    els_.resize(els_.size() - n_removed + 1);
    els_.back() = std::move(el);
    return {true, std::next(begin(), static_cast<unsigned>(els_.size() - 1)),
            end()};
    /*
    bitfield non_dominated_days{};
    auto n_removed = std::size_t{0};
    for (auto i = 0U; i < els_.size(); ++i) {
      auto dom_res = els_[i].dominates(el);
      if (std::get<0>(dom_res) == bitfield()) {
        return {false, end(), std::next(begin(), i)};
      } else {
        el.bitfield_ = std::get<0>(dom_res);
      }
      if (std::get<1>(dom_res) == bitfield()) {
        n_removed++;
        continue;
      } else {
        els_[i].bitfield_ = std::get<1>(dom_res);
      }
      els_[i - n_removed] = els_[i];
    }
    els_.resize(els_.size() - n_removed + 1);
    els_.back() = std::move(el);
    return {true, std::next(begin(), static_cast<unsigned>(els_.size() - 1)),
            end()};
            */
  }

  friend const_iterator begin(pareto_set const& s) { return s.begin(); }
  friend const_iterator end(pareto_set const& s) { return s.end(); }
  friend iterator begin(pareto_set& s) { return s.begin(); }
  friend iterator end(pareto_set& s) { return s.end(); }
  iterator begin() { return els_.begin(); }
  iterator end() { return els_.end(); }
  const_iterator begin() const { return els_.begin(); }
  const_iterator end() const { return els_.end(); }
  iterator erase(iterator const& it) { return els_.erase(it); }
  iterator erase(iterator const& from, iterator const& to) {
    return els_.erase(from, to);
  }
  void clear() { els_.clear(); }

private:
  std::vector<T> els_;
};

}  // namespace nigiri
