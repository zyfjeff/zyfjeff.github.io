## Lua filter实现分析

* Envoy中用`CSmartPtr`来管理`lua_State`的关闭。
```C++
CSmartPtr<lua_State, lua_close> state(lua_open());
```





### Lua 基础

Lua与C/C++交互的核心在于虚拟栈，为什么这么说呢? 在lua中我们可以很简单的通过`t[k] = v`来操作一个表，并且k和v是任意lua类型，
但是在C中要想实现这个能力就比较困难了，C中的类型都是静态的，我们可能需要提供不同lua类型的重载函数才能实现这个表的赋值操作。
或者是提供一个复合类型，他可以表示任何lua类型。

```lua
void lua_pushnil(lua_State *L);
void lua_pushboolea(lua_State *L, int bool);
void lua_pushnumber(lua_State *L, lua_Number n);
void lua_pushinteger(lua_State *L, lua_Integer n);
void lua_pushlstring(lua_State *L, const char *s, size_t len); 
void lua_pushstring(lua_State *L, const char *s);

// 默认的lua栈大小是20，通过LUA_MINISTACK定义
int lua_checkstack (lua_State *L, int sz);

// 对lua_checkstack的封装，不返回错误码，会返回错误内容
void luaL_checkstack (lua_State *L, int sz, const char *msg);


// 检查堆栈指定位置的值是否是指定类型
int lua_is* (lua_State *L, int index);
```

```c
typedef int (*lua_CFunction) (lua_State *L);
```


* 用来创建一个`lua_State`，一个`lua_State`就表示一个lua虚拟机，所有的lua API的第一个参数都是`lua_State`
```lua
lua_open()  //lua5.1的语法
luaL_newstate() //最新的用法
```

* 用来加载标准库
```lua
luaL_openlibs
```

* 运行lua脚本

```lua
int luaL_dostring (lua_State *L, const char *str);
Loads and runs the given string. It is defined as the following macro:

     (luaL_loadstring(L, str) || lua_pcall(L, 0, LUA_MULTRET, 0))
```



```lua
LUA_GCCOUNT
LUA_GCCOUNTB

lua_gc
```