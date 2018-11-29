#include "Cache.h"

CachedRoute::CachedRoute(std::string url,
    std::unique_ptr<folly::IOBuf> data,
    std::shared_ptr<proxygen::HTTPMessage> headers) {
    url_ = std::move(url);
    content_ = std::move(data);
    headers_ = std::move(headers);
    // TODO: take the sha256 hash of the content_ and assign it to the sha256 member here
    auto out = std::vector<uint8_t>(32);
    folly::ssl::OpenSSLHash::sha256(folly::range(out), *(content_.get()));
    sha256_ = folly::hexlify(out);
    std::cout << sha256_ << "\n";
}

CachedRoute::~CachedRoute() {
    std::cout << "CachedRoute deleted\n";
}

std::string CachedRoute::getURL() {
    return url_;
}

std::unique_ptr<folly::IOBuf> CachedRoute::getContent() {
    return content_.get()->clone();
}

std::shared_ptr<proxygen::HTTPMessage> CachedRoute::getHeaders() {
    return headers_;
}

/////////////////////////////////////////////////////////////////////////

std::shared_ptr<CachedRoute> MemoryCache::getCachedRoute(std::string url) {
    folly::Optional<std::shared_ptr<CachedRoute>> cached = cache_.get(url);
    if (cached) {
        return cached.value();
    }
    return nullptr;
}

void MemoryCache::addCachedRoute(std::string url,
    std::unique_ptr<folly::IOBuf> chain,
    std::shared_ptr<proxygen::HTTPMessage> headers) {

    // Create a new CachedRoute class
    std::shared_ptr<CachedRoute> newEntry = std::make_shared<CachedRoute>(url, chain.get()->clone(), std::move(headers));
    
    // Insert the CachedRoute class into the cache
    cache_.put(url, newEntry);
}

size_t MemoryCache::size() const { return cache_.size(); }
