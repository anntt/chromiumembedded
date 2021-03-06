// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_file_system.h"
#include "libcef/browser_file_writer.h"
#include "libcef/cef_thread.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemEntry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_file_writer.h"

using base::WeakPtr;

using WebKit::WebFileInfo;
using WebKit::WebFileSystem;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebFileWriter;
using WebKit::WebFileWriterClient;
using WebKit::WebFrame;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

using fileapi::FileSystemContext;
using fileapi::FileSystemOperationInterface;

BrowserFileSystem::BrowserFileSystem() {
}

BrowserFileSystem::~BrowserFileSystem() {
}

void BrowserFileSystem::CreateContext() {
  if (file_system_context_.get())
    return;

  if (file_system_dir_.CreateUniqueTempDir()) {
    std::vector<std::string> additional_allowed_schemes;
    additional_allowed_schemes.push_back("file");

    file_system_context_ = new FileSystemContext(
        CefThread::GetMessageLoopProxyForThread(CefThread::FILE),
        CefThread::GetMessageLoopProxyForThread(CefThread::IO),
        NULL /* special storage policy */,
        NULL /* quota manager */,
        file_system_dir_.path(),
        fileapi::FileSystemOptions(
            fileapi::FileSystemOptions::PROFILE_MODE_NORMAL,
            additional_allowed_schemes));
  } else {
    LOG(WARNING) << "Failed to create a temp dir for the filesystem."
                    "FileSystem feature will be disabled.";
  }
}

void BrowserFileSystem::OpenFileSystem(
    WebFrame* frame, WebFileSystem::Type web_filesystem_type,
    long long, bool create,  // NOLINT(runtime/int)
    WebFileSystemCallbacks* callbacks) {
  if (!frame || !file_system_context_.get()) {
    // The FileSystem temp directory was not initialized successfully.
    callbacks->didFail(WebKit::WebFileErrorSecurity);
    return;
  }

  fileapi::FileSystemType type;
  if (web_filesystem_type == WebFileSystem::TypeTemporary)
    type = fileapi::kFileSystemTypeTemporary;
  else if (web_filesystem_type == WebFileSystem::TypePersistent)
    type = fileapi::kFileSystemTypePersistent;
  else if (web_filesystem_type == WebFileSystem::TypeExternal)
    type = fileapi::kFileSystemTypeExternal;
  else {
    // Unknown type filesystem is requested.
    callbacks->didFail(WebKit::WebFileErrorSecurity);
    return;
  }

  GURL origin_url(frame->document().securityOrigin().toString());
  file_system_context_->OpenFileSystem(
      origin_url, type, create, OpenFileSystemHandler(callbacks));
}

void BrowserFileSystem::move(
    const WebURL& src_path,
    const WebURL& dest_path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(src_path)->Move(GURL(src_path), GURL(dest_path),
                                  FinishHandler(callbacks));
}

void BrowserFileSystem::copy(
    const WebURL& src_path, const WebURL& dest_path,
    WebFileSystemCallbacks* callbacks) {
  GetNewOperation(src_path)->Copy(GURL(src_path), GURL(dest_path),
                                  FinishHandler(callbacks));
}

void BrowserFileSystem::remove(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(path)->Remove(path, false /* recursive */,
                                FinishHandler(callbacks));
}

void BrowserFileSystem::removeRecursively(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(path)->Remove(path, true /* recursive */,
                                FinishHandler(callbacks));
}

void BrowserFileSystem::readMetadata(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(path)->GetMetadata(path, GetMetadataHandler(callbacks));
}

void BrowserFileSystem::createFile(
    const WebURL& path, bool exclusive, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(path)->CreateFile(path, exclusive, FinishHandler(callbacks));
}

void BrowserFileSystem::createDirectory(
    const WebURL& path, bool exclusive, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(path)->CreateDirectory(path, exclusive, false,
                                         FinishHandler(callbacks));
}

void BrowserFileSystem::fileExists(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(path)->FileExists(path, FinishHandler(callbacks));
}

void BrowserFileSystem::directoryExists(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(path)->DirectoryExists(path, FinishHandler(callbacks));
}

void BrowserFileSystem::readDirectory(
    const WebURL& path, WebFileSystemCallbacks* callbacks) {
  GetNewOperation(path)->ReadDirectory(path, ReadDirectoryHandler(callbacks));
}

