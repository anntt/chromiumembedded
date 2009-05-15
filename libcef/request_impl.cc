// Copyright (c) 2008 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "precompiled_libcef.h"
#include "request_impl.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/glue/glue_util.h"


CefRefPtr<CefRequest> CefRequest::CreateRequest()
{
  CefRefPtr<CefRequest> request(new CefRequestImpl());
  return request;
}

CefRequestImpl::CefRequestImpl()
{
}

CefRequestImpl::~CefRequestImpl()
{
}

std::wstring CefRequestImpl::GetURL()
{
  Lock();
  std::wstring url = url_;
  Unlock();
  return url;
}

void CefRequestImpl::SetURL(const std::wstring& url)
{
  Lock();
  url_ = url;
  Unlock();
}

std::wstring CefRequestImpl::GetFrame()
{
  Lock();
  std::wstring frame = frame_;
  Unlock();
  return frame;
}

void CefRequestImpl::SetFrame(const std::wstring& frame)
{
  Lock();
  frame_ = frame;
  Unlock();
}

std::wstring CefRequestImpl::GetMethod()
{
  Lock();
  std::wstring method = method_;
  Unlock();
  return method;
}

void CefRequestImpl::SetMethod(const std::wstring& method)
{
  Lock();
  method_ = method;
  Unlock();
}

CefRefPtr<CefPostData> CefRequestImpl::GetPostData()
{
  Lock();
  CefRefPtr<CefPostData> postData = postdata_;
  Unlock();
  return postData;
}

void CefRequestImpl::SetPostData(CefRefPtr<CefPostData> postData)
{
  Lock();
  postdata_ = postData;
  Unlock();
}

void CefRequestImpl::GetHeaderMap(HeaderMap& headerMap)
{
  Lock();
  headerMap = headermap_;
  Unlock();
}

void CefRequestImpl::SetHeaderMap(const HeaderMap& headerMap)
{
  Lock();
  headermap_ = headerMap;
  Unlock();
}

void CefRequestImpl::Set(const std::wstring& url,
                 const std::wstring& frame,
                 const std::wstring& method,
                 CefRefPtr<CefPostData> postData,
                 const HeaderMap& headerMap)
{
  Lock();
  url_ = url;
  frame_ = frame;
  method_ = method;
  postdata_ = postData;
  headermap_ = headerMap;
  Unlock();
}

void CefRequestImpl::SetHeaderMap(const WebRequest::HeaderMap& map)
{
  Lock();
  WebRequest::HeaderMap::const_iterator it = map.begin();
  for(; it != map.end(); ++it) {
    headermap_.insert(
        std::make_pair(
            UTF8ToWide(it->first.c_str()),
            UTF8ToWide(it->second.c_str())));
  }
  Unlock();
}

void CefRequestImpl::GetHeaderMap(WebRequest::HeaderMap& map)
{
  Lock();
  HeaderMap::const_iterator it = headermap_.begin();
  for(; it != headermap_.end(); ++it) {
    map.insert(
        std::make_pair(
            WideToUTF8(it->first.c_str()),
            WideToUTF8(it->second.c_str())));
  }
  Unlock();
}


CefRefPtr<CefPostData> CefPostData::CreatePostData()
{
  CefRefPtr<CefPostData> postdata(new CefPostDataImpl());
  return postdata;
}

CefPostDataImpl::CefPostDataImpl()
{
}

CefPostDataImpl::~CefPostDataImpl()
{
  RemoveElements();
}

size_t CefPostDataImpl::GetElementCount()
{
  Lock();
  size_t ct = elements_.size();
  Unlock();
  return ct;
}

void CefPostDataImpl::GetElements(ElementVector& elements)
{
  Lock();
  elements = elements_;
  Unlock();
}

bool CefPostDataImpl::RemoveElement(CefRefPtr<CefPostDataElement> element)
{
  bool deleted = false;

  Lock();

  ElementVector::iterator it = elements_.begin();
  for(; it != elements_.end(); ++it) {
    if(it->get() == element.get()) {
      elements_.erase(it);
      deleted = true;
      break;
    }
  }

  Unlock();
  
  return deleted;
}

bool CefPostDataImpl::AddElement(CefRefPtr<CefPostDataElement> element)
{
  bool found = false;
  
  Lock();
  
  // check that the element isn't already in the list before adding
  ElementVector::const_iterator it = elements_.begin();
  for(; it != elements_.end(); ++it) {
    if(it->get() == element.get()) {
      found = true;
      break;
    }
  }

  if(!found)
    elements_.push_back(element);
 
  Unlock();
  return !found;
}

void CefPostDataImpl::RemoveElements()
{
  Lock();
  elements_.clear();
  Unlock();
}

