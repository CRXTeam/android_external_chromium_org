// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_DATABASE_H_
#define CHROME_BROWSER_HISTORY_HISTORY_DATABASE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "chrome/browser/history/download_database.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/visit_database.h"
#include "chrome/browser/history/visitsegment_database.h"
#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

#if defined(OS_ANDROID)
#include "chrome/browser/history/android/android_cache_database.h"
#include "chrome/browser/history/android/android_urls_database.h"
#endif

namespace base {
class FilePath;
}

class HistoryQuickProviderTest;

namespace history {

// Encapsulates the SQL connection for the history database. This class holds
// the database connection and has methods the history system (including full
// text search) uses for writing and retrieving information.
//
// We try to keep most logic out of the history database; this should be seen
// as the storage interface. Logic for manipulating this storage layer should
// be in HistoryBackend.cc.
class HistoryDatabase : public DownloadDatabase,
#if defined(OS_ANDROID)
                        public AndroidURLsDatabase,
                        public AndroidCacheDatabase,
#endif
                        public URLDatabase,
                        public VisitDatabase,
                        public VisitSegmentDatabase {
 public:
  // A simple class for scoping a history database transaction. This does not
  // support rollback since the history database doesn't, either.
  class TransactionScoper {
   public:
    explicit TransactionScoper(HistoryDatabase* db) : db_(db) {
      db_->BeginTransaction();
    }
    ~TransactionScoper() {
      db_->CommitTransaction();
    }
   private:
    HistoryDatabase* db_;
  };

  // Must call Init() to complete construction. Although it can be created on
  // any thread, it must be destructed on the history thread for proper
  // database cleanup.
  HistoryDatabase();

  virtual ~HistoryDatabase();

  // Call before Init() to set the error callback to be used for the
  // underlying database connection.
  void set_error_callback(
      const sql::Connection::ErrorCallback& error_callback) {
    error_callback_ = error_callback;
  }

  // Must call this function to complete initialization. Will return
  // sql::INIT_OK on success. Otherwise, no other function should be called. You
  // may want to call BeginExclusiveMode after this when you are ready.
  sql::InitStatus Init(const base::FilePath& history_name);

  // Computes and records various metrics for the database. Should only be
  // called once and only upon successful Init.
  void ComputeDatabaseMetrics(const base::FilePath& filename);

  // Call to set the mode on the database to exclusive. The default locking mode
  // is "normal" but we want to run in exclusive mode for slightly better
  // performance since we know nobody else is using the database. This is
  // separate from Init() since the in-memory database attaches to slurp the
  // data out, and this can't happen in exclusive mode.
  void BeginExclusiveMode();

  // Returns the current version that we will generate history databases with.
  static int GetCurrentVersion();

  // Transactions on the history database. Use the Transaction object above
  // for most work instead of these directly. We support nested transactions
  // and only commit when the outermost transaction is committed. This means
  // that it is impossible to rollback a specific transaction. We could roll
  // back the outermost transaction if any inner one is rolled back, but it
  // turns out we don't really need this type of integrity for the history
  // database, so we just don't support it.
  void BeginTransaction();
  void CommitTransaction();
  int transaction_nesting() const {  // for debugging and assertion purposes
    return db_.transaction_nesting();
  }
  void RollbackTransaction();

  // Drops all tables except the URL, and download tables, and recreates them
  // from scratch. This is done to rapidly clean up stuff when deleting all
  // history. It is faster and less likely to have problems that deleting all
  // rows in the tables.
  //
  // We don't delete the downloads table, since there may be in progress
  // downloads. We handle the download history clean up separately in:
  // content::DownloadManager::RemoveDownloadsFromHistoryBetween.
  //
  // Returns true on success. On failure, the caller should assume that the
  // database is invalid. There could have been an error recreating a table.
  // This should be treated the same as an init failure, and the database
  // should not be used any more.
  //
  // This will also recreate the supplementary URL indices, since these
  // indices won't be created automatically when using the temporary URL
  // table (what the caller does right before calling this).
  bool RecreateAllTablesButURL();

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

  // Try to trim the cache memory used by the database.  If |aggressively| is
  // true try to trim all unused cache, otherwise trim by half.
  void TrimMemory(bool aggressively);

  // Razes the database. Returns true if successful.
  bool Raze();

  // Returns true if the history backend should erase the full text search
  // and archived history files as part of version 16 -> 17 migration. The
  // time format changed in this revision, and these files would be much slower
  // to migrate. Since the data is less important, they should be deleted.
  //
  // This flag will be valid after Init() is called. It will always be false
  // when running on Windows.
  bool needs_version_17_migration() const {
    return needs_version_17_migration_;
  }

  // Visit table functions ----------------------------------------------------

  // Update the segment id of a visit. Return true on success.
  bool SetSegmentID(VisitID visit_id, SegmentID segment_id);

  // Query the segment ID for the provided visit. Return 0 on failure or if the
  // visit id wasn't found.
  SegmentID GetSegmentID(VisitID visit_id);

  // Retrieves/Updates early expiration threshold, which specifies the earliest
  // known point in history that may possibly to contain visits suitable for
  // early expiration (AUTO_SUBFRAMES).
  virtual base::Time GetEarlyExpirationThreshold();
  virtual void UpdateEarlyExpirationThreshold(base::Time threshold);

 private:
#if defined(OS_ANDROID)
  // AndroidProviderBackend uses the |db_|.
  friend class AndroidProviderBackend;
  FRIEND_TEST_ALL_PREFIXES(AndroidURLsMigrationTest, MigrateToVersion22);
#endif
  friend class ::HistoryQuickProviderTest;
  friend class InMemoryURLIndexTest;

  // Overridden from URLDatabase:
  virtual sql::Connection& GetDB() OVERRIDE;

  // Migration -----------------------------------------------------------------

  // Makes sure the version is up-to-date, updating if necessary. If the
  // database is too old to migrate, the user will be notified. Returns
  // sql::INIT_OK iff  the DB is up-to-date and ready for use.
  //
  // This assumes it is called from the init function inside a transaction. It
  // may commit the transaction and start a new one if migration requires it.
  sql::InitStatus EnsureCurrentVersion();

#if !defined(OS_WIN)
  // Converts the time epoch in the database from being 1970-based to being
  // 1601-based which corresponds to the change in Time.internal_value_.
  void MigrateTimeEpoch();
#endif

  // ---------------------------------------------------------------------------

  sql::Connection::ErrorCallback error_callback_;
  sql::Connection db_;
  sql::MetaTable meta_table_;

  base::Time cached_early_expiration_threshold_;

  // See the getters above.
  bool needs_version_17_migration_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_DATABASE_H_
