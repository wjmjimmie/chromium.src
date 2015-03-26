// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/service_worker_handler.h"

#include "base/bind.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_watcher.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace content {
namespace devtools {
namespace service_worker {

using Response = DevToolsProtocolClient::Response;

namespace {

void ResultNoOp(bool success) {
}
void StatusNoOp(ServiceWorkerStatusCode status) {
}

const std::string GetVersionRunningStatusString(
    content::ServiceWorkerVersion::RunningStatus running_status) {
  switch (running_status) {
    case content::ServiceWorkerVersion::STOPPED:
      return kServiceWorkerVersionRunningStatusStopped;
    case content::ServiceWorkerVersion::STARTING:
      return kServiceWorkerVersionRunningStatusStarting;
    case content::ServiceWorkerVersion::RUNNING:
      return kServiceWorkerVersionRunningStatusRunning;
    case content::ServiceWorkerVersion::STOPPING:
      return kServiceWorkerVersionRunningStatusStopping;
  }
  return "";
}

const std::string GetVersionStatusString(
    content::ServiceWorkerVersion::Status status) {
  switch (status) {
    case content::ServiceWorkerVersion::NEW:
      return kServiceWorkerVersionStatusNew;
    case content::ServiceWorkerVersion::INSTALLING:
      return kServiceWorkerVersionStatusInstalling;
    case content::ServiceWorkerVersion::INSTALLED:
      return kServiceWorkerVersionStatusInstalled;
    case content::ServiceWorkerVersion::ACTIVATING:
      return kServiceWorkerVersionStatusActivating;
    case content::ServiceWorkerVersion::ACTIVATED:
      return kServiceWorkerVersionStatusActivated;
    case content::ServiceWorkerVersion::REDUNDANT:
      return kServiceWorkerVersionStatusRedundant;
  }
  return "";
}

scoped_refptr<ServiceWorkerVersion> CreateVersionDictionaryValue(
    const ServiceWorkerVersionInfo& version_info) {
  scoped_refptr<ServiceWorkerVersion> version(
      ServiceWorkerVersion::Create()
          ->set_version_id(base::Int64ToString(version_info.version_id))
          ->set_registration_id(
                base::Int64ToString(version_info.registration_id))
          ->set_script_url(version_info.script_url.spec())
          ->set_running_status(
                GetVersionRunningStatusString(version_info.running_status))
          ->set_status(GetVersionStatusString(version_info.status)));
  return version;
}

scoped_refptr<ServiceWorkerRegistration> CreateRegistrationDictionaryValue(
    const ServiceWorkerRegistrationInfo& registration_info) {
  scoped_refptr<ServiceWorkerRegistration> registration(
      ServiceWorkerRegistration::Create()
          ->set_registration_id(
                base::Int64ToString(registration_info.registration_id))
          ->set_scope_url(registration_info.pattern.spec())
          ->set_is_deleted(registration_info.delete_flag ==
                           ServiceWorkerRegistrationInfo::IS_DELETED));
  return registration;
}

scoped_refptr<ServiceWorkerDevToolsAgentHost> GetMatchingServiceWorker(
    const ServiceWorkerDevToolsAgentHost::List& agent_hosts,
    const GURL& url) {
  scoped_refptr<ServiceWorkerDevToolsAgentHost> best_host;
  std::string best_scope;
  for (auto host : agent_hosts) {
    if (host->GetURL().host() != url.host())
      continue;
    std::string path = host->GetURL().path();
    std::string file = host->GetURL().ExtractFileName();
    std::string scope = path.substr(0, path.length() - file.length());
    if (scope.length() > best_scope.length()) {
      best_host = host;
      best_scope = scope;
    }
  }
  return best_host;
}

ServiceWorkerDevToolsAgentHost::Map GetMatchingServiceWorkers(
    const std::set<GURL>& urls) {
  ServiceWorkerDevToolsAgentHost::List agent_hosts;
  ServiceWorkerDevToolsManager::GetInstance()->
      AddAllAgentHosts(&agent_hosts);
  ServiceWorkerDevToolsAgentHost::Map result;
  for (const GURL& url : urls) {
    scoped_refptr<ServiceWorkerDevToolsAgentHost> host =
        GetMatchingServiceWorker(agent_hosts, url);
    if (host)
      result[host->GetId()] = host;
  }
  return result;
}

bool CollectURLs(std::set<GURL>* urls, FrameTreeNode* tree_node) {
  urls->insert(tree_node->current_url());
  return false;
}

void StopServiceWorkerOnIO(scoped_refptr<ServiceWorkerContextWrapper> context,
                           int64 version_id) {
  ServiceWorkerContextCore* context_core = context->context();
  if (!context_core)
    return;
  if (content::ServiceWorkerVersion* version =
          context_core->GetLiveVersion(version_id)) {
    version->StopWorker(base::Bind(&StatusNoOp));
  }
}

void GetDevToolsRouteInfoOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    int64 version_id,
    const base::Callback<void(int, int)>& callback) {
  ServiceWorkerContextCore* context_core = context->context();
  if (!context_core)
    return;
  if (content::ServiceWorkerVersion* version =
          context_core->GetLiveVersion(version_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            callback, version->embedded_worker()->process_id(),
            version->embedded_worker()->worker_devtools_agent_route_id()));
  }
}

Response CreateContextErrorResoponse() {
  return Response::InternalError("Could not connect to the context");
}

Response CreateInvalidVersionIdErrorResoponse() {
  return Response::InternalError("Invalid version ID");
}

}  // namespace

