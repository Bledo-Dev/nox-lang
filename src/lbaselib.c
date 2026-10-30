/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in lua.h
*/

#define lbaselib_c
#define LUA_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#endif

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "llimits.h"


static int luaB_print (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = luaL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      lua_writestring("\t", 1);  /* add a tab before it */
    lua_writestring(s, l);  /* print it */
    lua_pop(L, 1);  /* pop result */
  }
  lua_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int luaB_warn (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  luaL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    luaL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    lua_warning(L, lua_tostring(L, i), 1);
  lua_warning(L, lua_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, unsigned base, lua_Integer *pn) {
  lua_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle sign */
  else if (*s == '+') s++;
  if (!isalnum(cast_uchar(*s)))  /* no digit? */
    return NULL;
  do {
    unsigned digit = cast_uint(isdigit(cast_uchar(*s))
                               ? *s - '0'
                               : (toupper(cast_uchar(*s)) - 'A') + 10);
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum(cast_uchar(*s)));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (lua_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int luaB_tonumber (lua_State *L) {
  if (lua_isnoneornil(L, 2)) {  /* standard conversion? */
    if (lua_type(L, 1) == LUA_TNUMBER) {  /* already a number? */
      lua_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = lua_tolstring(L, 1, &l);
      if (s != NULL && lua_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      luaL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    lua_Integer n = 0;  /* to avoid warnings */
    lua_Integer base = luaL_checkinteger(L, 2);
    luaL_checktype(L, 1, LUA_TSTRING);  /* no numbers as strings */
    s = lua_tolstring(L, 1, &l);
    luaL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, cast_uint(base), &n) == s + l) {
      lua_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  luaL_pushfail(L);  /* not a number */
  return 1;
}


static int luaB_error (lua_State *L) {
  int level = (int)luaL_optinteger(L, 2, 1);
  lua_settop(L, 1);
  if (lua_type(L, 1) == LUA_TSTRING && level > 0) {
    luaL_where(L, level);   /* add extra information */
    lua_pushvalue(L, 1);
    lua_concat(L, 2);
  }
  return lua_error(L);
}


static int luaB_getmetatable (lua_State *L) {
  luaL_checkany(L, 1);
  if (!lua_getmetatable(L, 1)) {
    lua_pushnil(L);
    return 1;  /* no metatable */
  }
  luaL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int luaB_setmetatable (lua_State *L) {
  int t = lua_type(L, 2);
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_argexpected(L, t == LUA_TNIL || t == LUA_TTABLE, 2, "nil or table");
  if (l_unlikely(luaL_getmetafield(L, 1, "__metatable") != LUA_TNIL))
    return luaL_error(L, "cannot change a protected metatable");
  lua_settop(L, 2);
  lua_setmetatable(L, 1);
  return 1;
}


static int luaB_rawequal (lua_State *L) {
  luaL_checkany(L, 1);
  luaL_checkany(L, 2);
  lua_pushboolean(L, lua_rawequal(L, 1, 2));
  return 1;
}


static int luaB_rawlen (lua_State *L) {
  int t = lua_type(L, 1);
  luaL_argexpected(L, t == LUA_TTABLE || t == LUA_TSTRING, 1,
                      "table or string");
  lua_pushinteger(L, l_castU2S(lua_rawlen(L, 1)));
  return 1;
}


static int luaB_rawget (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  lua_settop(L, 2);
  lua_rawget(L, 1);
  return 1;
}

static int luaB_rawset (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  luaL_checkany(L, 3);
  lua_settop(L, 3);
  lua_rawset(L, 1);
  return 1;
}


static int pushmode (lua_State *L, int oldmode) {
  if (oldmode == -1)
    luaL_pushfail(L);  /* invalid call to 'lua_gc' */
  else
    lua_pushstring(L, (oldmode == LUA_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'lua_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int luaB_collectgarbage (lua_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "isrunning", "generational", "incremental",
    "param", NULL};
  static const char optsnum[] = {LUA_GCSTOP, LUA_GCRESTART, LUA_GCCOLLECT,
    LUA_GCCOUNT, LUA_GCSTEP, LUA_GCISRUNNING, LUA_GCGEN, LUA_GCINC,
    LUA_GCPARAM};
  int o = optsnum[luaL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case LUA_GCCOUNT: {
      int k = lua_gc(L, o);
      int b = lua_gc(L, LUA_GCCOUNTB);
      checkvalres(k);
      lua_pushnumber(L, (lua_Number)k + ((lua_Number)b/1024));
      return 1;
    }
    case LUA_GCSTEP: {
      lua_Integer n = luaL_optinteger(L, 2, 0);
      int res = lua_gc(L, o, cast_sizet(n));
      checkvalres(res);
      lua_pushboolean(L, res);
      return 1;
    }
    case LUA_GCISRUNNING: {
      int res = lua_gc(L, o);
      checkvalres(res);
      lua_pushboolean(L, res);
      return 1;
    }
    case LUA_GCGEN: {
      return pushmode(L, lua_gc(L, o));
    }
    case LUA_GCINC: {
      return pushmode(L, lua_gc(L, o));
    }
    case LUA_GCPARAM: {
      static const char *const params[] = {
        "minormul", "majorminor", "minormajor",
        "pause", "stepmul", "stepsize", NULL};
      static const char pnum[] = {
        LUA_GCPMINORMUL, LUA_GCPMAJORMINOR, LUA_GCPMINORMAJOR,
        LUA_GCPPAUSE, LUA_GCPSTEPMUL, LUA_GCPSTEPSIZE};
      int p = pnum[luaL_checkoption(L, 2, NULL, params)];
      lua_Integer value = luaL_optinteger(L, 3, -1);
      lua_pushinteger(L, lua_gc(L, o, p, (int)value));
      return 1;
    }
    default: {
      int res = lua_gc(L, o);
      checkvalres(res);
      lua_pushinteger(L, res);
      return 1;
    }
  }
  luaL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int luaB_type (lua_State *L) {
  int t = lua_type(L, 1);
  luaL_argcheck(L, t != LUA_TNONE, 1, "value expected");
  lua_pushstring(L, lua_typename(L, t));
  return 1;
}


static int luaB_next (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (lua_next(L, 1))
    return 2;
  else {
    lua_pushnil(L);
    return 1;
  }
}


static int pairscont (lua_State *L, int status, lua_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 4;  /* __pairs did all the work, just return its results */
}

static int luaB_pairs (lua_State *L) {
  luaL_checkany(L, 1);
  if (luaL_getmetafield(L, 1, "__pairs") == LUA_TNIL) {  /* no metamethod? */
    lua_pushcfunction(L, luaB_next);  /* will return generator and */
    lua_pushvalue(L, 1);  /* state */
    lua_pushnil(L);  /* initial value */
    lua_pushnil(L);  /* to-be-closed object */
  }
  else {
    lua_pushvalue(L, 1);  /* argument 'self' to metamethod */
    lua_callk(L, 1, 4, 0, pairscont);  /* get 4 values from metamethod */
  }
  return 4;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (lua_State *L) {
  lua_Integer i = luaL_checkinteger(L, 2);
  i = luaL_intop(+, i, 1);
  lua_pushinteger(L, i);
  return (lua_geti(L, 1, i) == LUA_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int luaB_ipairs (lua_State *L) {
  luaL_checkany(L, 1);
  lua_pushcfunction(L, ipairsaux);  /* iteration function */
  lua_pushvalue(L, 1);  /* state */
  lua_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (lua_State *L, int status, int envidx) {
  if (l_likely(status == LUA_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      lua_pushvalue(L, envidx);  /* environment for loaded function */
      if (!lua_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        lua_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    luaL_pushfail(L);
    lua_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static const char *getMode (lua_State *L, int idx) {
  const char *mode = luaL_optstring(L, idx, NULL);
  if (mode != NULL && strchr(mode, 'B') != NULL) {
    /* Lua code cannot use fixed buffers */
    luaL_argerror(L, idx, "invalid mode");
  }
  return mode;
}


static int luaB_loadfile (lua_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  const char *mode = getMode(L, 2);
  int env = (!lua_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = luaL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/


/*
** Reader for generic 'load' function.
*/
static const char *generic_reader (lua_State *L, void *ud, size_t *size) {
  int *firstcall = cast(int *, ud);
  luaL_checkstack(L, 2, "too many nested functions");
  if (*firstcall)
    *firstcall = 0;
  else
    lua_pop(L, 1);  /* remove previous result */
  lua_pushvalue(L, 1);  /* get function */
  lua_call(L, 0, 1);  /* call it */
  if (lua_isnil(L, -1)) {
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!lua_isstring(L, -1)))
    luaL_error(L, "reader function must return a string");
  return lua_tolstring(L, -1, size);
}


static int luaB_load (lua_State *L) {
  int status;
  size_t l;
  const char *s = lua_tolstring(L, 1, &l);
  const char *mode = getMode(L, 3);
  int env = (!lua_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = luaL_optstring(L, 2, s);
    status = luaL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    int firstcall = 1;  /* userdata for generic_reader */
    const char *chunkname = luaL_optstring(L, 2, "=(load)");
    luaL_checktype(L, 1, LUA_TFUNCTION);
    status = lua_load(L, generic_reader, &firstcall, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (lua_State *L, int d1, lua_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'lua_Kfunction' prototype */
  return lua_gettop(L) - 1;
}


static int luaB_dofile (lua_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  lua_settop(L, 1);
  if (l_unlikely(luaL_loadfilex(L, fname, "bt") != LUA_OK))
    return lua_error(L);
  lua_callk(L, 0, LUA_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int luaB_assert (lua_State *L) {
  if (l_likely(lua_toboolean(L, 1)))  /* condition is true? */
    return lua_gettop(L);  /* return all arguments */
  else {  /* error */
    luaL_checkany(L, 1);  /* there must be a condition */
    lua_remove(L, 1);  /* remove it */
    lua_pushliteral(L, "assertion failed!");  /* default message */
    lua_settop(L, 1);  /* leave only message (default if no other one) */
    return luaB_error(L);  /* call 'error' */
  }
}


static int luaB_select (lua_State *L) {
  int n = lua_gettop(L);
  if (lua_type(L, 1) == LUA_TSTRING && *lua_tostring(L, 1) == '#') {
    lua_pushinteger(L, n-1);
    return 1;
  }
  else {
    lua_Integer i = luaL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    luaL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (lua_State *L, int status, lua_KContext extra) {
  if (l_unlikely(status != LUA_OK && status != LUA_YIELD)) {  /* error? */
    lua_pushboolean(L, 0);  /* first result (false) */
    lua_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return lua_gettop(L) - (int)extra;  /* return all results */
}


static int luaB_pcall (lua_State *L) {
  int status;
  luaL_checkany(L, 1);
  lua_pushboolean(L, 1);  /* first result if no errors */
  lua_insert(L, 1);  /* put it in place */
  status = lua_pcallk(L, lua_gettop(L) - 2, LUA_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'lua_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int luaB_xpcall (lua_State *L) {
  int status;
  int n = lua_gettop(L);
  luaL_checktype(L, 2, LUA_TFUNCTION);  /* check error function */
  lua_pushboolean(L, 1);  /* first result */
  lua_pushvalue(L, 1);  /* function */
  lua_rotate(L, 3, 2);  /* move them below function's arguments */
  status = lua_pcallk(L, n - 2, LUA_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int luaB_tostring (lua_State *L) {
  luaL_checkany(L, 1);
  luaL_tolstring(L, 1, NULL);
  return 1;
}


/*
** Nox sandbox.
**
** sandbox(code) runs 'code' with a fresh, private set of globals that holds
** only the harmless functions in 'nox_safe_funcs'.  import(kind, "name")
** copies one of the main script's globals into it.
**
** Protection rules for import:
** - every built-in global name is refused (recorded at startup), plus 'arg';
** - every built-in value is refused, even renamed or hidden inside a table:
**   at startup 'nox_snapshotbuiltins' walks everything reachable from the
**   globals, the registry and the string metatable and records it;
** - no C function can be imported at all (all built-ins are C functions,
**   including ones they create later, such as io.lines iterators);
** - userdata and coroutines cannot be imported;
** - tables are copied (without metatables), at most NOX_IMPORT_MAXDEPTH deep.
** If the startup record is missing, import refuses everything.
*/
#define NOX_BUILTIN_NAMES   "_NOX_BUILTIN_NAMES"
#define NOX_BUILTIN_VALUES  "_NOX_BUILTIN_VALUES"
#define NOX_STRING_META     "_NOX_STRING_META"
#define NOX_IMPORT_MAXDEPTH 100

/* Always refused, even if the startup record is incomplete. */
static const char *const nox_denied_names[] = {
  "_G", "_ENV", "os", "io", "debug", "package", "require", "load",
  "loadfile", "dofile", "loadstring", "setmetatable", "getmetatable",
  "rawset", "rawget", "rawequal", "rawlen", "collectgarbage", "web",
  "sandbox", "coroutine", "string", "arg", NULL
};

/*
** The only functions a sandbox starts with.  They are taken from here, not
** from the current globals, so a script that replaces 'print' or 'tostring'
** cannot hand the sandbox something else.
*/
static const luaL_Reg nox_safe_funcs[] = {
  {"assert", luaB_assert}, {"error", luaB_error}, {"ipairs", luaB_ipairs},
  {"next", luaB_next}, {"pairs", luaB_pairs}, {"pcall", luaB_pcall},
  {"print", luaB_print}, {"select", luaB_select},
  {"tonumber", luaB_tonumber}, {"tostring", luaB_tostring},
  {"type", luaB_type}, {"xpcall", luaB_xpcall},
  {NULL, NULL}
};


/* Record the value at 'idx' as built-in and queue it to be walked. */
static void nox_mark (lua_State *L, int vals, int work, int idx) {
  int t = lua_type(L, idx);
  if (t != LUA_TTABLE && t != LUA_TFUNCTION && t != LUA_TUSERDATA &&
      t != LUA_TTHREAD)
    return;
  idx = lua_absindex(L, idx);
  lua_pushvalue(L, idx);
  if (lua_rawget(L, vals) != LUA_TNIL) {  /* already recorded */
    lua_pop(L, 1);
    return;
  }
  lua_pop(L, 1);
  lua_pushvalue(L, idx);
  lua_pushboolean(L, 1);
  lua_rawset(L, vals);
  lua_pushvalue(L, idx);  /* walk it later (threads have nothing to walk) */
  if (t != LUA_TTHREAD)
    lua_rawseti(L, work, cast(lua_Integer, lua_rawlen(L, work)) + 1);
  else
    lua_pop(L, 1);
}


/*
** Record every built-in: the names of all current globals, and every
** table, function, userdata and coroutine reachable from the globals, the
** registry and the string metatable (keys, values, metatables, user
** values and function upvalues, all the way down).  Called once the standard libraries are open.
** A second call only adds to the record.
*/
LUALIB_API void nox_snapshotbuiltins (lua_State *L) {
  int names, vals, work;
  lua_Integer next = 1;
  luaL_checkstack(L, 12, "not enough stack to record built-ins");
  if (lua_getfield(L, LUA_REGISTRYINDEX, NOX_BUILTIN_NAMES) != LUA_TTABLE) {
    lua_pop(L, 1);
    lua_newtable(L);
  }
  names = lua_gettop(L);
  if (lua_getfield(L, LUA_REGISTRYINDEX, NOX_BUILTIN_VALUES) != LUA_TTABLE) {
    lua_pop(L, 1);
    lua_newtable(L);
  }
  vals = lua_gettop(L);
  lua_newtable(L);  /* tables still to walk */
  work = lua_gettop(L);
  lua_pushglobaltable(L);
  lua_pushnil(L);
  while (lua_next(L, -2) != 0) {  /* every global name */
    if (lua_type(L, -2) == LUA_TSTRING) {
      lua_pushvalue(L, -2);
      lua_pushboolean(L, 1);
      lua_rawset(L, names);
    }
    lua_pop(L, 1);
  }
  nox_mark(L, vals, work, -1);
  lua_pop(L, 1);  /* global table */
  nox_mark(L, vals, work, LUA_REGISTRYINDEX);
  lua_pushliteral(L, "");
  if (lua_getmetatable(L, -1)) {
    nox_mark(L, vals, work, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  while (next <= cast(lua_Integer, lua_rawlen(L, work))) {
    int obj;
    lua_rawgeti(L, work, next++);
    obj = lua_gettop(L);
    if (lua_type(L, obj) != LUA_TFUNCTION && lua_getmetatable(L, obj)) {
      nox_mark(L, vals, work, -1);
      lua_pop(L, 1);
    }
    if (lua_type(L, obj) == LUA_TFUNCTION) {  /* hidden upvalues */
      int n;
      for (n = 1; lua_getupvalue(L, obj, n) != NULL; n++) {
        nox_mark(L, vals, work, -1);
        lua_pop(L, 1);
      }
    }
    else if (lua_type(L, obj) == LUA_TUSERDATA) {
      int n;
      for (n = 1; lua_getiuservalue(L, obj, n) != LUA_TNONE; n++) {
        nox_mark(L, vals, work, -1);
        lua_pop(L, 1);
      }
      lua_pop(L, 1);  /* the 'none' result */
    }
    else {  /* table */
      lua_pushnil(L);
      while (lua_next(L, obj) != 0) {
        nox_mark(L, vals, work, -2);  /* keys can be functions/tables too */
        nox_mark(L, vals, work, -1);
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);  /* obj */
  }
  lua_pop(L, 1);  /* work list */
  /* frozen copy of the original string metatable, used inside sandboxes */
  if (lua_getfield(L, LUA_REGISTRYINDEX, NOX_STRING_META) == LUA_TNIL) {
    lua_pop(L, 1);
    lua_pushliteral(L, "");
    if (lua_getmetatable(L, -1)) {
      int orig = lua_gettop(L);
      lua_newtable(L);
      lua_pushnil(L);
      while (lua_next(L, orig) != 0) {  /* copy every field */
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_rawset(L, -4);
      }
      if (lua_getfield(L, orig, "__index") == LUA_TTABLE) {  /* the methods */
        int lib = lua_gettop(L);
        lua_newtable(L);
        lua_pushnil(L);
        while (lua_next(L, lib) != 0) {
          lua_pushvalue(L, -2);
          lua_insert(L, -2);
          lua_rawset(L, -4);
        }
        lua_setfield(L, -3, "__index");
      }
      lua_pop(L, 1);  /* __index (original) */
      lua_setfield(L, LUA_REGISTRYINDEX, NOX_STRING_META);
      lua_pop(L, 1);  /* original metatable */
    }
    lua_pop(L, 1);  /* "" */
  }
  else
    lua_pop(L, 1);
  lua_setfield(L, LUA_REGISTRYINDEX, NOX_BUILTIN_VALUES);
  lua_setfield(L, LUA_REGISTRYINDEX, NOX_BUILTIN_NAMES);
}


/* Raise an error if the value at 'idx' may not enter a sandbox. */
static void nox_checkimportable (lua_State *L, int idx) {
  int t = lua_type(L, idx);
  if (t == LUA_TUSERDATA || t == LUA_TTHREAD)
    luaL_error(L, "sandbox cannot import a %s", luaL_typename(L, idx));
  if (t == LUA_TFUNCTION && lua_iscfunction(L, idx))
    luaL_error(L, "sandbox cannot import a built-in function");
  if (t != LUA_TTABLE && t != LUA_TFUNCTION)
    return;  /* numbers, strings, booleans, json.null: always fine */
  idx = lua_absindex(L, idx);
  if (lua_getfield(L, LUA_REGISTRYINDEX, NOX_BUILTIN_VALUES) != LUA_TTABLE)
    luaL_error(L, "sandbox import is unavailable: the built-in record is "
                  "missing (open the libraries with luaL_openlibs)");
  lua_pushvalue(L, idx);
  if (lua_rawget(L, -2) != LUA_TNIL)
    luaL_error(L, "sandbox cannot import a built-in library value");
  lua_pop(L, 2);
}


static int nox_deniedname (lua_State *L, const char *name) {
  int i, denied;
  for (i = 0; nox_denied_names[i] != NULL; i++)
    if (strcmp(name, nox_denied_names[i]) == 0)
      return 1;
  if (lua_getfield(L, LUA_REGISTRYINDEX, NOX_BUILTIN_NAMES) != LUA_TTABLE)
    luaL_error(L, "sandbox import is unavailable: the built-in record is "
                  "missing (open the libraries with luaL_openlibs)");
  lua_getfield(L, -1, name);
  denied = lua_toboolean(L, -1);
  lua_pop(L, 2);
  return denied;
}


/*
** Copy a value into a sandbox.  Tables are copied recursively (and without
** metatables), so writes to an imported table cannot change the original.
** 'seen' keeps cyclic tables and shared sub-tables intact.
*/
static void sandbox_copy (lua_State *L, int from, int seen, int depth) {
  luaL_checkstack(L, 8, "sandbox import: table too large");
  from = lua_absindex(L, from);
  seen = lua_absindex(L, seen);
  nox_checkimportable(L, from);
  if (lua_type(L, from) != LUA_TTABLE) {
    lua_pushvalue(L, from);
    return;
  }
  lua_pushvalue(L, from);
  if (lua_rawget(L, seen) != LUA_TNIL)  /* already copied? */
    return;
  lua_pop(L, 1);
  if (depth >= NOX_IMPORT_MAXDEPTH)
    luaL_error(L, "sandbox cannot import tables nested more than %d levels",
               NOX_IMPORT_MAXDEPTH);
  lua_newtable(L);  /* copy */
  lua_pushvalue(L, from);
  lua_pushvalue(L, -2);
  lua_rawset(L, seen);  /* seen[from] = copy */
  lua_pushnil(L);  /* first key */
  while (lua_next(L, from) != 0) {
    /* stack: ..., copy, key, value */
    sandbox_copy(L, -2, seen, depth + 1);  /* copied key */
    sandbox_copy(L, -2, seen, depth + 1);  /* copied value */
    lua_rawset(L, -5);  /* copy[copied key] = copied value */
    lua_pop(L, 1);  /* remove original value; keep key */
  }
}


/* import(kind, "name") is available only inside a sandbox. */
static int luaB_sandbox_import (lua_State *L) {
  size_t namelen;
  const char *kind = luaL_checkstring(L, 1);
  const char *name = luaL_checklstring(L, 2, &namelen);
  int source;
  if (strlen(name) != namelen)
    return luaL_error(L, "sandbox import names cannot contain '\\0'");
  if (strcmp(kind, "variable") != 0 && strcmp(kind, "func") != 0 &&
      strcmp(kind, "datatype") != 0 && strcmp(kind, "tabletype") != 0 &&
      strcmp(kind, "value") != 0 && strcmp(kind, "module") != 0)
    return luaL_error(L, "unknown sandbox import kind '%s'", kind);
  if (nox_deniedname(L, name))
    return luaL_error(L, "sandbox cannot import built-in '%s'", name);
  lua_settop(L, 2);
  lua_pushvalue(L, 2);
  lua_rawget(L, lua_upvalueindex(1));  /* main global, without metamethods */
  if (lua_isnil(L, -1))
    return luaL_error(L, "cannot import undefined global '%s'", name);
  source = lua_gettop(L);
  lua_newtable(L);  /* table of copies, for cyclic tables */
  sandbox_copy(L, source, -1, 0);
  lua_setfield(L, lua_upvalueindex(2), name);
  return 0;
}


/*
** sandbox[[...]] runs the string in a new global environment.  Its only
** built-ins are the harmless functions in 'nox_safe_funcs'; no file, OS,
** package, debug, load, metatable, or raw-table API is present.  Values from
** the caller's globals enter only through 'import'.
*/
/*
** Strings, numbers, nil, booleans, functions, light userdata and coroutines
** share one metatable per type with ALL code, including the sandbox.  A main
** script that adds e.g. 'string.shell = os.execute' would hand that to the
** sandbox without any import.  So while a sandbox runs, strings get a frozen
** copy of the original string metatable (recorded at startup) and the other
** types get none; the previous ones are put back afterwards, even on error.
*/
static void nox_pushtypesample (lua_State *L, int i) {
  switch (i) {
    case 0: lua_pushnil(L); break;
    case 1: lua_pushboolean(L, 0); break;
    case 2: lua_pushlightuserdata(L, NULL); break;
    case 3: lua_pushinteger(L, 0); break;
    case 4: lua_pushliteral(L, ""); break;
    case 5: lua_pushcfunction(L, luaB_type); break;
    default: lua_pushthread(L); break;
  }
}

#define NOX_NTYPES  7

/* Push a table with the current type metatables and install safe ones. */
static void nox_swaptypemetas (lua_State *L) {
  int i, saved;
  lua_createtable(L, NOX_NTYPES, 0);
  saved = lua_gettop(L);
  for (i = 0; i < NOX_NTYPES; i++) {
    nox_pushtypesample(L, i);
    if (lua_getmetatable(L, -1)) {
      lua_rawseti(L, saved, i + 1);
    }
    if (i == 4)  /* strings: the frozen original (none if not recorded) */
      lua_getfield(L, LUA_REGISTRYINDEX, NOX_STRING_META);
    else
      lua_pushnil(L);
    if (!lua_istable(L, -1)) {
      lua_pop(L, 1);
      lua_pushnil(L);
    }
    lua_setmetatable(L, -2);
    lua_pop(L, 1);  /* sample */
  }
}

/* Put back the type metatables saved in the table at 'saved'. */
static void nox_restoretypemetas (lua_State *L, int saved) {
  int i;
  for (i = 0; i < NOX_NTYPES; i++) {
    nox_pushtypesample(L, i);
    lua_rawgeti(L, saved, i + 1);  /* nil if there was none */
    lua_setmetatable(L, -2);
    lua_pop(L, 1);
  }
}


static int luaB_sandbox (lua_State *L) {
  size_t size;
  const char *code = luaL_checklstring(L, 1, &size);
  const luaL_Reg *f;
  int env;
  int status;
  lua_settop(L, 1);  /* sandbox accepts exactly one source-string argument */
  lua_newtable(L);
  env = lua_gettop(L);
  for (f = nox_safe_funcs; f->name != NULL; f++) {
    lua_pushcfunction(L, f->func);
    lua_setfield(L, env, f->name);
  }
  lua_pushliteral(L, "variable"); lua_setfield(L, env, "variable");
  lua_pushliteral(L, "func"); lua_setfield(L, env, "func");
  lua_pushliteral(L, "datatype"); lua_setfield(L, env, "datatype");
  lua_pushliteral(L, "tabletype"); lua_setfield(L, env, "tabletype");
  lua_pushliteral(L, "value"); lua_setfield(L, env, "value");
  lua_pushliteral(L, "module"); lua_setfield(L, env, "module");
  lua_pushglobaltable(L);
  lua_pushvalue(L, env);
  lua_pushcclosure(L, luaB_sandbox_import, 2);
  lua_setfield(L, env, "import");
  status = luaL_loadbufferx(L, code, size, "=sandbox", "t");
  if (status != LUA_OK)
    return lua_error(L);
  lua_pushvalue(L, env);
  lua_setupvalue(L, -2, 1);  /* give the chunk its private _ENV */
  luaL_checkstack(L, 4, "sandbox");
  nox_swaptypemetas(L);  /* stack: source, env, chunk, saved */
  lua_insert(L, env + 1);  /* stack: source, env, saved, chunk */
  status = lua_pcall(L, 0, LUA_MULTRET, 0);
  nox_restoretypemetas(L, env + 1);  /* always, even after an error */
  if (status != LUA_OK)
    return lua_error(L);
  lua_remove(L, env + 1);  /* saved metatables */
  lua_remove(L, env);  /* private environment */
  lua_remove(L, 1);  /* source string */
  return lua_gettop(L);
}


#if defined(_WIN32)

#define WEB_MAX_RESPONSE  (16 * 1024 * 1024)
#define WEB_REQUEST_TYPE  "nox.web.request"

/*
** Everything a request allocates lives in this userdata.  If any Lua error
** (bad option, out of memory) interrupts web.fetch, its __gc still frees
** the strings and closes the WinHTTP handles, so nothing leaks.
*/
typedef struct WebRequest {
  HINTERNET session, connection, request;
  wchar_t *url, *method, *host, *target, *headers;
} WebRequest;


static void web_cleanup (WebRequest *w) {
  if (w->request) WinHttpCloseHandle(w->request);
  if (w->connection) WinHttpCloseHandle(w->connection);
  if (w->session) WinHttpCloseHandle(w->session);
  free(w->url); free(w->method); free(w->host); free(w->target);
  free(w->headers);
  memset(w, 0, sizeof(*w));
}


static int web_gc (lua_State *L) {
  web_cleanup(cast(WebRequest *, luaL_checkudata(L, 1, WEB_REQUEST_TYPE)));
  return 0;
}


/* Convert a UTF-8 Lua string to the UTF-16 form required by WinHTTP. */
static wchar_t *web_towide (const char *s, size_t len) {
  int n;
  wchar_t *result;
  if (len == 0)
    return cast(wchar_t *, calloc(1, sizeof(wchar_t)));
  if (len > INT_MAX)
    return NULL;
  n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, (int)len,
                          NULL, 0);
  if (n == 0)
    return NULL;
  result = cast(wchar_t *, malloc(cast_sizet(n + 1) * sizeof(wchar_t)));
  if (result == NULL)
    return NULL;
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, (int)len, result, n);
  result[n] = L'\0';
  return result;
}


static wchar_t *web_wcopy (const wchar_t *s, DWORD len) {
  wchar_t *result = cast(wchar_t *, malloc(cast_sizet(len + 1) *
                                            sizeof(wchar_t)));
  if (result == NULL)
    return NULL;
  if (len != 0)
    memcpy(result, s, cast_sizet(len) * sizeof(wchar_t));
  result[len] = L'\0';
  return result;
}


/* WinHttpCrackUrl keeps the query string in 'extra', separate from the path. */
static wchar_t *web_wjoin (const wchar_t *path, DWORD pathlen,
                           const wchar_t *extra, DWORD extralen) {
  DWORD length = pathlen + extralen;
  wchar_t *result;
  if (pathlen == 0) {  /* "https://host?x=1" still needs a "/" path */
    path = L"/";
    pathlen = 1;
    length++;
  }
  result = cast(wchar_t *, malloc(cast_sizet(length + 1) * sizeof(wchar_t)));
  if (result == NULL)
    return NULL;
  memcpy(result, path, cast_sizet(pathlen) * sizeof(wchar_t));
  if (extralen != 0)
    memcpy(result + pathlen, extra, cast_sizet(extralen) * sizeof(wchar_t));
  result[length] = L'\0';
  return result;
}


/*
** Header names must be HTTP "token" characters; values may not contain
** line breaks or NULs (which could smuggle in extra header lines).
*/
static int web_badheaderchar (const char *s, size_t len, int isname) {
  size_t i;
  for (i = 0; i < len; i++) {
    unsigned char c = cast_uchar(s[i]);
    if (isname) {
      if (c == '\0' || (!isalnum(c) && strchr("!#$%&'*+-.^_`|~", c) == NULL))
        return 1;
    }
    else if (c == '\r' || c == '\n' || c == '\0')
      return 1;
  }
  return 0;
}


/* Headers that carry a login; they must not follow a redirect elsewhere. */
static int web_sensitiveheader (const char *name) {
  static const char *const sensitive[] = {
    "authorization", "proxy-authorization", "cookie", NULL
  };
  int i;
  for (i = 0; sensitive[i] != NULL; i++) {
    const char *a = name, *b = sensitive[i];
    while (*a && *b && tolower(cast_uchar(*a)) == *b) { a++; b++; }
    if (*a == '\0' && *b == '\0')
      return 1;
  }
  return 0;
}


/*
** Check options.headers and leave all of them on the stack as one string
** "Name: value\r\n..."; nil if there are none.  Runs before anything is
** allocated, so a bad header just raises a normal error.
*/
static void web_collectheaders (lua_State *L, int options, int *sensitive) {
  int headers, acc, count = 0;
  lua_getfield(L, options, "headers");
  if (lua_isnil(L, -1))
    return;  /* leave nil */
  if (!lua_istable(L, -1))
    luaL_error(L, "web.fetch: options.headers must be a table");
  headers = lua_gettop(L);
  lua_pushliteral(L, "");
  acc = lua_gettop(L);
  lua_pushnil(L);
  while (lua_next(L, headers) != 0) {
    size_t keylen, vallen;
    const char *key, *value;
    if (lua_type(L, -2) != LUA_TSTRING)  /* never convert a lua_next key */
      luaL_error(L, "web.fetch: header names must be strings");
    if (lua_type(L, -1) != LUA_TSTRING && lua_type(L, -1) != LUA_TNUMBER)
      luaL_error(L, "web.fetch: header '%s' must be a string",
                 lua_tostring(L, -2));
    key = lua_tolstring(L, -2, &keylen);
    value = lua_tolstring(L, -1, &vallen);
    if (keylen == 0 || web_badheaderchar(key, keylen, 1))
      luaL_error(L, "web.fetch: invalid header name '%s'", key);
    if (web_badheaderchar(value, vallen, 0))
      luaL_error(L, "web.fetch: header '%s' contains a line break", key);
    if (web_sensitiveheader(key))
      *sensitive = 1;
    lua_pushvalue(L, acc);
    lua_pushfstring(L, "%s: %s\r\n", key, value);
    lua_concat(L, 2);
    lua_replace(L, acc);
    lua_pop(L, 1);  /* value; keep key for lua_next */
    count++;
  }
  lua_remove(L, headers);  /* collected text is now on top */
  if (count == 0) {
    lua_pop(L, 1);
    lua_pushnil(L);
  }
}


static int web_fail (lua_State *L, WebRequest *w, const char *what) {
  DWORD code = GetLastError();  /* read before cleanup can change it */
  const char *reason;
  switch (code) {
    case ERROR_WINHTTP_TIMEOUT: reason = "timed out"; break;
    case ERROR_WINHTTP_NAME_NOT_RESOLVED: reason = "could not find host"; break;
    case ERROR_WINHTTP_CANNOT_CONNECT: reason = "could not connect"; break;
    case ERROR_WINHTTP_CONNECTION_ERROR: reason = "connection was reset"; break;
    case ERROR_WINHTTP_SECURE_FAILURE: reason = "HTTPS certificate problem"; break;
    case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:
    case ERROR_WINHTTP_INVALID_URL: {
      web_cleanup(w);
      lua_pushnil(L);
      lua_pushliteral(L, "web.fetch: invalid URL (it must start with "
                         "http:// or https://)");
      return 2;
    }
    case ERROR_WINHTTP_REDIRECT_FAILED: reason = "redirect failed"; break;
    case ERROR_NOT_ENOUGH_MEMORY: reason = "out of memory"; break;
    default: reason = NULL; break;
  }
  web_cleanup(w);
  lua_pushnil(L);
  if (reason)
    lua_pushfstring(L, "web.fetch: %s (%s)", reason, what);
  else
    lua_pushfstring(L, "web.fetch: %s failed (Windows error %d)", what,
                    cast_int(code));
  return 2;
}


static int web_failmsg (lua_State *L, WebRequest *w, const char *msg) {
  web_cleanup(w);
  lua_pushnil(L);
  lua_pushstring(L, msg);
  return 2;
}


/*
** web.fetch(url [, options]) -> body, status | nil, error
** options = {method = "POST", body = "...", headers = { ["X-Key"] = "..." },
**            timeout = seconds, redirects = true/false}
*/
static int luaB_webfetch (lua_State *L) {
  size_t urllen, methodlen = 3, bodylen = 0, headerlen = 0, i;
  const char *url = luaL_checklstring(L, 1, &urllen);
  const char *method = "GET";
  const char *body = NULL;
  const char *headers = NULL;
  int timeout_ms = 0;
  int sensitive = 0;  /* sends Authorization/Cookie headers? */
  int redirects = -1;  /* -1: decide automatically */
  WebRequest *w;
  URL_COMPONENTS parts;
  DWORD status = 0, statuslen = sizeof(status), available, received;
  luaL_Buffer response;

  /* 1. validate every option first (errors here allocate nothing) */
  lua_settop(L, 2);
  if (!lua_isnil(L, 2)) {
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "method");
    if (!lua_isnil(L, -1)) {
      if (lua_type(L, -1) != LUA_TSTRING)
        return luaL_error(L, "web.fetch: options.method must be a string");
      method = lua_tolstring(L, -1, &methodlen);
      if (methodlen == 0)
        return luaL_error(L, "web.fetch: options.method is empty");
      for (i = 0; i < methodlen; i++)
        if (!isalpha(cast_uchar(method[i])))
          return luaL_error(L, "web.fetch: invalid method '%s'", method);
    }
    lua_getfield(L, 2, "body");
    if (!lua_isnil(L, -1)) {
      if (lua_type(L, -1) != LUA_TSTRING)
        return luaL_error(L, "web.fetch: options.body must be a string");
      body = lua_tolstring(L, -1, &bodylen);
      if (bodylen > 0xFFFFFFFFu)
        return luaL_error(L, "web.fetch: options.body is too large");
    }
    lua_getfield(L, 2, "timeout");
    if (!lua_isnil(L, -1)) {
      lua_Number seconds;
      if (lua_type(L, -1) != LUA_TNUMBER)
        return luaL_error(L, "web.fetch: options.timeout must be a number "
                             "of seconds");
      seconds = lua_tonumber(L, -1);
      if (!(seconds > 0))
        return luaL_error(L, "web.fetch: options.timeout must be above 0");
      timeout_ms = (seconds >= INT_MAX / 1000.0) ? INT_MAX :
                   cast_int(seconds * 1000.0);
      if (timeout_ms < 1) timeout_ms = 1;
    }
    lua_getfield(L, 2, "redirects");
    if (!lua_isnil(L, -1)) {
      if (!lua_isboolean(L, -1))
        return luaL_error(L, "web.fetch: options.redirects must be true "
                             "or false");
      redirects = lua_toboolean(L, -1);
    }
    web_collectheaders(L, 2, &sensitive);
    if (!lua_isnil(L, -1))
      headers = lua_tolstring(L, -1, &headerlen);
  }
  /* the strings above stay on the stack, so they stay alive */

  /* 2. from here on, the userdata owns everything we allocate */
  w = cast(WebRequest *, lua_newuserdatauv(L, sizeof(WebRequest), 0));
  memset(w, 0, sizeof(*w));
  if (luaL_newmetatable(L, WEB_REQUEST_TYPE)) {
    lua_pushcfunction(L, web_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);

  w->url = web_towide(url, urllen);
  if (w->url == NULL || urllen == 0)
    return web_failmsg(L, w, "web.fetch: URL is empty or not valid UTF-8");
  w->method = web_towide(method, methodlen);
  if (w->method == NULL) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return web_fail(L, w, "preparing request");
  }
  if (headers) {
    w->headers = web_towide(headers, headerlen);
    if (w->headers == NULL)
      return web_failmsg(L, w, "web.fetch: headers are not valid UTF-8");
  }
  memset(&parts, 0, sizeof(parts));
  parts.dwStructSize = sizeof(parts);
  /* Ask WinHTTP to point these components into 'url' instead of copying. */
  parts.dwHostNameLength = (DWORD)-1;
  parts.dwUrlPathLength = (DWORD)-1;
  parts.dwExtraInfoLength = (DWORD)-1;
  parts.dwUserNameLength = (DWORD)-1;
  parts.dwPasswordLength = (DWORD)-1;
  if (!WinHttpCrackUrl(w->url, 0, 0, &parts))
    return web_fail(L, w, "reading URL");
  if (parts.nScheme != INTERNET_SCHEME_HTTP &&
      parts.nScheme != INTERNET_SCHEME_HTTPS)
    return web_failmsg(L, w,
             "web.fetch: URL must start with http:// or https://");
  if (parts.dwHostNameLength == 0)
    return web_failmsg(L, w, "web.fetch: URL has no host name");
  if (parts.dwUserNameLength != 0 || parts.dwPasswordLength != 0)
    return web_failmsg(L, w, "web.fetch: put logins in an Authorization "
                             "header, not in the URL (user:pass@...)");
  w->host = web_wcopy(parts.lpszHostName, parts.dwHostNameLength);
  w->target = web_wjoin(parts.lpszUrlPath, parts.dwUrlPathLength,
                        parts.lpszExtraInfo, parts.dwExtraInfoLength);
  if (w->host == NULL || w->target == NULL) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return web_fail(L, w, "preparing request");
  }

  /* 3. talk to the server */
  w->session = WinHttpOpen(L"Nox/5.5.1",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (w->session == NULL)
    return web_fail(L, w, "starting WinHTTP");
  if (timeout_ms > 0)
    WinHttpSetTimeouts(w->session, timeout_ms, timeout_ms, timeout_ms,
                       timeout_ms);
  w->connection = WinHttpConnect(w->session, w->host, parts.nPort, 0);
  if (w->connection == NULL)
    return web_fail(L, w, "connecting");
  w->request = WinHttpOpenRequest(w->connection, w->method, w->target, NULL,
             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
             (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
  if (w->request == NULL)
    return web_fail(L, w, "opening request");
  /* By default WinHTTP follows redirects and re-sends added headers, which
     could hand a login to another site.  So requests with login headers
     only follow redirects when the script asks for it. */
  if (redirects == 0 || (redirects < 0 && sensitive)) {
    DWORD feature = WINHTTP_DISABLE_REDIRECTS;
    if (!WinHttpSetOption(w->request, WINHTTP_OPTION_DISABLE_FEATURE,
                          &feature, sizeof(feature)))
      return web_fail(L, w, "opening request");
  }
  if (w->headers && !WinHttpAddRequestHeaders(w->request, w->headers,
                 (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
    return web_fail(L, w, "adding headers");
  if (!WinHttpSendRequest(w->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          cast(LPVOID, body), cast(DWORD, bodylen),
                          cast(DWORD, bodylen), 0))
    return web_fail(L, w, "sending request");
  if (!WinHttpReceiveResponse(w->request, NULL))
    return web_fail(L, w, "waiting for response");
  if (!WinHttpQueryHeaders(w->request,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &statuslen,
                      WINHTTP_NO_HEADER_INDEX))
    status = 0;

  /* 4. read the body straight into a Lua buffer */
  luaL_buffinit(L, &response);
  for (;;) {
    char *chunk;
    available = 0;
    if (!WinHttpQueryDataAvailable(w->request, &available))
      return web_fail(L, w, "reading response");
    if (available == 0)
      break;
    if (available > WEB_MAX_RESPONSE ||
        luaL_bufflen(&response) + available > WEB_MAX_RESPONSE)
      return web_failmsg(L, w, "web.fetch: response is larger than 16 MB");
    chunk = luaL_prepbuffsize(&response, available);
    received = 0;
    if (!WinHttpReadData(w->request, chunk, available, &received))
      return web_fail(L, w, "reading response");
    luaL_addsize(&response, received);
  }
  luaL_pushresult(&response);
  web_cleanup(w);  /* close now instead of waiting for the GC */
  lua_pushinteger(L, cast(lua_Integer, status));
  return 2;
}

#else

static int luaB_webfetch (lua_State *L) {
  UNUSED(L);
  lua_pushnil(L);
  lua_pushliteral(L, "web.fetch is currently available only in Windows builds");
  return 2;
}

#endif


static const luaL_Reg web_funcs[] = {
  {"fetch", luaB_webfetch},
  {NULL, NULL}
};


static const luaL_Reg base_funcs[] = {
  {"assert", luaB_assert},
  {"collectgarbage", luaB_collectgarbage},
  {"dofile", luaB_dofile},
  {"error", luaB_error},
  {"getmetatable", luaB_getmetatable},
  {"ipairs", luaB_ipairs},
  {"loadfile", luaB_loadfile},
  {"load", luaB_load},
  {"next", luaB_next},
  {"pairs", luaB_pairs},
  {"pcall", luaB_pcall},
  {"print", luaB_print},
  {"warn", luaB_warn},
  {"rawequal", luaB_rawequal},
  {"rawlen", luaB_rawlen},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"select", luaB_select},
  {"sandbox", luaB_sandbox},
  {"setmetatable", luaB_setmetatable},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  {"xpcall", luaB_xpcall},
  /* placeholders */
  {LUA_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


LUAMOD_API int luaopen_base (lua_State *L) {
  /* open lib into global table */
  lua_pushglobaltable(L);
  luaL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, LUA_GNAME);
  /* set global _VERSION */
  lua_pushliteral(L, LUA_VERSION);
  lua_setfield(L, -2, "_VERSION");
  luaL_newlib(L, web_funcs);
  lua_setglobal(L, "web");
  return 1;
}
