# Nox

Nox is a small scripting language based on [Lua 5.5](https://www.lua.org/).
*Nox* means "night" in Latin, a nod to Lua ("moon").

It's the language behind the Nox game engine, but it works on its own too:
it's ordinary Lua plus a few things that are handy for games and apps.

## What Nox adds to Lua

**Sandboxing.** Run code with its own private globals and no access to files,
the OS or the network. Changes inside don't leak out.

```lua
playerName = "Player1"

sandbox[[
  import(variable, "playerName")
  print(playerName)      -- Player1
  playerName = "hacked"  -- only changes the sandbox's copy
  print(os, io)          -- nil  nil
]]

print(playerName)        -- Player1
```

More in [src/NOX_SANDBOX.md](src/NOX_SANDBOX.md).

**JSON.** Turn JSON text into tables.

```lua
local data = json.decode('{"name": "Nox", "items": ["one", "two"], "nothing": null}')
print(data.name, data.items[2])     -- Nox  two
print(data.nothing == json.null)    -- true
```

**Web requests** (Windows builds for now). HTTP and HTTPS with headers and a timeout.

```lua
local body, status = web.fetch("https://httpbin.org/json", { timeout = 10 })
if body then
  local data = json.decode(body)
  print(status, data.slideshow.title)
end
```

More in [src/NOX_WEB.md](src/NOX_WEB.md). On other systems `web.fetch` returns
`nil` and a message saying so.

**Luau-style shortcuts.** `x += 1` (and `-= *= /= //= %= ^= ..=`), and `continue` in loops.

```lua
local score = 0
for i = 1, 10 do
  if i % 2 == 0 then continue end
  score += i
end
print(score)   -- 25
```

Everything else is Lua 5.5, so the [Lua manual](https://www.lua.org/manual/5.5/)
applies.

## Building

You get two programs: `nox` (runs scripts) and `noxc` (compiles them to bytecode).

**Windows** (with [w64devkit](https://github.com/skeeto/w64devkit) or MinGW):

```
cd src
make mingw
```

Or run `build_nox.bat`. It expects w64devkit at `C:\LocalCompile\Compilers\w64devkit`,
so change that line if yours is somewhere else. It builds and runs a few checks,
writing the results to `build_log.txt`.

**Linux / macOS:**

```
cd src
make linux     # or: make macosx
```

## Running

```
nox script.nox
noxc -o script.out script.nox   # compile
nox script.out                  # run the compiled file
```

## Tests

`run_nox_tests.bat` runs every script in `tests/`, both as source and compiled
with `noxc`, and checks the two give the same output. Results go to `run_log.txt`.
The sandbox tests try to break out of the sandbox; they should all pass.

## License

MIT, see [LICENSE](LICENSE). Nox is based on Lua, which is also MIT
(Copyright (C) 1994-2026 Lua.org, PUC-Rio); that notice is in LICENSE too.
