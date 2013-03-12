// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

class RenderViewHost;
class WebContents;

// Describes interface for managing devtools agents from browser process.
class CONTENT_EXPORT DevToolsAgentHost
    : public base::RefCounted<DevToolsAgentHost> {
 public:
  // Returns DevToolsAgentHost that can be used for inspecting |rvh|.
  // New DevToolsAgentHost will be created if it does not exist.
  static scoped_refptr<DevToolsAgentHost> GetFor(RenderViewHost* rvh);

  // Returns true iff an instance of DevToolsAgentHost for the |rvh|
  // does exist.
  static bool HasFor(RenderViewHost* rvh);

  // Returns DevToolsAgentHost that can be used for inspecting shared worker
  // with given worker process host id and routing id.
  static scoped_refptr<DevToolsAgentHost> GetForWorker(int worker_process_id,
                                                       int worker_route_id);

  static bool IsDebuggerAttached(WebContents* web_contents);

  // Detaches given |rvh| from the agent host temporarily and returns the agent
  // host id that allows to reattach another rvh to that agent host later.
  // Returns empty string if there is no agent host associated with the |rvh|.
  static std::string DisconnectRenderViewHost(RenderViewHost* rvh);

  // Reattaches agent host detached with DisconnectRenderViewHost method above
  // to |rvh|.
  static void ConnectRenderViewHost(const std::string& agent_host_cookie,
                                    RenderViewHost* rvh);

  // Returns a list of all existing RenderViewHost's that can be debugged.
  static std::vector<RenderViewHost*> GetValidRenderViewHosts();

  // Returns the unique id of the agent.
  virtual std::string GetId() = 0;

  // Returns render view host instance for this host if any.
  virtual RenderViewHost* GetRenderViewHost() = 0;

 protected:
  friend class base::RefCounted<DevToolsAgentHost>;
  virtual ~DevToolsAgentHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
