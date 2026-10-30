/*
** JSON decoder for Nox
*/

#define ljson_c
#define LUA_LIB

#include "lprefix.h"


#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "llimits.h"


typedef struct JsonParser {
  lua_State *L;
  const char *start;
  const char *s;
  const char *end;
  int depth;
} JsonParser;


/* A stable value for JSON null, because Lua's nil cannot be stored in tables. */
static char json_null;


static void json_error (JsonParser *p) {
  luaL_error(p->L, "invalid JSON near byte %d",
             cast_int(p->s - p->start));
}


static void json_skipspace (JsonParser *p) {
  while (p->s < p->end && (*p->s == ' ' || *p->s == '\n' ||
         *p->s == '\r' || *p->s == '\t'))
    p->s++;
}


static int json_hex (JsonParser *p) {
  int result = 0;
  int i;
  for (i = 0; i < 4; i++) {
    int c;
    if (p->s == p->end)
      json_error(p);
    c = cast_uchar(*p->s++);
    result *= 16;
    if (c >= '0' && c <= '9') result += c - '0';
    else if (c >= 'a' && c <= 'f') result += c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') result += c - 'A' + 10;
    else json_error(p);
  }
  return result;
}


static void json_addutf8 (luaL_Buffer *b, unsigned int code) {
  if (code <= 0x7f)
    luaL_addchar(b, cast_char(code));
  else if (code <= 0x7ff) {
    luaL_addchar(b, cast_char(0xc0 | (code >> 6)));
    luaL_addchar(b, cast_char(0x80 | (code & 0x3f)));
  }
  else if (code <= 0xffff) {
    luaL_addchar(b, cast_char(0xe0 | (code >> 12)));
    luaL_addchar(b, cast_char(0x80 | ((code >> 6) & 0x3f)));
    luaL_addchar(b, cast_char(0x80 | (code & 0x3f)));
  }
  else {
    luaL_addchar(b, cast_char(0xf0 | (code >> 18)));
    luaL_addchar(b, cast_char(0x80 | ((code >> 12) & 0x3f)));
    luaL_addchar(b, cast_char(0x80 | ((code >> 6) & 0x3f)));
    luaL_addchar(b, cast_char(0x80 | (code & 0x3f)));
  }
}


/* Read a JSON string and leave its decoded Lua string on the stack. */
static void json_string (JsonParser *p) {
  luaL_Buffer b;
  if (p->s == p->end || *p->s++ != '"')
    json_error(p);
  luaL_buffinit(p->L, &b);
  while (p->s < p->end) {
    unsigned int c = cast_uchar(*p->s++);
    if (c == '"') {
      luaL_pushresult(&b);
      return;
    }
    if (c < 0x20)
      json_error(p);
    if (c != '\\') {
      luaL_addchar(&b, cast_char(c));
      continue;
    }
    if (p->s == p->end)
      json_error(p);
    c = cast_uchar(*p->s++);
    switch (c) {
      case '"': luaL_addchar(&b, '"'); break;
      case '\\': luaL_addchar(&b, '\\'); break;
      case '/': luaL_addchar(&b, '/'); break;
      case 'b': luaL_addchar(&b, '\b'); break;
      case 'f': luaL_addchar(&b, '\f'); break;
      case 'n': luaL_addchar(&b, '\n'); break;
      case 'r': luaL_addchar(&b, '\r'); break;
      case 't': luaL_addchar(&b, '\t'); break;
      case 'u': {
        unsigned int code = cast_uint(json_hex(p));
        /* A half of a surrogate pair on its own is legal JSON but has no
           UTF-8 form, so it becomes U+FFFD (the replacement character). */
        if (code >= 0xd800 && code <= 0xdbff) {  /* high surrogate */
          const char *save = p->s;
          unsigned int low = 0;
          if (p->end - p->s >= 6 && p->s[0] == '\\' && p->s[1] == 'u') {
            p->s += 2;
            low = cast_uint(json_hex(p));
          }
          if (low >= 0xdc00 && low <= 0xdfff)
            code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
          else {  /* not followed by a low surrogate */
            p->s = save;
            code = 0xfffd;
          }
        }
        else if (code >= 0xdc00 && code <= 0xdfff)  /* lone low surrogate */
          code = 0xfffd;
        json_addutf8(&b, code);
        break;
      }
      default: json_error(p);
    }
  }
  json_error(p);
}


static void json_value (JsonParser *p);


