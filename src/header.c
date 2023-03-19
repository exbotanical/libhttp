#include "header.h"

#include <pthread.h>  // for pthread_once
#include <stdbool.h>
#include <string.h>  // for strlen

#include "libhttp.h"
#include "xmalloc.h"

static pthread_once_t init_common_headers_once = PTHREAD_ONCE_INIT;
static pthread_once_t init_singleton_headers_once = PTHREAD_ONCE_INIT;

// common_headers interns common header strings
static hash_set* common_headers;

// singleton_headers interns common header strings
static hash_set* singleton_headers;

// to_lower is used to convert chars to lower case
static const int to_lower = 'a' - 'A';

/**
 * init_common_headers initializes the common headers hash set
 */
static void init_common_headers() {
  common_headers = hs_init(39);

  hs_insert(common_headers, "Accept");
  hs_insert(common_headers, "Accept-Charset");
  hs_insert(common_headers, "Accept-Encoding");
  hs_insert(common_headers, "Accept-Language");
  hs_insert(common_headers, "Accept-Ranges");
  hs_insert(common_headers, "Cache-Control");
  hs_insert(common_headers, "Cc");
  hs_insert(common_headers, "Connection");
  hs_insert(common_headers, "Content-Id");
  hs_insert(common_headers, "Content-Language");
  hs_insert(common_headers, "Content-Length");
  hs_insert(common_headers, "Content-Transfer-Encoding");
  hs_insert(common_headers, "Content-Type");
  hs_insert(common_headers, "Cookie");
  hs_insert(common_headers, "Date");
  hs_insert(common_headers, "Dkim-Signature");
  hs_insert(common_headers, "Etag");
  hs_insert(common_headers, "Expires");
  hs_insert(common_headers, "From");
  hs_insert(common_headers, "Host");
  hs_insert(common_headers, "If-Modified-Since");
  hs_insert(common_headers, "If-None-Match");
  hs_insert(common_headers, "In-Reply-To");
  hs_insert(common_headers, "Last-Modified");
  hs_insert(common_headers, "Location");
  hs_insert(common_headers, "Message-Id");
  hs_insert(common_headers, "Mime-Version");
  hs_insert(common_headers, "Pragma");
  hs_insert(common_headers, "Received");
  hs_insert(common_headers, "Return-Path");
  hs_insert(common_headers, "Server");
  hs_insert(common_headers, "Set-Cookie");
  hs_insert(common_headers, "Subject");
  hs_insert(common_headers, "To");
  hs_insert(common_headers, "User-Agent");
  hs_insert(common_headers, "Via");
  hs_insert(common_headers, "X-Forwarded-For");
  hs_insert(common_headers, "X-Imforwards");
  hs_insert(common_headers, "X-Powered-By");
}

/**
 * init_singleton_headers initializes the singleton headers hash set. A
 * singleton header is a header that cannot be duplicated for a request per
 * RFC 7230.
 * TODO: fix + test
 * TODO: case sensitive?
 */
static void init_singleton_headers() {
  singleton_headers = hs_init(3);

  hs_insert(singleton_headers, "Content-Type");
  hs_insert(singleton_headers, "Content-Length");
  hs_insert(singleton_headers, "Host");
}

/**
 * get_common_header_set atomically initializes and retrieves the common headers
 * hash set. Initialization is guaranteed to only run once, on the first
 * invocation
 *
 * @return hash_set*
 */
static hash_set* get_common_header_set() {
  pthread_once(&init_common_headers_once, init_common_headers);

  return common_headers;
}

/**
 * get_singleton_header_set atomically initializes and retrieves the singleton
 * headers hash set. Initialization is guaranteed to only run once, on the first
 * invocation
 *
 * @return hash_set*
 */
static hash_set* get_singleton_header_set() {
  pthread_once(&init_singleton_headers_once, init_singleton_headers);

  return singleton_headers;
}

