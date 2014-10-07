// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
#define MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "mojo/aura/window_tree_host_mojo.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/window_manager_delegate.h"
#include "mojo/services/window_manager/window_manager_service_impl.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
namespace client {
class ActivationClient;
class FocusClient;
}
class Window;
}

namespace wm {
class FocusRules;
class ScopedCaptureClient;
}

namespace mojo {

class AuraInit;
class DummyDelegate;
class WindowManagerServiceImpl;
class WindowTreeHostMojo;

// Implements core window manager functionality that could conceivably be shared
// across multiple window managers implementing superficially different user
// experiences. Establishes communication with the view manager.
// A window manager wishing to use this core should create and own an instance
// of this object. They may implement the associated ViewManager/WindowManager
// delegate interfaces exposed by the view manager, this object provides the
// canonical implementation of said interfaces but will call out to the wrapped
// instances.
// This object maintains an aura::WindowTreeHost containing a hierarchy of
// aura::Windows. Window manager functionality (e.g. focus, activation,
// modality, etc.) are implemented using aura core window manager components.
class WindowManagerApp
    : public ApplicationDelegate,
      public ViewManagerDelegate,
      public WindowManagerDelegate,
      public ViewObserver,
      public ui::EventHandler,
      public aura::client::FocusChangeObserver,
      public aura::client::ActivationChangeObserver {
 public:
  WindowManagerApp(ViewManagerDelegate* view_manager_delegate,
                   WindowManagerDelegate* window_manager_delegate);
  virtual ~WindowManagerApp();

  static View* GetViewForWindow(aura::Window* window);
  aura::Window* GetWindowForViewId(Id view);

  // Register/deregister new connections to the window manager service.
  void AddConnection(WindowManagerServiceImpl* connection);
  void RemoveConnection(WindowManagerServiceImpl* connection);

  // These are canonical implementations of the window manager API methods.
  void SetCapture(Id view);
  void FocusWindow(Id view);
  void ActivateWindow(Id view);

  bool IsReady() const;

  // A client of this object will use this accessor to gain access to the
  // aura::Window hierarchy and attach event handlers.
  aura::WindowTreeHost* host() { return window_tree_host_.get(); }

  void InitFocus(wm::FocusRules* rules);

  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* impl) override;
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) override;

 private:
  typedef std::set<WindowManagerServiceImpl*> Connections;
  typedef std::map<Id, aura::Window*> ViewIdToWindowMap;

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       View* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) override;
  virtual void OnViewManagerDisconnected(ViewManager* view_manager) override;

  // Overridden from WindowManagerDelegate:
  virtual void Embed(
      const String& url,
      InterfaceRequest<ServiceProvider> service_provider) override;
  virtual void DispatchEvent(EventPtr event) override;

  // Overridden from ViewObserver:
  virtual void OnTreeChanged(
      const ViewObserver::TreeChangeParams& params) override;
  virtual void OnViewDestroyed(View* view) override;
  virtual void OnViewBoundsChanged(View* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) override;

  // Overridden from ui::EventHandler:
  virtual void OnEvent(ui::Event* event) override;

  // Overridden from aura::client::FocusChangeObserver:
  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) override;

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) override;

  // Creates an aura::Window for every view in the hierarchy beneath |view|,
  // and adds to the registry so that it can be retrieved later via
  // GetWindowForViewId().
  // TODO(beng): perhaps View should have a property bag.
  void RegisterSubtree(View* view, aura::Window* parent);
  // Deletes the aura::Windows associated with the hierarchy beneath |id|,
  // and removes from the registry.
  void UnregisterSubtree(View* view);

  Shell* shell_;

  InterfaceFactoryImplWithContext<WindowManagerServiceImpl, WindowManagerApp>
      window_manager_service_factory_;

  ViewManagerDelegate* wrapped_view_manager_delegate_;
  WindowManagerDelegate* wrapped_window_manager_delegate_;

  ViewManager* view_manager_;
  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;
  View* root_;

  scoped_ptr<AuraInit> aura_init_;
  scoped_ptr<WindowTreeHostMojo> window_tree_host_;

  scoped_ptr<wm::ScopedCaptureClient> capture_client_;
  scoped_ptr<aura::client::FocusClient> focus_client_;
  aura::client::ActivationClient* activation_client_;

  Connections connections_;
  ViewIdToWindowMap view_id_to_window_map_;

  scoped_ptr<DummyDelegate> dummy_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApp);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_APP_H_
