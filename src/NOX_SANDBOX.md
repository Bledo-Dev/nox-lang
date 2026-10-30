# Nox sandbox

`sandbox[[ ... ]]` runs the code inside in its own private set of globals.
Anything it creates or changes there disappears when it finishes.

```nox
var = "test"

sandbox[[
  import(variable, "var")
  print(var)  -- test
  var = "only inside the sandbox"
]]

print(var)  -- test
```

`sandbox` is an ordinary function that receives the code as a string, so
`sandbox("print(1)")` works too. If the code itself contains `]]` (for
example `a[b[1]]`), use the long form `sandbox[==[ ... ]==]`. Syntax errors
inside are reported when the sandbox runs, not by `noxc -p`.

## What the sandbox starts with

Only `assert`, `error`, `ipairs`, `next`, `pairs`, `pcall`, `print`,
`select`, `tonumber`, `tostring`, `type` and `xpcall`, plus `import`.
These are always the original built-in versions, even if your script has
replaced its own `print` or `tostring`.

There is no `os`, `io`, `load`, `require`, `debug`, `package`, `web`,
`setmetatable`, `getmetatable`, raw-table, or garbage-collector access.

## import

`import(kind, "name")` copies one of the main script's **global** variables
into the sandbox. A `local` is never visible. The kind label documents your
intent; all kinds behave the same:

- `import(variable, "name")` for ordinary values (the usual choice)
- `import(func, "name")` for a function
- `import(datatype, "name")`, `import(tabletype, "name")`,
  `import(value, "name")`, or `import(module, "name")`

Tables are copied (without metatables, up to 100 levels deep, cycles kept),
so the sandbox cannot change your originals.

## What can never be imported

- Any built-in global by name (`os`, `io`, `load`, `_G`, `string`, `arg`, ...).
- Any built-in value under a different name or inside a table, e.g.
  `myos = os` or `tools = { run = os.execute }` or a copy of
  `package.searchers`.
- Any C function (every built-in function is one, including ones they
  create later, such as `io.lines(...)` iterators), userdata (such as open
  files), and coroutines.

## Things to know

- **The code inside chooses what to import.** If you run code written by
  someone else, it can import any of your non-built-in globals by name.
  Keep secrets (passwords, tokens) in `local` variables.
- **Imported functions run with full power.** A function you wrote runs
  with your normal globals, so whatever it does or returns (a file it opened,
  an error object) reaches the sandbox. Only import functions you trust.
- **String methods work inside**, e.g. `("abc"):upper()`, but only the
  original built-in ones: if your script adds its own (`string.shell = ...`),
  the sandbox does not see them. Likewise, custom metatables you set on
  numbers, `nil`, booleans or functions (with `debug.setmetatable`) are
  switched off while a sandbox runs, including while it calls a function you
  imported, and switched back on afterwards. `("x"):rep(n)` can still use a
  lot of memory.
- **There is no time or memory limit yet.** `sandbox[[ while true do end ]]`
  never returns.
