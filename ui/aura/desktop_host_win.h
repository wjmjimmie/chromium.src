// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_HOST_WIN_H_
#define UI_AURA_DESKTOP_HOST_WIN_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/desktop_host.h"
#include "ui/base/win/window_impl.h"

namespace ui {
class InputMethod;
}  // namespace ui

namespace aura {

class DesktopHostWin : public DesktopHost,
                       public ui::WindowImpl {
 public:
  explicit DesktopHostWin(const gfx::Rect& bounds);
  virtual ~DesktopHostWin();

  // MessageLoop::Dispatcher:
  virtual bool Dispatch(const MSG& msg);

  // DesktopHost:
  virtual void SetDesktop(Desktop* desktop) OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void ToggleFullScreen() OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual gfx::Point QueryMouseLocation() OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;

  virtual void SetInputMethod(ui::InputMethod* input_method) OVERRIDE;
  virtual ui::InputMethod* GetInputMethod() const OVERRIDE;

  // ui::internal::InputMethodDelegate Override.
  virtual void DispatchKeyEventPostIME(const base::NativeEvent& event) OVERRIDE;
  virtual void DispatchFabricatedKeyEventPostIME(
      ui::EventType type, ui::KeyboardCode key_code, int flags) OVERRIDE;

 private:
  BEGIN_MSG_MAP_EX(DesktopHostWin)
    // Range handlers must go first!
    MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE, WM_NCXBUTTONDBLCLK, OnMouseRange)

    // Key events.
    MESSAGE_HANDLER_EX(WM_KEYDOWN, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_KEYUP, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_SYSKEYUP, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_CHAR, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_SYSCHAR, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_IME_CHAR, OnKeyEvent)

    MSG_WM_CLOSE(OnClose)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_SIZE(OnSize)
  END_MSG_MAP()

  void OnClose();
  LRESULT OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  void OnPaint(HDC dc);
  void OnSize(UINT param, const CSize& size);

  Desktop* desktop_;

  scoped_ptr<ui::InputMethod> input_method_;

  bool fullscreen_;
  RECT saved_window_rect_;
  DWORD saved_window_style_;
  DWORD saved_window_ex_style_;

  DISALLOW_COPY_AND_ASSIGN(DesktopHostWin);
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_HOST_WIN_H_
