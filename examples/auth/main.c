#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

#include "deps/jsob/jsob.h"
#include "libhash/libhash.h"
#include "libhttp.h"

#define PORT 9000
#define SESSION_TIMEOUT_MINUTES 10

// TODO: mutex
hash_table *session_store;
hash_table *user_store;

char *COOKIE_ID = "DemoCookie";

const int PASSWORD_HASH_KEY = 7;

typedef struct {
  char *username;
  char *password;
} user;

bool eq(const char *s1, const char *s2) { return strcmp(s1, s2) == 0; }

char *create_sid() {
  char uuid[UUID_STR_LEN];

  uuid_t bin_uuid;
  uuid_generate_random(bin_uuid);
  uuid_unparse(bin_uuid, uuid);

  return strdup(uuid);
}

user *create_user(char *username, char *password) {
  user *u = malloc(sizeof(user));
  u->username = username;
  u->password = password;

  return u;
}

bool user_exists(char *username) {
  return ht_search(user_store, username) != NULL;
}

// This is for the demo ONLY! Do NOT use this for actual auth - it is NOT
// secure!!!
char *hash_password(char *pw) {
  char ret[sizeof(pw) + 1];

  int i = 0;
  for (; i < strlen(pw); i++) {
    ret[i] = (pw[i] ^ PASSWORD_HASH_KEY);
  }

  ret[i] = '\0';

  return strdup(ret);
}

response *index_handler(request *req, response *res) {
  set_status(res, STATUS_OK);
  set_body(res, from_file("./index.html"));

  return res;
}

response *css_handler(request *req, response *res) {
  set_status(res, STATUS_OK);
  set_body(res, from_file("./style.css"));

  return res;
}

response *data_handler(request *req, response *res) {
  cookie *c = get_cookie(req, COOKIE_ID);
  char *sid = cookie_get_value(c);
  char *username = ht_get(session_store, sid);

  set_body(res, fmt_str("{ \"data\": \"Hello, %s!\" }", username));
  set_header(res, "Content-Type", "application/json");
  set_status(res, STATUS_OK);

  return res;
}

response *register_handler(request *req, response *res) {
  char *username = jsob_getstr(request_get_body(req), "username");
  if (!username) {
    set_status(res, STATUS_BAD_REQUEST);
    set_body(res, "Must provide a username");
    return res;
  }

  char *password = jsob_getstr(request_get_body(req), "password");
  if (!password) {
    set_status(res, STATUS_BAD_REQUEST);
    set_body(res, "Must provide a password");
    return res;
  }

  if (user_exists(username)) {
    set_status(res, STATUS_BAD_REQUEST);
    set_body(res, fmt_str("Username %s exists", username));  // TODO: fmt
    return res;
  }

  user *u = create_user(username, hash_password(password));
  ht_insert(user_store, username, u);

  char *sid = create_sid();
  ht_insert(session_store, sid, username);

  cookie *c = cookie_init(COOKIE_ID, sid);
  cookie_set_expires(c, n_minutes_from_now(SESSION_TIMEOUT_MINUTES));

  set_cookie(res, c);

  set_status(res, STATUS_CREATED);
  return res;
}

response *login_handler(request *req, response *res) {
  char *username = jsob_getstr(request_get_body(req), "username");
  if (!username) {
    set_status(res, STATUS_BAD_REQUEST);
    set_body(res, "Must provide a username");
    return res;
  }

  char *password = jsob_getstr(request_get_body(req), "password");
  if (!password) {
    set_status(res, STATUS_BAD_REQUEST);
    set_body(res, "Must provide a password");
    return res;
  }

  if (!user_exists(username)) {
    set_status(res, STATUS_BAD_REQUEST);
    set_body(res, "invalid credentials");

    return res;
  }

  user *u = (char *)ht_get(user_store, username);

  if (!u || !s_equals(hash_password(u->password), password)) {
    set_status(res, STATUS_BAD_REQUEST);
    set_body(res, "invalid credentials");

    return res;
  }

  char *session_id = create_sid();
  ht_insert(session_store, session_id, username);

  cookie *c = cookie_init(COOKIE_ID, session_id);
  cookie_set_expires(c, n_minutes_from_now(SESSION_TIMEOUT_MINUTES));
  cookie_set_path(c, "/");
  set_cookie(res, c);

  set_status(res, STATUS_OK);

  return res;
}

response *logout_handler(request *req, response *res) {
  cookie *c = get_cookie(req, COOKIE_ID);
  if (!c) {
    set_status(res, STATUS_UNAUTHORIZED);
    res->done = true;  // TODO: just don't return?

    return res;
  }

  char *sid = cookie_get_value(c);
  if (!sid) {
    set_status(res, STATUS_UNAUTHORIZED);
    res->done = true;

    return res;
  }

  ht_delete(session_store, sid);

  cookie_set_max_age(c, -1);
  set_cookie(res, c);

  return res;
}

response *auth_middleware(request *req, response *res) {
  set_header(res, "X-Authorized-By", "TheDemoApp");
  if (eq(request_get_path(req), "/") ||
      eq(request_get_path(req), "/style.css") ||
      eq(request_get_path(req), "/login") ||
      eq(request_get_path(req), "/register")) {
    return res;
  }

  cookie *c = get_cookie(req, COOKIE_ID);

  if (!c) {
    set_status(res, STATUS_UNAUTHORIZED);
    res->done = true;  // TODO: just don't return?

    return res;
  }

  char *sid = cookie_get_value(c);

  char *username = ht_get(session_store, sid);
  if (!username || !user_exists(username)) {
    set_status(res, STATUS_UNAUTHORIZED);
    res->done = true;

    return res;
  }

  cookie_set_expires(c, n_minutes_from_now(SESSION_TIMEOUT_MINUTES));
  set_cookie(res, c);

  return res;
}

// TODO: cors
int main() {
  session_store = ht_init(0);
  user_store = ht_init(0);

  router_attr *attr = router_attr_init();
  add_middleware(attr, auth_middleware);

  http_router *router = router_init(attr);

  router_register(router, "/register", register_handler, METHOD_POST, NULL);
  router_register(router, "/login", login_handler, METHOD_POST, NULL);
  router_register(router, "/logout", logout_handler, METHOD_POST, NULL);
  router_register(router, "/data", data_handler, METHOD_GET, NULL);

  router_register(router, "/", index_handler, METHOD_GET, NULL);
  router_register(router, "/style.css", css_handler, METHOD_GET, NULL);

  tcp_server *server = server_init(router, PORT);
  server_start(server);
  server_free(server);

  return EXIT_SUCCESS;
}
