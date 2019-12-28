/* Copyright (C) 2013 Papavasileiou Dimitris
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lualib.h>
#include <lauxlib.h>
#include <getopt.h>
#include <unistd.h>

#ifdef LUAP_LUAJIT
#include <luajit.h>
#endif

#include "prompt.h"

#if !defined(LUA_INIT)
#define LUA_INIT "LUA_INIT"
#endif

#if LUA_VERSION_NUM == 502
#define LUA_INITVERSION  \
        LUA_INIT "_" LUA_VERSION_MAJOR "_" LUA_VERSION_MINOR
#endif

#if LUA_VERSION_NUM == 501
#define COPYRIGHT LUA_VERSION " " LUA_COPYRIGHT
#else
#define COPYRIGHT LUA_COPYRIGHT
#endif

static int interactive = 0, colorize = 1;

static void greet()
{
  fprintf(stdout, "%s" "luap " LUAP_VERSION " -- " COPYRIGHT "%s\n"
          "(c) 2012-2015 Dimitris Papavasiliou, Boris Nagaev\n"
          "(c) 2019-2020 Tomas Pollak (forkhq.com)\n\n"
          "%s" "Welcome back, master." "%s",
          colorize ? "\033[1m" : "",
          colorize ? "\033[0m" : "",
          colorize ? "\033[1m" : "",
          colorize ? "\033[0m" : "");
}

static void dostring (lua_State *L, const char *string, const char *name)
{
  if (luaL_loadbuffer(L, string, strlen(string), name)) {
    fprintf(stderr,
            "%s%s%s\n",
            colorize ? "\033[0;31m" : "",
            lua_tostring (L, -1),
            colorize ? "\033[0m" : "");
    exit (EXIT_FAILURE);
  } else if (luap_call (L, 0)) {
    exit (EXIT_FAILURE);
  }
}

static void dofile (lua_State *L, const char *name)
{
  if (luaL_loadfile(L, name)) {
    fprintf(stderr,
            "%s%s%s\n",
            colorize ? "\033[0;31m" : "",
            lua_tostring (L, -1),
            colorize ? "\033[0m" : "");
    exit (EXIT_FAILURE);
  } else if (luap_call (L, 0)) {
    exit (EXIT_FAILURE);
  }
}

#ifdef LUAP_LUAJIT
static void l_message(const char *pname, const char *msg)
{
  if (pname) { fputs(pname, stderr); fputc(':', stderr); fputc(' ', stderr); }
  fputs(msg, stderr); fputc('\n', stderr);
  fflush(stderr);
}

static int report(lua_State *L, int status)
{
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message("luap", msg);
    lua_pop(L, 1);
  }
  return status;
}

/* Load add-on module. */
static int loadjitmodule(lua_State *L)
{
  lua_getglobal(L, "require");
  lua_pushliteral(L, "jit.");
  lua_pushvalue(L, -3);
  lua_concat(L, 2);

  if (lua_pcall(L, 1, 1, 0)) {
    const char *msg = lua_tostring(L, -1);
    if (msg && !strncmp(msg, "module ", 7))
      goto nomodule;

    return report(L, 1);
  }

  lua_getfield(L, -1, "start");
  if (lua_isnil(L, -1)) {
  nomodule:
    l_message("luap",
	  "unknown luaJIT command or jit.* modules not installed");
    return 1;
  }

  lua_remove(L, -2);  /* Drop module table. */
  return 0;
}

/* Run command with options. */
static int runcmdopt(lua_State *L, const char *opt)
{
  int narg = 0;
  if (opt && *opt) {
    for (;;) {  /* Split arguments. */
      const char *p = strchr(opt, ',');
      narg++;
      if (!p) break;
      if (p == opt)
	      lua_pushnil(L);
      else
	      lua_pushlstring(L, opt, (size_t)(p - opt));
      opt = p + 1;
    }

    if (*opt)
      lua_pushstring(L, opt);
    else
      lua_pushnil(L);
  }

  return report(L, lua_pcall(L, narg, 0, 0));
}

/* JIT engine control command: try jit library first or load add-on module. */
static int dojitcmd(lua_State *L, const char *cmd)
{
  const char *opt = strchr(cmd, '=');
  lua_pushlstring(L, cmd, opt ? (size_t)(opt - cmd) : strlen(cmd));
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, "jit");  /* Get jit.* module table. */
  lua_remove(L, -2);
  lua_pushvalue(L, -2);
  lua_gettable(L, -2);  /* Lookup library function. */

  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);  /* Drop non-function and jit.* table, keep module name. */
    if (loadjitmodule(L))
      return 1;
  } else {
    lua_remove(L, -2);  /* Drop jit.* table. */
  }

  lua_remove(L, -2);  /* Drop module name. */
  return runcmdopt(L, opt ? opt+1 : opt);
}