void CefPostDataImpl::Set(const net::UploadData& data)
{
  Lock();

  CefRefPtr<CefPostDataElement> postelem;
  const std::vector<net::UploadData::Element>& elements = data.elements();
  std::vector<net::UploadData::Element>::const_iterator it = elements.begin();
  for (; it != elements.end(); ++it) {
    postelem = CefPostDataElement::CreatePostDataElement();
    static_cast<CefPostDataElementImpl*>(postelem.get())->Set(*it);
    AddElement(postelem);
  }

  Unlock();
}

void CefPostDataImpl::Get(net::UploadData& data)
{
  Lock();

  net::UploadData::Element* element;
  ElementVector::iterator it = elements_.begin();
  for(; it != elements_.end(); ++it) {
    element = new net::UploadData::Element();
    static_cast<CefPostDataElementImpl*>(it->get())->Set(*element);
  }

  Unlock();
}


CefRefPtr<CefPostDataElement> CefPostDataElement::CreatePostDataElement()
{
  CefRefPtr<CefPostDataElement> element(new CefPostDataElementImpl());
  return element;
}

CefPostDataElementImpl::CefPostDataElementImpl()
{
  type_ = PDE_TYPE_EMPTY;
}

CefPostDataElementImpl::~CefPostDataElementImpl()
{
  SetToEmpty();
}

void CefPostDataElementImpl::SetToEmpty()
{
  Lock();
  if(type_ == PDE_TYPE_BYTES)
    free(data_.bytes.bytes);
  else if(type_ == PDE_TYPE_FILE)
    free(data_.filename);
  type_ = PDE_TYPE_EMPTY;
  Unlock();
}

void CefPostDataElementImpl::SetToFile(const std::wstring& fileName)
{
  Lock();
  // Clear any data currently in the element
  SetToEmpty();

  // Assign the new file name
  size_t size = fileName.size();
  wchar_t* data = static_cast<wchar_t*>(malloc((size + 1) * sizeof(wchar_t)));
  DCHECK(data != NULL);
  if(data == NULL)
    return;

  memcpy(static_cast<void*>(data), static_cast<const void*>(fileName.c_str()),
      size * sizeof(wchar_t));
  data[size] = 0;

  // Assign the new data
  type_ = PDE_TYPE_FILE;
  data_.filename = data;
  Unlock();
}

void CefPostDataElementImpl::SetToBytes(size_t size, const void* bytes)
{
  Lock();
  // Clear any data currently in the element
  SetToEmpty();

  // Assign the new data
  void* data = malloc(size);
  DCHECK(data != NULL);
  if(data == NULL)
    return;

  memcpy(data, bytes, size);
  
  type_ = PDE_TYPE_BYTES;
  data_.bytes.bytes = data;
  data_.bytes.size = size;
  Unlock();
}

CefPostDataElement::Type CefPostDataElementImpl::GetType()
{
  Lock();
  CefPostDataElement::Type type = type_;
  Unlock();
  return type;
}

std::wstring CefPostDataElementImpl::GetFile()
{
  Lock();
  DCHECK(type_ == PDE_TYPE_FILE);
  std::wstring filename;
  if(type_ == PDE_TYPE_FILE)
    filename = data_.filename;
  Unlock();
  return filename;
}

size_t CefPostDataElementImpl::GetBytesCount()
{
  Lock();
  DCHECK(type_ == PDE_TYPE_BYTES);
  size_t size = 0;
  if(type_ == PDE_TYPE_BYTES)
    size = data_.bytes.size;
  Unlock();
  return size;
}

size_t CefPostDataElementImpl::GetBytes(size_t size, void *bytes)
{
  Lock();
  DCHECK(type_ == PDE_TYPE_BYTES);
  size_t rv = 0;
  if(type_ == PDE_TYPE_BYTES) {
    rv = (size < data_.bytes.size ? size : data_.bytes.size);
    memcpy(bytes, data_.bytes.bytes, rv);
  }
  Unlock();
  return rv;
}

void CefPostDataElementImpl::Set(const net::UploadData::Element& element)
{
  Lock();

  if (element.type() == net::UploadData::TYPE_BYTES) {
    SetToBytes(element.bytes().size(),
        static_cast<const void*>(
            std::string(element.bytes().begin(),
            element.bytes().end()).c_str()));
  } else if (element.type() == net::UploadData::TYPE_FILE) {
    SetToFile(element.file_path().value());
  } else {
    NOTREACHED();
  }

  Unlock();
}

void CefPostDataElementImpl::Get(net::UploadData::Element& element)
{
  Lock();

  if(type_ == PDE_TYPE_BYTES) {
    element.SetToBytes(static_cast<char*>(data_.bytes.bytes), data_.bytes.size);
  } else if(type_ == PDE_TYPE_FILE) {
    element.SetToFilePath(FilePath(data_.filename));
  } else {
    NOTREACHED();
  }

  Unlock();
}
