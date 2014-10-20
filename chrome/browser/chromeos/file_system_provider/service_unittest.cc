// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/service.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kDisplayName[] = "Camera Pictures";

// The dot in the file system ID is there in order to check that saving to
// preferences works correctly. File System ID is used as a key in
// a base::DictionaryValue, so it has to be stored without path expansion.
const char kFileSystemId[] = "camera/pictures/id .!@#$%^&*()_+";

// Utility observer, logging events from file_system_provider::Service.
class LoggingObserver : public Observer {
 public:
  class Event {
   public:
    Event(const ProvidedFileSystemInfo& file_system_info,
          base::File::Error error)
        : file_system_info_(file_system_info), error_(error) {}
    ~Event() {}

    const ProvidedFileSystemInfo& file_system_info() {
      return file_system_info_;
    }
    base::File::Error error() { return error_; }

   private:
    ProvidedFileSystemInfo file_system_info_;
    base::File::Error error_;
  };

  LoggingObserver() {}
  virtual ~LoggingObserver() {}

  // file_system_provider::Observer overrides.
  virtual void OnProvidedFileSystemMount(
      const ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override {
    mounts.push_back(Event(file_system_info, error));
  }

  virtual void OnProvidedFileSystemUnmount(
      const ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override {
    unmounts.push_back(Event(file_system_info, error));
  }

  std::vector<Event> mounts;
  std::vector<Event> unmounts;
};

// Creates a fake extension with the specified |extension_id|.
scoped_refptr<extensions::Extension> CreateFakeExtension(
    const std::string& extension_id) {
  base::DictionaryValue manifest;
  std::string error;
  manifest.SetStringWithoutPathExpansion(extensions::manifest_keys::kVersion,
                                         "1.0.0.0");
  manifest.SetStringWithoutPathExpansion(extensions::manifest_keys::kName,
                                         "unused");
  return extensions::Extension::Create(base::FilePath(),
                                       extensions::Manifest::UNPACKED,
                                       manifest,
                                       extensions::Extension::NO_FLAGS,
                                       extension_id,
                                       &error);
}

// Stores a provided file system information in preferences together with a
// fake observed entry.
void RememberFakeFileSystem(TestingProfile* profile,
                            const std::string& extension_id,
                            const std::string& file_system_id,
                            const std::string& display_name,
                            bool writable,
                            bool supports_notify_tag,
                            const ObservedEntry& observed_entry) {
  TestingPrefServiceSyncable* const pref_service =
      profile->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  base::DictionaryValue extensions;
  base::DictionaryValue* file_systems = new base::DictionaryValue();
  base::DictionaryValue* file_system = new base::DictionaryValue();
  file_system->SetStringWithoutPathExpansion(kPrefKeyFileSystemId,
                                             kFileSystemId);
  file_system->SetStringWithoutPathExpansion(kPrefKeyDisplayName, kDisplayName);
  file_system->SetBooleanWithoutPathExpansion(kPrefKeyWritable, writable);
  file_system->SetBooleanWithoutPathExpansion(kPrefKeySupportsNotifyTag,
                                              supports_notify_tag);
  file_systems->SetWithoutPathExpansion(kFileSystemId, file_system);
  extensions.SetWithoutPathExpansion(kExtensionId, file_systems);

  // Remember observed entries.
  base::DictionaryValue* const observed_entries = new base::DictionaryValue();
  file_system->SetWithoutPathExpansion(kPrefKeyObservedEntries,
                                       observed_entries);
  base::DictionaryValue* const observed_entry_value =
      new base::DictionaryValue();
  observed_entries->SetWithoutPathExpansion(observed_entry.entry_path.value(),
                                            observed_entry_value);
  observed_entry_value->SetStringWithoutPathExpansion(
      kPrefKeyObservedEntryEntryPath, observed_entry.entry_path.value());
  observed_entry_value->SetBooleanWithoutPathExpansion(
      kPrefKeyObservedEntryRecursive, observed_entry.recursive);
  observed_entry_value->SetStringWithoutPathExpansion(
      kPrefKeyObservedEntryLastTag, observed_entry.last_tag);

  pref_service->Set(prefs::kFileSystemProviderMounted, extensions);
}

}  // namespace

class FileSystemProviderServiceTest : public testing::Test {
 protected:
  FileSystemProviderServiceTest() : profile_(NULL) {}

