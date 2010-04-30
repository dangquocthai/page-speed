// Copyright 2010 Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CACHE_URL_FETCHER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CACHE_URL_FETCHER_H_

#include "base/scoped_ptr.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_async_fetcher.h"
#include "net/instaweb/util/public/url_fetcher.h"

namespace net_instaweb {

class HTTPCache;
class MessageHandler;
class UrlAsyncFetcher;

// Composes a URL fetcher with an http cache, to generate a caching
// URL fetcher.
//
// This fetcher will return true and provide an immediate result for
// entries in the cache.  When entries are not in the cache, it will
// initiate an asynchronous 'get' and store the result in the cache.
//
// See also CacheUrlAsyncFetcher, which will yield its results asynchronously
// for elements not in the cache, and immediately for results that are.
class CacheUrlFetcher : public UrlFetcher {
 public:
  CacheUrlFetcher(HTTPCache* cache, UrlFetcher* fetcher)
      : http_cache_(cache),
        sync_fetcher_(fetcher),
        async_fetcher_(NULL) {
  }
  CacheUrlFetcher(HTTPCache* cache, UrlAsyncFetcher* fetcher)
      : http_cache_(cache),
        sync_fetcher_(NULL),
        async_fetcher_(fetcher) {
  }
  virtual ~CacheUrlFetcher();

  virtual bool StreamingFetchUrl(
      const std::string& url,
      const MetaData& request_headers,
      MetaData* response_headers,
      Writer* fetched_content_writer,
      MessageHandler* message_handler);

  // Helper class to hold state for a single asynchronous fetch.  When
  // the fetch is complete, we'll put the resource in the cache.
  //
  // This class is declared here to facilitate code-sharing with
  // CacheAsyncUrlFetcher.
  class AsyncFetch : public UrlAsyncFetcher::Callback {
   public:
    AsyncFetch(const StringPiece& url, HTTPCache* cache,
               MessageHandler* handler);
    virtual ~AsyncFetch();

    virtual void Done(bool success);
    void Start(UrlAsyncFetcher* fetcher, const MetaData& request_headers);

    // This hook allows the CacheUrlAsyncFetcher to capture the headers for
    // its client, while still enabling this class to cache them.
    virtual MetaData* ResponseHeaders() = 0;

    void UpdateCache();

   protected:
    std::string content_;
    MessageHandler* message_handler_;

   private:
    std::string url_;
    StringWriter writer_;
    HTTPCache* http_cache_;
    Callback* callback_;
  };


 private:
  HTTPCache* http_cache_;
  UrlFetcher* sync_fetcher_;
  UrlAsyncFetcher* async_fetcher_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CACHE_URL_FETCHER_H_
