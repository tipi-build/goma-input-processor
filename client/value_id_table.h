// Copyright 2022 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_VALUE_ID_TABLE_H_
#define DEVTOOLS_GOMA_CLIENT_VALUE_ID_TABLE_H_

#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "autolock_timer.h"
#include "glog/logging.h"

namespace devtools_goma {

// ValueIDTable holds values and issues IDs for each value. This class doesn't
// have multiple instances of the same value and that can be used to reduce
// memory usage by using issued IDs instead of holding multiple instances of the
// same value in memory. This class is thread-safe.
template <typename T>
class ValueIDTable {
 public:
  using Id = int;

  // GetId returns the ID of given |value|. If |value| is not stored, new ID is
  // returned.
  Id GetId(const T& value) ABSL_LOCKS_EXCLUDED(&mu_) {
    {
      AUTO_SHARED_LOCK(lock, &mu_);
      auto v = map_to_id_.find(value);
      if (v != map_to_id_.end()) {
        return v->second;
      }
    }

    AUTO_EXCLUSIVE_LOCK(lock, &mu_);
    auto v = map_to_id_.try_emplace(value, values_.size());
    if (v.second) {
      values_.push_back(value);
    }
    return v.first->second;
  }

  // GetValue returns the corresponding value for the given ID.
  T GetValue(const Id id) const ABSL_LOCKS_EXCLUDED(&mu_) {
    AUTO_SHARED_LOCK(lock, &mu_);
    DCHECK_LT(id, values_.size());
    return values_[id];
  }

  // Clear removes all elements from ValueIDTable.
  void Clear() ABSL_LOCKS_EXCLUDED(&mu_) {
    AUTO_EXCLUSIVE_LOCK(lock, &mu_);
    values_.clear();
    map_to_id_.clear();
  }

 private:
  mutable ReadWriteLock mu_;
  std::vector<T> values_ ABSL_GUARDED_BY(mu_);
  absl::flat_hash_map<T, Id> map_to_id_ ABSL_GUARDED_BY(mu_);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_VALUE_ID_TABLE_H_