static void json_array (JsonParser *p) {
  int index = 1;
  int table;
  p->s++;  /* '[' */
  if (++p->depth > 200)
    luaL_error(p->L, "JSON nested deeper than 200 levels");
  lua_newtable(p->L);
  table = lua_gettop(p->L);
  json_skipspace(p);
  if (p->s < p->end && *p->s == ']') {
    p->s++;
    p->depth--;
    return;
  }
  for (;;) {
    json_value(p);
    lua_rawseti(p->L, table, index++);
    json_skipspace(p);
    if (p->s == p->end)
      json_error(p);
    if (*p->s == ']') {
      p->s++;
      p->depth--;
      return;
    }
    if (*p->s++ != ',')
      json_error(p);
    json_skipspace(p);
  }
}


static void json_object (JsonParser *p) {
  int table;
  p->s++;  /* '{' */
  if (++p->depth > 200)
    luaL_error(p->L, "JSON nested deeper than 200 levels");
  lua_newtable(p->L);
  table = lua_gettop(p->L);
  json_skipspace(p);
  if (p->s < p->end && *p->s == '}') {
    p->s++;
    p->depth--;
    return;
  }
  for (;;) {
    if (p->s == p->end || *p->s != '"')
      json_error(p);
    json_string(p);  /* key */
    json_skipspace(p);
    if (p->s == p->end || *p->s++ != ':')
      json_error(p);
    json_skipspace(p);
    json_value(p);
    lua_rawset(p->L, table);
    json_skipspace(p);
    if (p->s == p->end)
      json_error(p);
    if (*p->s == '}') {
      p->s++;
      p->depth--;
      return;
    }
    if (*p->s++ != ',')
      json_error(p);
    json_skipspace(p);
  }
}


static void json_number (JsonParser *p) {
  const char *start = p->s;
  const char *number;
  if (*p->s == '-') p->s++;
  if (p->s == p->end) json_error(p);
  if (*p->s == '0') p->s++;
  else {
    if (*p->s < '1' || *p->s > '9') json_error(p);
    do { p->s++; } while (p->s < p->end && *p->s >= '0' && *p->s <= '9');
  }
  if (p->s < p->end && *p->s == '.') {
    p->s++;
    if (p->s == p->end || *p->s < '0' || *p->s > '9') json_error(p);
    do { p->s++; } while (p->s < p->end && *p->s >= '0' && *p->s <= '9');
  }
  if (p->s < p->end && (*p->s == 'e' || *p->s == 'E')) {
    p->s++;
    if (p->s < p->end && (*p->s == '+' || *p->s == '-')) p->s++;
    if (p->s == p->end || *p->s < '0' || *p->s > '9') json_error(p);
    do { p->s++; } while (p->s < p->end && *p->s >= '0' && *p->s <= '9');
  }
  number = lua_pushlstring(p->L, start, cast_sizet(p->s - start));
  if (lua_stringtonumber(p->L, number) == 0)
    json_error(p);
  lua_remove(p->L, -2);  /* source string; leave number */
}


static void json_literal (JsonParser *p, const char *text, int len, int type) {
  if (p->end - p->s < len || memcmp(p->s, text, cast_sizet(len)) != 0)
    json_error(p);
  p->s += len;
  if (type < 0)
    lua_pushlightuserdata(p->L, &json_null);
  else
    lua_pushboolean(p->L, type);
}


static void json_value (JsonParser *p) {
  /* each nesting level keeps a table (and maybe a key) on the C stack */
  luaL_checkstack(p->L, 4, "JSON nested too deeply");
  json_skipspace(p);
  if (p->s == p->end) json_error(p);
  switch (*p->s) {
    case '"': json_string(p); return;
    case '{': json_object(p); return;
    case '[': json_array(p); return;
    case 't': json_literal(p, "true", 4, 1); return;
    case 'f': json_literal(p, "false", 5, 0); return;
    case 'n': json_literal(p, "null", 4, -1); return;
    default: json_number(p); return;
  }
}


static int luaB_jsondecode (lua_State *L) {
  size_t length;
  const char *text = luaL_checklstring(L, 1, &length);
  JsonParser p;
  p.L = L;
  p.start = text;
  p.s = text;
  p.end = text + length;
  p.depth = 0;
  json_value(&p);
  json_skipspace(&p);
  if (p.s != p.end)
    json_error(&p);
  return 1;
}


static const luaL_Reg json_funcs[] = {
  {"decode", luaB_jsondecode},
  {NULL, NULL}
};


LUAMOD_API int luaopen_json (lua_State *L) {
  luaL_newlib(L, json_funcs);
  lua_pushlightuserdata(L, &json_null);
  lua_setfield(L, -2, "null");
  return 1;
}
