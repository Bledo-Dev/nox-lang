# Nox web requests (Windows)

`web.fetch(url[, options])` makes an HTTP or HTTPS request and returns the
response body and HTTP status code. For failures it returns `nil, error`.

```nox
local body, status = web.fetch("https://api.example.com/profile", {
  headers = {
    ["Authorization"] = "Bearer " .. token,
    ["Accept"] = "application/json"
  },
  timeout = 15
})
```

`timeout` is in seconds (decimals like `2.5` are fine). A non-2xx response
is still returned as `body, status`, so check the status. Network problems
return `nil` plus a readable message such as
`web.fetch: could not find host (sending request)`. Bad options (a header that is
not a string, a header containing a line break, a negative timeout) raise an
error right away.

Use `method` and `body` for sign-in/API POST requests:

```nox
local response, status = web.fetch("https://api.example.com/sign-in", {
  method = "POST",
  body = '{"email":"person@example.com","password":"secret"}',
  headers = { ["Content-Type"] = "application/json" },
  timeout = 15
})
```

Redirects are followed automatically, except when the request has an
`Authorization`, `Proxy-Authorization` or `Cookie` header: then a redirect is
returned as-is (status 301/302/...), so a login is never passed on to a
different site. Use `redirects = true` or `redirects = false` to choose
yourself. Logins written into the URL (`https://user:pass@...`) are refused;
use an `Authorization` header instead.

Do not commit passwords, tokens, or API keys to source files. Prefer an
environment-provided value or an OS secret store, then send it in the API's
documented `Authorization` header. `web.fetch` permits HTTP for local testing,
but use HTTPS for authentication and real API traffic.

Windows builds must link against WinHTTP: add `-lwinhttp` to a direct `gcc`
command. The Nox `mingw` Makefile target already includes it.
