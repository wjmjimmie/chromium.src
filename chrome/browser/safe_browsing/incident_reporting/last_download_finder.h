// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_LAST_DOWNLOAD_FINDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_LAST_DOWNLOAD_FINDER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/safe_browsing/incident_reporting/download_metadata_manager.h"
#include "components/history/core/browser/history_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class HistoryService;
class Profile;

namespace content {
class NotificationDetails;
class NotificationSource;
}

namespace history {
struct DownloadRow;
}

namespace safe_browsing {

class ClientIncidentReport_DownloadDetails;

// Finds the most recent executable downloaded by any on-the-record profile with
// history that participates in safe browsing.
class LastDownloadFinder : public content::NotificationObserver,
                           public history::HistoryServiceObserver {
 public:
  typedef base::Callback<void(
      content::BrowserContext* context,
      const DownloadMetadataManager::GetDownloadDetailsCallback&)>
      DownloadDetailsGetter;

  // The type of a callback run by the finder upon completion. The argument is a
  // protobuf containing details of the download that was found, or an empty
  // pointer if none was found.
  typedef base::Callback<void(scoped_ptr<ClientIncidentReport_DownloadDetails>)>
      LastDownloadCallback;

  ~LastDownloadFinder() override;

  // Initiates an asynchronous search for the most recent download. |callback|
  // will be run when the search is complete. The returned instance can be
  // deleted to terminate the search, in which case |callback| is not invoked.
  // Returns NULL without running |callback| if there are no eligible profiles
  // to search.
  static scoped_ptr<LastDownloadFinder> Create(
      const DownloadDetailsGetter& download_details_getter,
      const LastDownloadCallback& callback);

 protected:
  // Protected constructor so that unit tests can create a fake finder.
  LastDownloadFinder();

 private:
  enum ProfileWaitState {
    WAITING_FOR_METADATA,
    WAITING_FOR_HISTORY,
  };

  LastDownloadFinder(const DownloadDetailsGetter& download_details_getter,
                     const std::vector<Profile*>& profiles,
                     const LastDownloadCallback& callback);

  // Adds |profile| to the set of profiles to be searched if it is an
  // on-the-record profile with history that participates in safe browsing. A
  // search for metadata is initiated immediately.
  void SearchInProfile(Profile* profile);

  // DownloadMetadataManager::GetDownloadDetailsCallback. If |details| are
  // provided, retrieves them if they are the most relevant results. Otherwise
  // begins a search in history. Reports results if there are no more pending
  // queries.
  void OnMetadataQuery(
      Profile* profile,
      scoped_ptr<ClientIncidentReport_DownloadDetails> details);

  // Abandons the search for downloads in |profile|, reporting results if there
  // are no more pending queries.
  void AbandonSearchInProfile(Profile* profile);

  // HistoryService::DownloadQueryCallback. Retrieves the most recent completed
  // executable download from |downloads| and reports results if there are no
  // more pending queries.
  void OnDownloadQuery(
      Profile* profile,
      scoped_ptr<std::vector<history::DownloadRow> > downloads);

  // Removes the profile pointed to by |it| from profile_states_ and reports
  // results if there are no more pending queries.
  void RemoveProfileAndReportIfDone(
      std::map<Profile*, ProfileWaitState>::iterator iter);

  // Invokes the caller-supplied callback with the download found.
  void ReportResults();

  // content::NotificationObserver methods.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // history::HistoryServiceObserver:
  void OnHistoryServiceLoaded(HistoryService* service) override;
  void HistoryServiceBeingDeleted(HistoryService* history_service) override;

  // Caller-supplied callback to make an asynchronous request for a profile's
  // persistent download details.
  DownloadDetailsGetter download_details_getter_;

  // Caller-supplied callback to be invoked when the most recent download is
  // found.
  LastDownloadCallback callback_;

  // A mapping of profiles for which a download query is pending to their
  // respective states.
  std::map<Profile*, ProfileWaitState> profile_states_;

  // Registrar for observing profile lifecycle notifications.
  content::NotificationRegistrar notification_registrar_;

  // The most interesting download details retrieved from download metadata.
  scoped_ptr<ClientIncidentReport_DownloadDetails> details_;

  // The most recent download, updated progressively as query results arrive.
  history::DownloadRow most_recent_row_;

  // HistoryServiceObserver
  ScopedObserver<HistoryService, HistoryServiceObserver>
      history_service_observer_;

  // A factory for asynchronous operations on profiles' HistoryService.
  base::WeakPtrFactory<LastDownloadFinder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LastDownloadFinder);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_LAST_DOWNLOAD_FINDER_H_
