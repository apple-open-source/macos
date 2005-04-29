// luna2.h -- C++ interface wrapper for Lua
// $Id: luna2.h,v 1.3 2003/11/23 01:42:50 wcvs Exp $
// This is part of Metakit, see http://www.equi4.com/metakit/

#include <assert.h>

extern "C" {
  #include <lua.h>
  #include <lauxlib.h>
}

class Lua {
  lua_State* _state;
  int _pos;
  int _res;
public:
  Lua (lua_State* L, int n_) : _state (L), _pos (n_), _res (0) {}

  bool more() const { return _pos < lua_gettop(_state); }
  void done() const { assert(!more()); }

  Lua& operator++ () { assert(more()); ++_pos; return *this; }
  int nextarg() { assert(more()); return ++_pos; }
  operator lua_State* () const { return _state; }
  int numresults() const { return _res; }

  // required args
  operator bool () const { return !lua_isnil(_state, _pos); }
  operator int () const { return luaL_check_int(_state, _pos); }
  operator long () const { return luaL_check_long(_state, _pos); }
  operator double () const { return luaL_check_number(_state, _pos); }
  operator char () const { return *luaL_check_string(_state, _pos); }
  operator const char* () const { return luaL_check_string(_state, _pos); }

  // optional args
  Lua& operator>> (bool& a) { if (more()) a = ++*this; return *this; }
  Lua& operator>> (int& a) { if (more()) a = ++*this; return *this; }
  Lua& operator>> (long& a) { if (more()) a = ++*this; return *this; }
  Lua& operator>> (double& a) { if (more()) a = ++*this; return *this; }
  Lua& operator>> (char& a) { if (more()) a = ++*this; return *this; }
  Lua& operator>> (const char*& a) { if (more()) a = ++*this; return *this; }

  // results
  Lua& operator<< (bool a) {
    if (a)
      lua_pushnumber(_state, 1);
    else
      lua_pushnil(_state);
    ++_res;
    return *this;
  }
  Lua& operator<< (int a) {
    lua_pushnumber(_state, a);
    ++_res;
    return *this;
  }
  Lua& operator<< (long a) {
    lua_pushnumber(_state, a);
    ++_res;
    return *this;
  }
  Lua& operator<< (double a) {
    lua_pushnumber(_state, a);
    ++_res;
    return *this;
  }
  Lua& operator<< (char a) {
    lua_pushlstring(_state, &a, 1);
    ++_res;
    return *this;
  }
  Lua& operator<< (const char* a) {
    lua_pushstring(_state, a);
    ++_res;
    return *this;
  }
  Lua& pushusertag(void* a, int t) {
    lua_pushusertag(_state, a, t);
    ++_res;
    return *this;
  }
};

// Inspired by the luna.h code of Lenny Palozzi - lenny.palozzi@home.net

template <class T> class Luna {
  /* constructs T objects */
  static int constructor(lua_State* L) {
    Lua lua (L, 1);
    (void) create(lua);
    return lua.numresults();
  }
  /* member function dispatcher */
  static int proxy(lua_State* L) {
    assert(lua_tag(L, 1) == otag);
    T* obj = static_cast<T*>(lua_touserdata(L,1));
    int i = static_cast<int>(lua_tonumber(L,-1)); 
    lua_pop(L, 1);
    Lua lua (L, 1);
    (obj->*(T::regTable[i].mfunc))(lua);
    return lua.numresults();
  }
  /* method call dispatcher */
  static int methodcall(lua_State* L) {
    assert(lua_tag(L, 1) == otag); // table or userdata
    assert(lua_istable(L, -1) && lua_tag(L, -1) == otag);
    assert(lua_isstring(L, 2));
    lua_pushvalue(L,2);
    lua_rawget(L, -2);
    return 1;
  }
  /* member access dispatcher */
  static int getmember(lua_State* L) {
    //assert(lua_isuserdata(L, 1) && lua_tag(L, 1) == otag);
    assert(lua_tag(L, 1) == otag); // table or userdata
    assert(lua_istable(L, -1) && lua_tag(L, -1) == otag);
    assert(lua_isstring(L, 2));
    lua_pushvalue(L,2);
    lua_rawget(L, -2);
    lua_pushvalue(L,1);
    lua_call(L, 1, 1);
    return 1;
  }
#if 0
  /* member modify dispatcher */
  static int setmember(lua_State* L) {
    //assert(lua_isuserdata(L, 1) && lua_tag(L, 1) == otag);
    assert(lua_tag(L, 1) == otag); // table or userdata
    assert(lua_istable(L, -1) && lua_tag(L, -1) == otag);
    assert(lua_isstring(L, 2));
    lua_pushvalue(L,2);
    lua_rawget(L, -2);
    lua_pushvalue(L, 1);
    lua_pushnumber(L, 1);
    lua_call(L, 2, 1);
    return 1;
  }
#endif
  /* releases objects */
  static int gc_obj(lua_State* L) {
    T* obj = static_cast<T*>(lua_touserdata(L, -1));
    delete obj;
    return 0;
  }
protected: 
  Luna(); /* hide default constructor */
public:
  static int otag;
  /* member function map */
  struct RegType { const char* name; void(T::*mfunc)(Lua&); };      
  /* register class T */
  static void Register(lua_State* L, int data =0) {
    lua_newtable(L);
    if (otag == 0) {
      otag = lua_newtag(L);
      	// TODO: there's a major mixup in here - the table should only
	// get the function tm, and the constructed userdata objects
	// should get the gc and gettable tm's, using a 2nd tag number
      lua_pushcfunction(L, &Luna<T>::constructor);
      lua_settagmethod(L, otag, "function");
      lua_pushcfunction(L, &Luna<T>::gc_obj);
      lua_settagmethod(L, otag, "gc");
      lua_pushvalue(L,-1);
      lua_pushcclosure(L, data ? &Luna<T>::getmember
	  		       : &Luna<T>::methodcall, 1);
      lua_settagmethod(L, otag, "gettable");
#if 0
      if (data > 1) {
	lua_pushvalue(L,-1);
	lua_pushcclosure(L, &Luna<T>::setmember, 1);
	lua_settagmethod(L, otag, "settable");
      }
#endif
    }
    lua_settag(L,otag);
    /* register the member functions */
    for (int i=0; T::regTable[i].name; i++) {
      lua_pushstring(L, T::regTable[i].name);
      lua_pushnumber(L, i);
      lua_pushcclosure(L, &Luna<T>::proxy, 1);
      lua_settable(L, -3);
    }
    lua_setglobal(L, T::className);
  }
  /* constructs T objects and returns them for use in C++ */
  static T* create(Lua& lua) {
    T* obj = new T(lua); /* new T */
    assert(otag != 0);
    lua.pushusertag(obj,otag);
    return obj; /* also leaves userdata object on Lua stack */
  }
  /* grab an arg of the proper type */
  static T* getarg(Lua& lua, int n = 0) {
    if (n == 0) n = lua.nextarg();
    void* p = lua_touserdata(lua, n);
    return p != 0 && lua_tag(lua, n) == otag ? (T*) p : 0;
  }
};

template <class T> int Luna<T>::otag = 0;

template <class T> Lua& operator>> (Lua& lua, T*& arg) {
  if (lua.more()) arg = Luna<T>::getarg(lua);
  return lua;
}