  virtual ~FileSystemProviderServiceTest() {}

  virtual void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    user_manager_ = new FakeUserManager();
    user_manager_->AddUser(profile_->GetProfileName());
    user_manager_enabler_.reset(new ScopedUserManagerEnabler(user_manager_));
    extension_registry_.reset(new extensions::ExtensionRegistry(profile_));
    service_.reset(new Service(profile_, extension_registry_.get()));
    service_->SetFileSystemFactoryForTesting(
        base::Bind(&FakeProvidedFileSystem::Create));
    extension_ = CreateFakeExtension(kExtensionId);
    fake_observed_entry_.entry_path =
        base::FilePath(FILE_PATH_LITERAL("/a/b/c"));
    fake_observed_entry_.recursive = true;
    fake_observed_entry_.last_tag = "hello-world";
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  FakeUserManager* user_manager_;
  scoped_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  scoped_ptr<extensions::ExtensionRegistry> extension_registry_;
  scoped_ptr<Service> service_;
  scoped_refptr<extensions::Extension> extension_;
  ObservedEntry fake_observed_entry_;
};

TEST_F(FileSystemProviderServiceTest, MountFileSystem) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(kExtensionId, observer.mounts[0].file_system_info().extension_id());
  EXPECT_EQ(kFileSystemId,
            observer.mounts[0].file_system_info().file_system_id());
  base::FilePath expected_mount_path =
      util::GetMountPath(profile_, kExtensionId, kFileSystemId);
  EXPECT_EQ(expected_mount_path.AsUTF8Unsafe(),
            observer.mounts[0].file_system_info().mount_path().AsUTF8Unsafe());
  EXPECT_EQ(kDisplayName, observer.mounts[0].file_system_info().display_name());
  EXPECT_FALSE(observer.mounts[0].file_system_info().writable());
  EXPECT_FALSE(observer.mounts[0].file_system_info().supports_notify_tag());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());
  ASSERT_EQ(0u, observer.unmounts.size());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest,
       MountFileSystem_WritableAndSupportsNotifyTag) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;
  EXPECT_TRUE(service_->MountFileSystem(kExtensionId, options));

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_TRUE(observer.mounts[0].file_system_info().writable());
  EXPECT_TRUE(observer.mounts[0].file_system_info().supports_notify_tag());
  ASSERT_EQ(0u, observer.unmounts.size());
  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, MountFileSystem_UniqueIds) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  EXPECT_FALSE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

  ASSERT_EQ(2u, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, observer.mounts[1].error());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, MountFileSystem_StressTest) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  const size_t kMaxFileSystems = 16;
  for (size_t i = 0; i < kMaxFileSystems; ++i) {
    const std::string file_system_id =
        std::string("test-") + base::IntToString(i);
    EXPECT_TRUE(service_->MountFileSystem(
        kExtensionId, MountOptions(file_system_id, kDisplayName)));
  }
  ASSERT_EQ(kMaxFileSystems, observer.mounts.size());

  // The next file system is out of limit, and registering it should fail.
  EXPECT_FALSE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

  ASSERT_EQ(kMaxFileSystems + 1, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED,
            observer.mounts[kMaxFileSystems].error());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(kMaxFileSystems, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  EXPECT_TRUE(service_->UnmountFileSystem(
      kExtensionId, kFileSystemId, Service::UNMOUNT_REASON_USER));
  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  EXPECT_EQ(kExtensionId,
            observer.unmounts[0].file_system_info().extension_id());
  EXPECT_EQ(kFileSystemId,
            observer.unmounts[0].file_system_info().file_system_id());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(0u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem_OnExtensionUnload) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  // Directly call the observer's method.
  service_->OnExtensionUnloaded(
      profile_,
      extension_.get(),
      extensions::UnloadedExtensionInfo::REASON_DISABLE);

  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  EXPECT_EQ(kExtensionId,
            observer.unmounts[0].file_system_info().extension_id());
  EXPECT_EQ(kFileSystemId,
            observer.unmounts[0].file_system_info().file_system_id());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(0u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem_WrongExtensionId) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  const std::string kWrongExtensionId = "helloworldhelloworldhelloworldhe";

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());
  ASSERT_EQ(1u, service_->GetProvidedFileSystemInfoList().size());

  EXPECT_FALSE(service_->UnmountFileSystem(
      kWrongExtensionId, kFileSystemId, Service::UNMOUNT_REASON_USER));
  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, observer.unmounts[0].error());
  ASSERT_EQ(1u, service_->GetProvidedFileSystemInfoList().size());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RestoreFileSystem_OnExtensionLoad) {
  // Create a fake entry in the preferences.
  RememberFakeFileSystem(profile_,
                         kExtensionId,
                         kFileSystemId,
                         kDisplayName,
                         true /* writable */,
                         true /* supports_notify_tag */,
                         fake_observed_entry_);

  // Create a new service instance in order to load remembered file systems
  // from preferences.
  scoped_ptr<Service> new_service(
      new Service(profile_, extension_registry_.get()));
  LoggingObserver observer;
  new_service->AddObserver(&observer);

  new_service->SetFileSystemFactoryForTesting(
      base::Bind(&FakeProvidedFileSystem::Create));

  EXPECT_EQ(0u, observer.mounts.size());

  // Directly call the observer's method.
  new_service->OnExtensionLoaded(profile_, extension_.get());

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());

  EXPECT_EQ(kExtensionId, observer.mounts[0].file_system_info().extension_id());
  EXPECT_EQ(kFileSystemId,
            observer.mounts[0].file_system_info().file_system_id());
  EXPECT_TRUE(observer.mounts[0].file_system_info().writable());
  EXPECT_TRUE(observer.mounts[0].file_system_info().supports_notify_tag());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      new_service->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  ProvidedFileSystemInterface* const file_system =
      new_service->GetProvidedFileSystem(kExtensionId, kFileSystemId);
  ASSERT_TRUE(file_system);

  const ObservedEntries* const observed_entries =
      file_system->GetObservedEntries();
  ASSERT_TRUE(observed_entries);
  ASSERT_EQ(1u, observed_entries->size());

  const ObservedEntries::const_iterator restored_observed_entry_it =
      observed_entries->find(fake_observed_entry_.entry_path);
  ASSERT_NE(observed_entries->end(), restored_observed_entry_it);

  EXPECT_EQ(fake_observed_entry_.entry_path,
            restored_observed_entry_it->second.entry_path);
  EXPECT_EQ(fake_observed_entry_.recursive,
            restored_observed_entry_it->second.recursive);
  EXPECT_EQ(fake_observed_entry_.last_tag,
            restored_observed_entry_it->second.last_tag);

  new_service->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem) {
  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;

  ProvidedFileSystemInfo file_system_info(
      kExtensionId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")));

  ObservedEntries observed_entries;
  observed_entries[fake_observed_entry_.entry_path] = fake_observed_entry_;

  service_->RememberFileSystem(file_system_info, observed_entries);

  TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::DictionaryValue* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::DictionaryValue* file_systems = NULL;
  ASSERT_TRUE(extensions->GetDictionaryWithoutPathExpansion(kExtensionId,
                                                            &file_systems));
  EXPECT_EQ(1u, file_systems->size());

  const base::Value* file_system_value = NULL;
  const base::DictionaryValue* file_system = NULL;
  ASSERT_TRUE(
      file_systems->GetWithoutPathExpansion(kFileSystemId, &file_system_value));
  ASSERT_TRUE(file_system_value->GetAsDictionary(&file_system));

  std::string file_system_id;
  EXPECT_TRUE(file_system->GetStringWithoutPathExpansion(kPrefKeyFileSystemId,
                                                         &file_system_id));
  EXPECT_EQ(kFileSystemId, file_system_id);

  std::string display_name;
  EXPECT_TRUE(file_system->GetStringWithoutPathExpansion(kPrefKeyDisplayName,
                                                         &display_name));
  EXPECT_EQ(kDisplayName, display_name);

  bool writable = false;
  EXPECT_TRUE(
      file_system->GetBooleanWithoutPathExpansion(kPrefKeyWritable, &writable));
  EXPECT_TRUE(writable);

  bool supports_notify_tag = false;
  EXPECT_TRUE(file_system->GetBooleanWithoutPathExpansion(
      kPrefKeySupportsNotifyTag, &supports_notify_tag));
  EXPECT_TRUE(supports_notify_tag);

  const base::DictionaryValue* observed_entries_value = NULL;
  ASSERT_TRUE(file_system->GetDictionaryWithoutPathExpansion(
      kPrefKeyObservedEntries, &observed_entries_value));

  const base::DictionaryValue* observed_entry = NULL;
  ASSERT_TRUE(observed_entries_value->GetDictionaryWithoutPathExpansion(
      fake_observed_entry_.entry_path.value(), &observed_entry));

  std::string entry_path;
  EXPECT_TRUE(observed_entry->GetStringWithoutPathExpansion(
      kPrefKeyObservedEntryEntryPath, &entry_path));
  EXPECT_EQ(fake_observed_entry_.entry_path.value(), entry_path);

  bool recursive = false;
  EXPECT_TRUE(observed_entry->GetBooleanWithoutPathExpansion(
      kPrefKeyObservedEntryRecursive, &recursive));
  EXPECT_EQ(fake_observed_entry_.recursive, recursive);

  std::string last_tag;
  EXPECT_TRUE(observed_entry->GetStringWithoutPathExpansion(
      kPrefKeyObservedEntryLastTag, &last_tag));
  EXPECT_EQ(fake_observed_entry_.last_tag, last_tag);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnMount) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  const base::DictionaryValue* extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::DictionaryValue* file_systems = NULL;
  ASSERT_TRUE(extensions->GetDictionaryWithoutPathExpansion(kExtensionId,
                                                            &file_systems));
  EXPECT_EQ(1u, file_systems->size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnUnmountOnShutdown) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  {
    EXPECT_TRUE(service_->MountFileSystem(
        kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
    ASSERT_EQ(1u, observer.mounts.size());

    const base::DictionaryValue* extensions =
        pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
    ASSERT_TRUE(extensions);

    const base::DictionaryValue* file_systems = NULL;
    ASSERT_TRUE(extensions->GetDictionaryWithoutPathExpansion(kExtensionId,
                                                              &file_systems));
    EXPECT_EQ(1u, file_systems->size());
  }

  {
    EXPECT_TRUE(service_->UnmountFileSystem(
        kExtensionId, kFileSystemId, Service::UNMOUNT_REASON_SHUTDOWN));

    const base::DictionaryValue* const extensions =
        pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
    ASSERT_TRUE(extensions);

    const base::DictionaryValue* file_systems = NULL;
    ASSERT_TRUE(extensions->GetDictionaryWithoutPathExpansion(kExtensionId,
                                                              &file_systems));
    EXPECT_EQ(1u, file_systems->size());
  }

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnUnmountByUser) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  {
    EXPECT_TRUE(service_->MountFileSystem(
        kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
    ASSERT_EQ(1u, observer.mounts.size());

    const base::DictionaryValue* extensions =
        pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
    ASSERT_TRUE(extensions);

    const base::DictionaryValue* file_systems = NULL;
    ASSERT_TRUE(extensions->GetDictionaryWithoutPathExpansion(kExtensionId,
                                                              &file_systems));
    EXPECT_EQ(1u, file_systems->size());
  }

  {
    EXPECT_TRUE(service_->UnmountFileSystem(
        kExtensionId, kFileSystemId, Service::UNMOUNT_REASON_USER));

    const base::DictionaryValue* const extensions =
        pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
    ASSERT_TRUE(extensions);

    const base::DictionaryValue* file_systems = NULL;
    EXPECT_FALSE(extensions->GetDictionaryWithoutPathExpansion(kExtensionId,
                                                               &file_systems));
  }

  service_->RemoveObserver(&observer);
}

}  // namespace file_system_provider
}  // namespace chromeos