/* Optimization flags. */
static int dojitopt(lua_State *L, const char *opt)
{
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, "jit.opt");  /* Get jit.opt.* module table. */
  lua_remove(L, -2);
  lua_getfield(L, -1, "start");
  lua_remove(L, -2);
  return runcmdopt(L, opt);
}
#endif

int main(int argc, char **argv)
{
  lua_State *L;
  char option;
  int done = 0;

  L = luaL_newstate();
  luap_setname (L, "lua");

  /* Open the standard libraries. */
  lua_gc(L, LUA_GCSTOP, 0);
  luaL_openlibs(L);
  lua_gc(L, LUA_GCRESTART, 0);

  if (L == NULL) {
    fprintf(stderr,
            "%s: Could not create Lua state (out of memory).\n",
            argv[0]);

    return EXIT_FAILURE;
  }

  if (!isatty (STDOUT_FILENO) || !isatty (STDERR_FILENO)) {
    colorize = 0;
  }

  luap_setcolor (L, colorize);

  /* Take care of the LUA_INIT environment variable. */
  {
    const char *name, *init;

#if LUA_VERSION_NUM == 502
    name = "=" LUA_INITVERSION;
    init = getenv(name + 1);

    if (init == NULL) {
#endif
      name = "=" LUA_INIT;
      init = getenv(name + 1);

#if LUA_VERSION_NUM == 502
    }
#endif

    if (init) {
      if (init[0] == '@') {
        dofile(L, init + 1);
      } else {
        dostring(L, init, name);
      }
    }
  }

  /* Parse the command line. */

#ifdef LUAP_LUAJIT
  const char* OPTS = "ivphe:l:j:O:";
#else
  const char* OPTS = "ivphe:l:";
#endif

  while ((option = getopt (argc, argv, OPTS)) != -1) {
    if (option == 'i') {
      interactive = 1;
    } else if (option == 'v') {
      greet();
      done = 1;
    } else if (option == 'p') {
      colorize = 0;
      luap_setcolor (L, colorize);
    } else if (option == 'e') {
      dostring (L, optarg, "=(command line)");
      done = 1;
    } else if (option == 'l') {
      lua_getglobal (L, "require");
      lua_pushstring (L, optarg);
      if (luap_call (L, 1)) {
        return EXIT_FAILURE;
      } else {
        lua_setglobal (L, optarg);
      }
#ifdef LUAP_LUAJIT
    /* LuaJIT extension */
    } else if (option == 'j') {
      if (dojitcmd(L, optarg)) {
        return EXIT_FAILURE;
      }
    } else if (option == 'O') {
      if (dojitopt(L, optarg)) {
        return EXIT_FAILURE;
      }
#endif
    } else if (option == 'h') {
      printf (
          "Usage: %s [OPTION...] [[SCRIPT] ARGS]\n\n"
          "Options:\n"
          "  -h       Display this help message\n"
          "  -e STMT  Execute string 'STMT'\n"
          "  -l NAME  Require library 'NAME'\n"
          "  -j cmd   Perform LuaJIT control command\n"
          "  -O[opt]  Control LuaJIT optimizations\n"
          "  -p       Force plain, uncolored output\n"
          "  -v       Print version information\n"
          "  -i       Enter interactive mode after executing SCRIPT\n",
          argv[0]);

      done = 1;
    } else {
      exit(1);
    }
  }

  if (argc > optind) {
    char *name;
    int i;

    if (!strcmp (argv[optind], "-")) {
      name = NULL;
    } else {
      name = argv[optind];
    }

    /* Collect all command line arguments into a table. */
    lua_createtable (L, argc - optind - 1, optind + 1);

    for (i = 0 ; i <= argc ; i += 1) {
      lua_pushstring (L, argv[i]);
      lua_rawseti (L, -2, i - optind);
    }

    lua_setglobal (L, "arg");

    /* Load the script and call it. */
    dofile (L, name);
    done = 1;
  }

  if (!done || interactive) {
    if (isatty (STDIN_FILENO)) {
      char *home;

      greet();
      fprintf (stdout, "\n");
      home = getenv("HOME");

      {
        char path[strlen(home) + sizeof("/.lua_history")];
        strcpy(path, home);
        strcat(path, "/.lua_history");
        luap_sethistory (L, path);
      }

      luap_setprompts (L, "> ", ">> ");
      luap_enter(L);
    } else {
      dofile (L, NULL);
    }
  }

  lua_close(L);
  return EXIT_SUCCESS;
}