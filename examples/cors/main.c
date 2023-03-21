#include <stdlib.h>

#include "libhttp.h"

#define PORT 9000

res_t *handler(req_t *req, res_t *res) {
  set_header(res, "Content-Type", "text/plain");
  set_header(res, "X-Powered-By", "demo");

  set_body(res, "Hello World!");
  set_status(res, STATUS_OK);

  return res;
}

int main() {
  router_attr_t attr = ROUTE_ATTR_INITIALIZER;
  router_t *router = router_init(attr);
  router_register(router, "/", handler, middlewares(use_cors(cors_allow_all())),
                  METHOD_GET, METHOD_OPTIONS, NULL);

  server_t *server = server_init(router, PORT);
  if (!server_start(server)) {
    server_free(server);

    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
