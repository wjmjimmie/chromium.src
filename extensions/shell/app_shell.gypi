# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'app_shell_lib_sources': [
      'app/paths_mac.h',
      'app/paths_mac.mm',
      'app/shell_main_delegate.cc',
      'app/shell_main_delegate.h',
      'browser/api/identity/identity_api.cc',
      'browser/api/identity/identity_api.h',
      'browser/shell_browser_context_keyed_service_factories.cc',
      'browser/shell_browser_context_keyed_service_factories.h',
      'browser/default_shell_browser_main_delegate.cc',
      'browser/default_shell_browser_main_delegate.h',
      'browser/desktop_controller.cc',
      'browser/desktop_controller.h',
      'browser/media_capture_util.cc',
      'browser/media_capture_util.h',
      'browser/shell_app_delegate.cc',
      'browser/shell_app_delegate.h',
      'browser/shell_app_view_guest_delegate.cc',
      'browser/shell_app_view_guest_delegate.h',
      'browser/shell_app_window_client.cc',
      'browser/shell_app_window_client.h',
      'browser/shell_app_window_client_mac.mm',
      'browser/shell_audio_controller_chromeos.cc',
      'browser/shell_audio_controller_chromeos.h',
      'browser/shell_browser_context.cc',
      'browser/shell_browser_context.h',
      'browser/shell_browser_main_delegate.h',
      'browser/shell_browser_main_parts.cc',
      'browser/shell_browser_main_parts.h',
      'browser/shell_browser_main_parts_mac.h',
      'browser/shell_browser_main_parts_mac.mm',
      'browser/shell_content_browser_client.cc',
      'browser/shell_content_browser_client.h',
      'browser/shell_desktop_controller_mac.h',
      'browser/shell_desktop_controller_mac.mm',
      'browser/shell_device_client.cc',
      'browser/shell_device_client.h',
      'browser/shell_display_info_provider.cc',
      'browser/shell_display_info_provider.h',
      'browser/shell_extension_host_delegate.cc',
      'browser/shell_extension_host_delegate.h',
      'browser/shell_extension_system.cc',
      'browser/shell_extension_system.h',
      'browser/shell_extension_system_factory.cc',
      'browser/shell_extension_system_factory.h',
      'browser/shell_extension_web_contents_observer.cc',
      'browser/shell_extension_web_contents_observer.h',
      'browser/shell_extensions_api_client.cc',
      'browser/shell_extensions_api_client.h',
      'browser/shell_extensions_browser_client.cc',
      'browser/shell_extensions_browser_client.h',
      'browser/shell_native_app_window.cc',
      'browser/shell_native_app_window.h',
      'browser/shell_native_app_window_mac.h',
      'browser/shell_native_app_window_mac.mm',
      'browser/shell_network_controller_chromeos.cc',
      'browser/shell_network_controller_chromeos.h',
      'browser/shell_network_delegate.cc',
      'browser/shell_network_delegate.h',
      'browser/shell_oauth2_token_service.cc',
      'browser/shell_oauth2_token_service.h',
      'browser/shell_prefs.cc',
      'browser/shell_prefs.h',
      'browser/shell_runtime_api_delegate.cc',
      'browser/shell_runtime_api_delegate.h',
      'browser/shell_special_storage_policy.cc',
      'browser/shell_special_storage_policy.h',
      'browser/shell_speech_recognition_manager_delegate.cc',
      'browser/shell_speech_recognition_manager_delegate.h',
      'browser/shell_update_query_params_delegate.cc',
      'browser/shell_update_query_params_delegate.h',
      'browser/shell_url_request_context_getter.cc',
      'browser/shell_url_request_context_getter.h',
      'browser/shell_web_contents_modal_dialog_manager.cc',
      'common/shell_content_client.cc',
      'common/shell_content_client.h',
      'common/shell_extensions_client.cc',
      'common/shell_extensions_client.h',
      'common/switches.cc',
      'common/switches.h',
      'renderer/shell_content_renderer_client.cc',
      'renderer/shell_content_renderer_client.h',
      'renderer/shell_extensions_renderer_client.cc',
      'renderer/shell_extensions_renderer_client.h',
      'utility/shell_content_utility_client.cc',
      'utility/shell_content_utility_client.h',
    ],
    'app_shell_lib_sources_aura': [
      'browser/shell_app_window_client_aura.cc',
      'browser/shell_desktop_controller_aura.cc',
      'browser/shell_desktop_controller_aura.h',
      'browser/shell_native_app_window_aura.cc',
      'browser/shell_native_app_window_aura.h',
      'browser/shell_screen.cc',
      'browser/shell_screen.h',
    ],
    'app_shell_lib_sources_chromeos': [
      'browser/api/shell_gcd/shell_gcd_api.cc',
      'browser/api/shell_gcd/shell_gcd_api.h',
      'browser/api/vpn_provider/vpn_service_factory.cc',
    ],
    'app_shell_lib_sources_nacl': [
      'browser/shell_nacl_browser_delegate.cc',
      'browser/shell_nacl_browser_delegate.h',
    ],
    'app_shell_sources': [
      'app/shell_main.cc',
    ],
    'app_shell_sources_mac': [
      'app/shell_main_mac.h',
      'app/shell_main_mac.cc',
    ],
    'app_shell_unittests_sources': [
      '../test/extensions_unittests_main.cc',
      'browser/api/identity/identity_api_unittest.cc',
      'browser/shell_oauth2_token_service_unittest.cc',
      'browser/shell_prefs_unittest.cc',
      'common/shell_content_client_unittest.cc',
    ],
    'app_shell_unittests_sources_aura': [
      'browser/shell_desktop_controller_aura_unittest.cc',
      'browser/shell_native_app_window_aura_unittest.cc',
      'browser/shell_screen_unittest.cc',
    ],
    'app_shell_unittests_sources_chromeos': [
      'browser/api/shell_gcd/shell_gcd_api_unittest.cc',
      'browser/shell_audio_controller_chromeos_unittest.cc',
    ],
    'app_shell_unittests_sources_nacl': [
      'browser/shell_nacl_browser_delegate_unittest.cc',
    ],
  },
}
