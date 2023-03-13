#include <stdarg.h>  // for variadic args functions

#include "libhttp.h"

// TODO: replace with a generic `collect_x`
array_t *collect_middleware(handler_t *middleware, ...) {
  array_t *middlewares = array_init();
  if (!middlewares) {
    return NULL;
  }

  va_list args;
  va_start(args, middleware);

  while (middleware != NULL) {
    if (!array_push(middlewares, middleware)) {
      return NULL;
    }

    middleware = va_arg(args, handler_t *);
  }

  va_end(args);

  return middlewares;
}