ServiceWorkerHandler::ServiceWorkerHandler()
    : enabled_(false), weak_factory_(this) {
}

ServiceWorkerHandler::~ServiceWorkerHandler() {
  Disable();
}

void ServiceWorkerHandler::SetRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  render_frame_host_ = render_frame_host;
  // Do not call UpdateHosts yet, wait for load to commit.
  if (!render_frame_host) {
    context_ = nullptr;
    return;
  }
  StoragePartition* partition = BrowserContext::GetStoragePartition(
      render_frame_host->GetProcess()->GetBrowserContext(),
      render_frame_host->GetSiteInstance());
  DCHECK(partition);
  context_ = static_cast<ServiceWorkerContextWrapper*>(
      partition->GetServiceWorkerContext());
}

void ServiceWorkerHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void ServiceWorkerHandler::UpdateHosts() {
  if (!enabled_)
    return;

  urls_.clear();
  if (render_frame_host_) {
    render_frame_host_->frame_tree_node()->frame_tree()->ForEach(
        base::Bind(&CollectURLs, &urls_));
  }

  ServiceWorkerDevToolsAgentHost::Map old_hosts = attached_hosts_;
  ServiceWorkerDevToolsAgentHost::Map new_hosts =
      GetMatchingServiceWorkers(urls_);

  for (auto pair : old_hosts) {
    if (new_hosts.find(pair.first) == new_hosts.end())
      ReportWorkerTerminated(pair.second.get());
  }

  for (auto pair : new_hosts) {
    if (old_hosts.find(pair.first) == old_hosts.end())
      ReportWorkerCreated(pair.second.get());
  }
}

void ServiceWorkerHandler::Detached() {
  Disable();
}

Response ServiceWorkerHandler::Enable() {
  if (enabled_)
    return Response::OK();
  if (!context_)
    return Response::InternalError("Could not connect to the context");
  enabled_ = true;

  ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);

  context_watcher_ = new ServiceWorkerContextWatcher(
      context_, base::Bind(&ServiceWorkerHandler::OnWorkerRegistrationUpdated,
                           weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerHandler::OnWorkerVersionUpdated,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerHandler::OnErrorReported,
                 weak_factory_.GetWeakPtr()));
  context_watcher_->Start();

  UpdateHosts();
  return Response::OK();
}

Response ServiceWorkerHandler::Disable() {
  if (!enabled_)
    return Response::OK();
  enabled_ = false;

  ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
  for (const auto& pair : attached_hosts_)
    pair.second->DetachClient();
  attached_hosts_.clear();
  DCHECK(context_watcher_);
  context_watcher_->Stop();
  context_watcher_ = nullptr;
  return Response::OK();
}

Response ServiceWorkerHandler::SendMessage(
    const std::string& worker_id,
    const std::string& message) {
  auto it = attached_hosts_.find(worker_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not connected to the worker");
  it->second->DispatchProtocolMessage(message);
  return Response::OK();
}

Response ServiceWorkerHandler::Stop(
    const std::string& worker_id) {
  auto it = attached_hosts_.find(worker_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not connected to the worker");
  it->second->UnregisterWorker();
  return Response::OK();
}

Response ServiceWorkerHandler::Unregister(const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResoponse();
  context_->UnregisterServiceWorker(GURL(scope_url), base::Bind(&ResultNoOp));
  return Response::OK();
}

Response ServiceWorkerHandler::StartWorker(const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResoponse();
  context_->StartServiceWorker(GURL(scope_url), base::Bind(&StatusNoOp));
  return Response::OK();
}

Response ServiceWorkerHandler::StopWorker(const std::string& version_id) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResoponse();
  int64 id = 0;
  if (!base::StringToInt64(version_id, &id))
    return CreateInvalidVersionIdErrorResoponse();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&StopServiceWorkerOnIO, context_, id));
  return Response::OK();
}

