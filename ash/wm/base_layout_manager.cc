// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_layout_manager.h"

#include "ash/screen_util.h"
#include "ash/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/corewm/window_util.h"

namespace ash {
namespace internal {

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, public:

BaseLayoutManager::BaseLayoutManager(aura::Window* root_window)
    : root_window_(root_window) {
  Shell::GetInstance()->activation_client()->AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
  root_window_->AddObserver(this);
}

BaseLayoutManager::~BaseLayoutManager() {
  if (root_window_)
    root_window_->RemoveObserver(this);
  for (WindowSet::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
  Shell::GetInstance()->activation_client()->RemoveObserver(this);
}

// static
gfx::Rect BaseLayoutManager::BoundsWithScreenEdgeVisible(
    aura::Window* window,
    const gfx::Rect& restore_bounds) {
  gfx::Rect max_bounds =
      ash::ScreenUtil::GetMaximizedWindowBoundsInParent(window);
  // If the restore_bounds are more than 1 grid step away from the size the
  // window would be when maximized, inset it.
  max_bounds.Inset(ash::internal::WorkspaceWindowResizer::kScreenEdgeInset,
                   ash::internal::WorkspaceWindowResizer::kScreenEdgeInset);
  if (restore_bounds.Contains(max_bounds))
    return max_bounds;
  return restore_bounds;
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, aura::LayoutManager overrides:

void BaseLayoutManager::OnWindowResized() {
}

void BaseLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  windows_.insert(child);
  child->AddObserver(this);
  wm::WindowState* window_state = wm::GetWindowState(child);
  window_state->AddObserver(this);

  // Only update the bounds if the window has a show state that depends on the
  // workspace area.
  if (window_state->IsMaximizedOrFullscreen())
    UpdateBoundsFromShowType(window_state);
}

void BaseLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  wm::GetWindowState(child)->RemoveObserver(this);
}

void BaseLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
}

void BaseLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visible) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  // Attempting to show a minimized window. Unminimize it.
  if (visible && window_state->IsMinimized())
    window_state->Unminimize();
}

void BaseLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  gfx::Rect child_bounds(requested_bounds);
  wm::WindowState* window_state = wm::GetWindowState(child);
  // Some windows rely on this to set their initial bounds.
  if (window_state->IsMaximized())
    child_bounds = ScreenUtil::GetMaximizedWindowBoundsInParent(child);
  else if (window_state->IsFullscreen())
    child_bounds = ScreenUtil::GetDisplayBoundsInParent(child);
  SetChildBoundsDirect(child, child_bounds);
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, aura::WindowObserver overrides:

void BaseLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == window) {
    root_window_->RemoveObserver(this);
    root_window_ = NULL;
  }
}

void BaseLayoutManager::OnWindowBoundsChanged(aura::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  if (root_window_ == window)
    AdjustAllWindowsBoundsForWorkAreaChange(ADJUST_WINDOW_DISPLAY_SIZE_CHANGED);
}

//////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, aura::client::ActivationChangeObserver implementation:

