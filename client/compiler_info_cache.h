// Copyright 2015 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_COMPILER_INFO_CACHE_H_
#define DEVTOOLS_GOMA_CLIENT_COMPILER_INFO_CACHE_H_

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "autolock_timer.h"
#include "basictypes.h"
#include "cache_file.h"
#include "compiler_info.h"
#include "gtest/gtest_prod.h"
#include "json/json.h"
#include "lockhelper.h"

namespace devtools_goma {

class CompilerFlags;
class CompilerInfo;
class CompilerInfoState;
class CompilerInfoDataTable;

// CompilerInfoCache caches CompilerInfo.
// Information about a particular compiler found in 'path', with
// extra '-mxx' information.
// This class is thread-safe.
class CompilerInfoCache {
 public:
  struct Key {
    static const bool kCwdRelative = true;
    std::string base;
    std::string cwd;
    std::string local_compiler_path;

    std::string ToString(bool cwd_relative) const;
    std::string abs_local_compiler_path() const;
  };

  // CompilerInfoValidator just calls IsValid() of CompilerInfo.
  // You can set your own validator to test CompilerInfoCache.
  class CompilerInfoValidator {
  public:
    virtual ~CompilerInfoValidator() {}
    // Returns true if compiler_info cache is valid.
    virtual bool Validate(const CompilerInfo& compiler_info,
                          const std::string& local_compiler_path);
  };

  ~CompilerInfoCache();

  // Initializes the CompilerInfoCache.
  // Call LoadIfEnabled to load cache file.
  static void Init(const std::string& cache_dir,
                   const std::string& cache_filename,
                   int num_entries,
                   absl::Duration cache_holding_time);
  // Load cached data from
  // JoinPathRespectAbsolute(cache_dir, cache_filename),
  // when cache_filename is not empty.
  // Do nothing if cache_filename is empty.
  static void LoadIfEnabled();
  static CompilerInfoCache* instance() { return instance_; }

  // Saves CompilerInfoCache into cache file.
  static void Quit();

  static Key CreateKey(const CompilerFlags& flags,
                       const std::string& local_compiler_path,
                       const std::vector<std::string>& key_envs);

  // Lookup just checks cached compiler_info.
  // Returns CompilerInfoState in cache.
  // It would be better to use ScopedCompilerInfoState to manage the
  // returned pointer.
  //    ScopedCompilerInfoState cis(cache->Lookup(...));
  //
  // Note that found compiler_info may not be valid.
  // Returns nullptr if it missed in cache or found obsoleted.
  CompilerInfoState* Lookup(const Key& key);

  // Store stores compiler_info in cache and returns compiler_info_state.
  // compiler_info may be disabled if the same local compiler was already
  // disabled.
  // Takes ownership of data.
  CompilerInfoState* Store(const Key& key,
                           std::unique_ptr<CompilerInfoData> data);

  // Disable compiler_info_state and other compiler_info_states with
  // the same local compiler.
  bool Disable(CompilerInfoState* compiler_info_state,
               const std::string& disabled_reason);

  void Dump(std::ostringstream* ss);
  void DumpCompilersJSON(Json::Value* json);

  bool HasCompilerMismatch() const;

  int NumStores() const;
  int NumStoreDups() const;
  int NumMiss() const;
  int NumFail() const;
  int NumUsed() const;
  int Count() const;
  int LoadedSize() const;

  // Takes the ownership of validator.
  // Use this for testing purpose.
  void SetValidator(CompilerInfoValidator* validator) ABSL_LOCKS_EXCLUDED(mu_);
  CompilerInfoValidator* validator() const ABSL_LOCKS_EXCLUDED(mu_) {
    AUTO_SHARED_LOCK(lock, &mu_);
    return validator_.get();
  }

  bool Save() ABSL_LOCKS_EXCLUDED(mu_);

 private:
  FRIEND_TEST(CompilerInfoCacheTest, LimitTableEntriesTest);
  CompilerInfoCache(const std::string& cache_filename,
                    int num_entries,
                    absl::Duration cache_holding_time);

  static std::string HashKey(const CompilerInfoData& data);
  bool Load() ABSL_LOCKS_EXCLUDED(mu_);
  bool Unmarshal(const CompilerInfoDataTable& table) ABSL_LOCKS_EXCLUDED(mu_);
  bool UnmarshalUnlocked(const CompilerInfoDataTable& table)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  bool Marshal(CompilerInfoDataTable* table) ABSL_LOCKS_EXCLUDED(mu_);
  static void LimitTableEntries(CompilerInfoDataTable* table, int num_entries);
  bool MarshalUnlocked(CompilerInfoDataTable* table)
      ABSL_SHARED_LOCKS_REQUIRED(mu_);
  void Clear() ABSL_LOCKS_EXCLUDED(mu_);
  void ClearUnlocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  CompilerInfoState* LookupUnlocked(const std::string& compiler_info_key,
                                    const std::string& abs_local_compiler_path)
      ABSL_SHARED_LOCKS_REQUIRED(mu_);

  // Check CompilerInfo validity. CompilerInfo that does not match with the
  // current local compiler will be removed or updated.
  void UpdateOlderCompilerInfo() ABSL_LOCKS_EXCLUDED(mu_);
  void UpdateOlderCompilerInfoUnlocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  const absl::flat_hash_map<std::string, CompilerInfoState*> compiler_info() {
    AUTO_SHARED_LOCK(lock, &mu_);
    return compiler_info_;
  }

  const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>*>
  keys_by_hash() const {
    absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>*> ret;
    AUTO_SHARED_LOCK(lock, &mu_);
    for (const auto& it : keys_by_hash_) {
      ret.emplace(it.first, it.second.get());
    }
    return ret;
  }

  friend class CompilerInfoCacheTest;

  static CompilerInfoCache* instance_;

  const CacheFile cache_file_;
  const int max_num_entries_;
  const absl::Duration cache_holding_time_;

  std::unique_ptr<CompilerInfoValidator> validator_ ABSL_GUARDED_BY(mu_) =
      absl::make_unique<CompilerInfoCache::CompilerInfoValidator>();

  mutable ReadWriteLock mu_;

  // key: compiler_info_key
  absl::flat_hash_map<std::string, CompilerInfoState*> compiler_info_
      ABSL_GUARDED_BY(mu_);

  // key: hash of CompilerInfoData. value: compiler_info_key.
  absl::flat_hash_map<std::string,
                      std::unique_ptr<absl::flat_hash_set<std::string>>>
      keys_by_hash_ ABSL_GUARDED_BY(mu_);

  int num_stores_ ABSL_GUARDED_BY(mu_) = 0;
  int num_store_dups_ ABSL_GUARDED_BY(mu_) = 0;
  int num_miss_ ABSL_GUARDED_BY(mu_) = 0;
  int num_fail_ ABSL_GUARDED_BY(mu_) = 0;
  int loaded_size_ ABSL_GUARDED_BY(mu_) = 0;
  absl::Time loaded_timestamp_ ABSL_GUARDED_BY(mu_) = absl::Now();

  DISALLOW_COPY_AND_ASSIGN(CompilerInfoCache);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_COMPILER_INFO_CACHE_H_