WebFileWriter* BrowserFileSystem::createFileWriter(
    const WebURL& path, WebFileWriterClient* client) {
  return new BrowserFileWriter(path, client, file_system_context_.get());
}

FileSystemOperationInterface* BrowserFileSystem::GetNewOperation(
    const WebURL& url) {
  return file_system_context_->CreateFileSystemOperation(
      GURL(url),
      base::MessageLoopProxy::current());
}

FileSystemOperationInterface::StatusCallback
BrowserFileSystem::FinishHandler(WebFileSystemCallbacks* callbacks) {
  return base::Bind(&BrowserFileSystem::DidFinish,
                    AsWeakPtr(), base::Unretained(callbacks));
}

FileSystemOperationInterface::ReadDirectoryCallback
BrowserFileSystem::ReadDirectoryHandler(WebFileSystemCallbacks* callbacks) {
  return base::Bind(&BrowserFileSystem::DidReadDirectory,
                    AsWeakPtr(), base::Unretained(callbacks));
}

FileSystemOperationInterface::GetMetadataCallback
BrowserFileSystem::GetMetadataHandler(WebFileSystemCallbacks* callbacks) {
  return base::Bind(&BrowserFileSystem::DidGetMetadata,
                    AsWeakPtr(), base::Unretained(callbacks));
}

FileSystemContext::OpenFileSystemCallback
BrowserFileSystem::OpenFileSystemHandler(WebFileSystemCallbacks* callbacks) {
  return base::Bind(&BrowserFileSystem::DidOpenFileSystem,
                    AsWeakPtr(), base::Unretained(callbacks));
}

void BrowserFileSystem::DidFinish(WebFileSystemCallbacks* callbacks,
                                 base::PlatformFileError result) {
  if (result == base::PLATFORM_FILE_OK)
    callbacks->didSucceed();
  else
    callbacks->didFail(webkit_glue::PlatformFileErrorToWebFileError(result));
}

void BrowserFileSystem::DidGetMetadata(WebFileSystemCallbacks* callbacks,
                                      base::PlatformFileError result,
                                      const base::PlatformFileInfo& info,
                                      const FilePath& platform_path) {
  if (result == base::PLATFORM_FILE_OK) {
    WebFileInfo web_file_info;
    web_file_info.length = info.size;
    web_file_info.modificationTime = info.last_modified.ToDoubleT();
    web_file_info.type = info.is_directory ?
        WebFileInfo::TypeDirectory : WebFileInfo::TypeFile;
    web_file_info.platformPath =
        webkit_glue::FilePathToWebString(platform_path);
    callbacks->didReadMetadata(web_file_info);
  } else {
    callbacks->didFail(webkit_glue::PlatformFileErrorToWebFileError(result));
  }
}

void BrowserFileSystem::DidReadDirectory(
    WebFileSystemCallbacks* callbacks,
    base::PlatformFileError result,
    const std::vector<base::FileUtilProxy::Entry>& entries,
    bool has_more) {
  if (result == base::PLATFORM_FILE_OK) {
    std::vector<WebFileSystemEntry> web_entries_vector;
    for (std::vector<base::FileUtilProxy::Entry>::const_iterator it =
            entries.begin(); it != entries.end(); ++it) {
      WebFileSystemEntry entry;
      entry.name = webkit_glue::FilePathStringToWebString(it->name);
      entry.isDirectory = it->is_directory;
      web_entries_vector.push_back(entry);
    }
    WebVector<WebKit::WebFileSystemEntry> web_entries = web_entries_vector;
    callbacks->didReadDirectory(web_entries, has_more);
  } else {
    callbacks->didFail(webkit_glue::PlatformFileErrorToWebFileError(result));
  }
}

void BrowserFileSystem::DidOpenFileSystem(
    WebFileSystemCallbacks* callbacks,
    base::PlatformFileError result,
    const std::string& name, const GURL& root) {
  if (result == base::PLATFORM_FILE_OK) {
    if (!root.is_valid())
      callbacks->didFail(WebKit::WebFileErrorSecurity);
    else
      callbacks->didOpenFileSystem(WebString::fromUTF8(name), root);
  } else {
    callbacks->didFail(webkit_glue::PlatformFileErrorToWebFileError(result));
  }
}