Response ServiceWorkerHandler::InspectWorker(const std::string& version_id) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResoponse();

  int64 id = 0;
  if (!base::StringToInt64(version_id, &id))
    return CreateInvalidVersionIdErrorResoponse();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetDevToolsRouteInfoOnIO, context_, id,
                 base::Bind(&ServiceWorkerHandler::OpenNewDevToolsWindow,
                            weak_factory_.GetWeakPtr())));
  return Response::OK();
}

void ServiceWorkerHandler::OpenNewDevToolsWindow(int process_id,
                                                 int devtools_agent_route_id) {
  scoped_refptr<DevToolsAgentHostImpl> agent_host(
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(process_id, devtools_agent_route_id));
  if (!agent_host.get())
    return;
  agent_host->Inspect(render_frame_host_->GetProcess()->GetBrowserContext());
}

void ServiceWorkerHandler::OnWorkerRegistrationUpdated(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registration_values;
  for (const auto& registration : registrations) {
    registration_values.push_back(
        CreateRegistrationDictionaryValue(registration));
  }
  client_->WorkerRegistrationUpdated(
      WorkerRegistrationUpdatedParams::Create()->set_registrations(
          registration_values));
}

void ServiceWorkerHandler::OnWorkerVersionUpdated(
    const std::vector<ServiceWorkerVersionInfo>& versions) {
  std::vector<scoped_refptr<ServiceWorkerVersion>> version_values;
  for (const auto& version : versions) {
    version_values.push_back(CreateVersionDictionaryValue(version));
  }
  client_->WorkerVersionUpdated(
      WorkerVersionUpdatedParams::Create()->set_versions(version_values));
}

void ServiceWorkerHandler::OnErrorReported(
    int64 registration_id,
    int64 version_id,
    const ServiceWorkerContextObserver::ErrorInfo& info) {
  client_->WorkerErrorReported(
      WorkerErrorReportedParams::Create()->set_error_message(
          ServiceWorkerErrorMessage::Create()
              ->set_error_message(base::UTF16ToUTF8(info.error_message))
              ->set_registration_id(base::Int64ToString(registration_id))
              ->set_version_id(base::Int64ToString(version_id))
              ->set_source_url(info.source_url.spec())
              ->set_line_number(info.line_number)
              ->set_column_number(info.column_number)));
}

void ServiceWorkerHandler::DispatchProtocolMessage(
    DevToolsAgentHost* host,
    const std::string& message) {

  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;  // Already disconnected.

  client_->DispatchMessage(
      DispatchMessageParams::Create()->
          set_worker_id(host->GetId())->
          set_message(message));
}

void ServiceWorkerHandler::AgentHostClosed(
    DevToolsAgentHost* host,
    bool replaced_with_another_client) {
  client_->WorkerTerminated(WorkerTerminatedParams::Create()->
      set_worker_id(host->GetId()));
  attached_hosts_.erase(host->GetId());
}

void ServiceWorkerHandler::WorkerCreated(
    ServiceWorkerDevToolsAgentHost* host) {
  auto hosts = GetMatchingServiceWorkers(urls_);
  if (hosts.find(host->GetId()) != hosts.end() && !host->IsAttached())
    host->PauseForDebugOnStart();
}

void ServiceWorkerHandler::WorkerReadyForInspection(
    ServiceWorkerDevToolsAgentHost* host) {
  UpdateHosts();
}

void ServiceWorkerHandler::WorkerDestroyed(
    ServiceWorkerDevToolsAgentHost* host) {
  UpdateHosts();
}

void ServiceWorkerHandler::ReportWorkerCreated(
    ServiceWorkerDevToolsAgentHost* host) {
  if (host->IsAttached())
    return;
  attached_hosts_[host->GetId()] = host;
  host->AttachClient(this);
  client_->WorkerCreated(WorkerCreatedParams::Create()->
      set_worker_id(host->GetId())->
      set_url(host->GetURL().spec()));
}

void ServiceWorkerHandler::ReportWorkerTerminated(
    ServiceWorkerDevToolsAgentHost* host) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;
  host->DetachClient();
  client_->WorkerTerminated(WorkerTerminatedParams::Create()->
      set_worker_id(host->GetId()));
  attached_hosts_.erase(it);
}

}  // namespace service_worker
}  // namespace devtools
}  // namespace content