/**
 * is_valid_header_field_byte returns a bool indicating whether the given byte
 * is a valid header char
 *
 * @param b
 * @return bool
 */
static bool is_valid_header_field_byte(int b) {
  return b < sizeof(token_table) && token_table[b];
}

/**
 * is_singleton_header returns a bool indicating whether the given header
 * `header_key` is a singleton header
 *
 * @param header_key
 * @return bool
 */
static bool is_singleton_header(const char* header_key) {
  return hs_contains(get_singleton_header_set(), header_key);
}

/**
 * canonical_mime_header_key modifies a header key into a valid format
 *
 * @param s
 * @return char*
 */
static char* canonical_mime_header_key(char* s) {
  hash_set* common_headers = get_common_header_set();
  // Avoid needless computation if we know the header is already correct
  if (hs_contains(common_headers, s)) {
    return s;
  }

  // See if it looks like a header key; if not return it as-is
  for (unsigned int i = 0; i < strlen(s); i++) {
    char c = s[i];
    if (is_valid_header_field_byte(c)) {
      continue;
    }

    return s;
  }

  bool upper = true;
  for (unsigned int i = 0; i < strlen(s); i++) {
    char c = s[i];
    // Canonicalize: first letter upper case and upper case after each dash
    // MIME headers are ASCII only, so no Unicode issues
    if (upper && 'a' <= c && c <= 'z') {
      c -= to_lower;
    } else if (!upper && 'A' <= c && c <= 'Z') {
      c += to_lower;
    }

    // TODO: fix ... ugh this is disgusting
    char* ca = xmalloc(strlen(s) + 1);
    strncpy(ca, s, strlen(s));
    ca[strlen(s)] = '\0';
    ca[i] = c;
    s = ca;

    upper = c == '-';  // for next time
  }

  return s;
}

char* to_canonical_MIME_header_key(char* s) {
  // Check for canonical encoding
  bool upper = true;
  for (unsigned int i = 0; i < strlen(s); i++) {
    char c = s[i];
    if (!is_valid_header_field_byte(c)) {
      return s;
    }

    if (upper && 'a' <= c && c <= 'z') {
      return canonical_mime_header_key(s);
    }

    if (!upper && 'A' <= c && c <= 'Z') {
      return canonical_mime_header_key(s);
    }

    upper = c == '-';
  }

  return s;
}

char* req_header_get(hash_table* headers, const char* key) {
  ht_record* header = ht_search(headers, key);
  if (!header) {
    return NULL;
  }

  return array_get(header->value, 0);
}

char** req_header_values(hash_table* headers, const char* key) {
  ht_record* header = ht_search(headers, key);
  if (!header) {
    return NULL;
  }

  unsigned int sz = array_size(header->value);
  char** headers_list = xmalloc(sz);

  for (unsigned int i = 0; i < sz; i++) {
    headers_list[i] = (char*)array_get(header->value, i);
  }

  return headers_list;
}

void res_header_append(array_t* headers, const char* key, const char* value) {
  header_t* h = xmalloc(sizeof(header_t));

  h->key = key;
  h->value = value;
  array_push(headers, h);
}

bool insert_header(hash_table* headers, const char* k, const char* v) {
  // Check if we've already encountered this header key. Some headers cannot
  // be duplicated e.g. Content-Type, so we'll need to handle those as well
  ht_record* existing_headers = ht_search(headers, k);
  if (existing_headers) {
    // Disallow duplicates where necessary e.g. multiple Content-Type headers is
    // a 400 This follows Section 4.2 of RFC 7230 to ensure we handle multiples
    // of the same header correctly
    // TODO: handle all singletons
    if (is_singleton_header(k)) {
      return false;
    }

    // TODO: test
    array_push(existing_headers->value, (void*)v);
  } else {
    // We haven't encountered this header before; insert into the hash table
    // along with the first value
    array_t* values = array_init();
    if (!values) {
      // TODO: die
    }

    array_push(values, (void*)v);
    ht_insert(headers, k, values);
  }

  return true;
}