void BaseLayoutManager::OnWindowActivated(aura::Window* gained_active,
                                          aura::Window* lost_active) {
  wm::WindowState* window_state = wm::GetWindowState(gained_active);
  if (window_state && window_state->IsMinimized() &&
      !gained_active->IsVisible()) {
    window_state->Unminimize();
    DCHECK(!window_state->IsMinimized());
  }
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, ash::ShellObserver overrides:

void BaseLayoutManager::OnDisplayWorkAreaInsetsChanged() {
  AdjustAllWindowsBoundsForWorkAreaChange(
      ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED);
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, ash::wm::WindowStateObserver overrides:

void BaseLayoutManager::OnWindowShowTypeChanged(wm::WindowState* window_state,
                                                wm::WindowShowType old_type) {
  if (old_type != wm::SHOW_TYPE_MINIMIZED &&
      !window_state->HasRestoreBounds() &&
      window_state->IsMaximizedOrFullscreen() &&
      !wm::IsMaximizedOrFullscreenWindowShowType(old_type)) {
    window_state->SetRestoreBoundsInParent(window_state->window()->bounds());
  }

  UpdateBoundsFromShowType(window_state);
  ShowTypeChanged(window_state, old_type);
}

//////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, protected:

void BaseLayoutManager::ShowTypeChanged(
    wm::WindowState* window_state,
    wm::WindowShowType last_show_type) {
  if (window_state->IsMinimized()) {
    if (last_show_type == wm::SHOW_TYPE_MINIMIZED)
      return;

    // Save the previous show state so that we can correctly restore it.
    window_state->window()->SetProperty(aura::client::kRestoreShowStateKey,
                                        wm::ToWindowShowState(last_show_type));
    views::corewm::SetWindowVisibilityAnimationType(
        window_state->window(), WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);

    // Hide the window.
    window_state->window()->Hide();
    // Activate another window.
    if (window_state->IsActive())
      window_state->Deactivate();
  } else if ((window_state->window()->TargetVisibility() ||
              last_show_type == wm::SHOW_TYPE_MINIMIZED) &&
             !window_state->window()->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
    if (last_show_type == wm::SHOW_TYPE_MINIMIZED)
      window_state->set_unminimize_to_restore_bounds(false);
  }
}

void BaseLayoutManager::AdjustAllWindowsBoundsForWorkAreaChange(
    AdjustWindowReason reason) {
  // Don't do any adjustments of the insets while we are in screen locked mode.
  // This would happen if the launcher was auto hidden before the login screen
  // was shown and then gets shown when the login screen gets presented.
  if (reason == ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED &&
      Shell::GetInstance()->session_state_delegate()->IsScreenLocked())
    return;

  // If a user plugs an external display into a laptop running Aura the
  // display size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  for (WindowSet::const_iterator it = windows_.begin();
       it != windows_.end();
       ++it) {
    AdjustWindowBoundsForWorkAreaChange(wm::GetWindowState(*it), reason);
  }
}

void BaseLayoutManager::AdjustWindowBoundsForWorkAreaChange(
    wm::WindowState* window_state,
    AdjustWindowReason reason) {
  aura::Window* window = window_state->window();
  if (window_state->IsMaximized()) {
    SetChildBoundsDirect(
        window, ScreenUtil::GetMaximizedWindowBoundsInParent(window));
  } else if (window_state->IsFullscreen()) {
    SetChildBoundsDirect(
        window, ScreenUtil::GetDisplayBoundsInParent(window));
  } else {
    // The work area may be smaller than the full screen.
    gfx::Rect display_rect =
        ScreenUtil::GetDisplayWorkAreaBoundsInParent(window);
    // Put as much of the window as possible within the display area.
    gfx::Rect bounds = window->bounds();
    bounds.AdjustToFit(display_rect);
    window->SetBounds(bounds);
  }
}

//////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, private:

void BaseLayoutManager::UpdateBoundsFromShowType(
    wm::WindowState* window_state) {
  aura::Window* window = window_state->window();
  switch (window_state->window_show_type()) {
    case wm::SHOW_TYPE_DEFAULT:
    case wm::SHOW_TYPE_NORMAL:
      if (window_state->HasRestoreBounds()) {
        gfx::Rect bounds_in_parent = window_state->GetRestoreBoundsInParent();
        SetChildBoundsDirect(window,
                             BoundsWithScreenEdgeVisible(window,
                                                         bounds_in_parent));
      }
      window_state->ClearRestoreBounds();
      break;

    case wm::SHOW_TYPE_LEFT_SNAPPED:
    case wm::SHOW_TYPE_RIGHT_SNAPPED:
      if (window_state->HasRestoreBounds())
        SetChildBoundsDirect(window, window_state->GetRestoreBoundsInParent());
      window_state->ClearRestoreBounds();
      break;

    case wm::SHOW_TYPE_MAXIMIZED:
      SetChildBoundsDirect(
          window, ScreenUtil::GetMaximizedWindowBoundsInParent(window));
      break;

    case wm::SHOW_TYPE_FULLSCREEN:
      // Don't animate the full-screen window transition.
      // TODO(jamescook): Use animation here.  Be sure the lock screen works.
      SetChildBoundsDirect(window,
                           ScreenUtil::GetDisplayBoundsInParent(window));
      break;

    case wm::SHOW_TYPE_MINIMIZED:
    case wm::SHOW_TYPE_INACTIVE:
    case wm::SHOW_TYPE_DETACHED:
    case wm::SHOW_TYPE_END:
    case wm::SHOW_TYPE_AUTO_POSITIONED:
      break;
  }
}

}  // namespace internal
}  // namespace ash
