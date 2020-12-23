## UserData
通过lua的UserData可以存储C中创建的自定义类型，首先通过`lua_newuserdata`创建一块内存，然后用自定义类型的指针指向这块内存，
然后我们就可以在C中操作自定义类型了。



## upvalues

## 其他

* `luaL_error`

```lua
static int luaL_error(lua_State*L,const char*fmt,...){
    va_list argp;
    va_start(argp,fmt);
    luaL_where(L,1);
    lua_pushvfstring(L,fmt,argp);
    va_end(argp);
    lua_concat(L,2);
    return lua_error(L);
}
```

## 引用

我们没办法使用C指针来指向一个lua对象，因此如果想在C代码中传递lua对象就只能使用lua给我们提供的引用系统。因此当我们在C代码中需要指向一个lua对象的时候，可以先给
这个lua对象创建一个引用，然后在C代码中保存这个引用就可以。这样就可以通过传递引用的方式来代替传递lua对象。lua中的引用本质上其实就是注册表中以数值为key，以lua对象
为value的字段。

* `int luaL_ref (lua_State *L, int t);`

在t所在table中，创建一个引用，并将栈顶的元素进行引用关联，最终返回一个唯一的数值。接下来通过这个数值，我们就可以通过`lua_rawgeti`获取到引用的对象了。

```C
// 给global全局变量创建引用
lua_getglobal(L,  global);
int ref = luaL_ref(L, LUA_REGISTRYINDEX);
// 通过这个引用访问全局变量
lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
```


## 全局变量和环境
Lua5.3中没有全局变量，但是会通过全局环境(global environment，一个普通的表)来进行模拟，Lua加载代码运行的时候，会创建一个名为_ENV的upvale，这是一个普通的表
是在Lua虚拟机初始化的时候就被创建了，里面包含了各种标准库函数。在我们访问全局变量的时候, 实际上是在访问上值 _ENV 中的键值，lua编译器在编译的时候，其实就会给
所有的全局变量都会被重写成对_ENV表的字段访问。G其实就是_ENV中的一个字段。_G实际指向了_ENV。

```lua
print(_ENV._G == _ENV) -- => Output: true
print(_G == _ENV) -- => Output: true
print(_G._G == _ENV) -- => Output: true
print(_G._G == _G) -- => Output: true
```


## 函数

* `lua_call`
* `lua_pcall`


## 协程和线程

* `lua_State *lua_newthread (lua_State *L);`

创建一个线程，然后放到栈顶，并且返回lua_State来表示这个新的线程。这个返回的新lua_State和原来的lua_State共享所有的全局对象，但是有自己独立的执行栈。


* `int lua_resume (lua_State *L, int narg);`

启动或者恢复指定协程，需要将要运行的funcion、以及参数都放到堆栈中。如果这个协程运行结束没有任何错误就返回0，如果返回的是`LUA_YIELD`,表明协程停止了，此时堆栈上
放着

Starts and resumes a coroutine in a given thread.

To start a coroutine, you first create a new thread (see lua_newthread); then you push onto its stack the main function plus any arguments; then you call lua_resume, with narg being the number of arguments. This call returns when the coroutine suspends or finishes its execution. When it returns, the stack contains all values passed to lua_yield, or all values returned by the body function. lua_resume returns LUA_YIELD if the coroutine yields, 0 if the coroutine finishes its execution without errors, or an error code in case of errors (see lua_pcall). In case of errors, the stack is not unwound, so you can use the debug API over it. The error message is on the top of the stack. To restart a coroutine, you put on its stack only the values to be passed as results from yield, and then call lua_resume.

* `lua_yeild`

## 基本操作

* `lua_pushvfstring`
* `luaL_where`
* `lua_concat`
* `luaL_typename`


## 表和元表

registry 是lua的注册表(是一张只能被C代码访问的全局表)，获取registry就和获取普通的表一样，可以像操作普通的表一样来操作registry表，只不过这个表的默认在栈中的位置是`LUA_REGISTRYINDEX`
(lua中定义了这个值为-10000)，这个位置并不是真实存在的我们称之为伪索引。通过这个伪索引就可以获取到registry表，然后我们就可以像操作正常表一样的方式来操作registry表了。所有的C库代码都共享
同一个registry表，因此我们需要小心使用这个表，避免冲突。

> 不能用`lua_remove`和`lua_insert`来操作registry表，因为registry并不是真实存在于堆栈中，而`lua_remove`和`lua_insert`要操作真实存在于堆栈中的值。
> registry注册表中不能使用数值类型的键，因为lua语言将数值键用作引用系统了。

* `int luaL_newmetatable (lua_State *L, const char *tname)`
创建一个元表，并放到registry中，内部其实就是通过`lua_newtable`，创建了一个普通的表，然后通过`lua_setfield`给registry注册表添加了一个key为tname，value为空table的字段。
并将其放在registry表，而普通的表是放在global中的。


```lua
-- luaL_newmetatable的实现
static int luaL_newmetatable(lua_State*L,const char*tname){
    -- 查询registry表是否存在对应的字段
    lua_getfield(L,(-10000),tname);
    if(!lua_isnil(L,-1))
        return 0;
    -- 如果不存在，会在栈顶放一个nil值，因此这里需要去掉栈顶
    lua_pop(L,1);
    -- 创建一个普通的表，然后把这个表再次放到栈顶上，因为lua_setfield会消耗栈顶，最后返回的时候需要把
    -- 这个table放在栈顶上，因此这里把table再次放到栈顶上了，相当于栈的1、2两个位置都是这个table
    lua_newtable(L);
    lua_pushvalue(L,-1);
    -- 设置注册表key为tname，value为刚创建的空table
    lua_setfield(L,(-10000),tname);
    return 1;
}
```

* `void lua_gettable (lua_State *L, int index)`

首先从index处获取到table，然后用栈顶的值作为表的key来获取其value，最后将获取到的value放到栈顶上。

```lua
    // 要查询的key放入栈中
    lua_pushstring(L, key); /* push key */
    // -2 index所在位置存放了table
    lua_gettable(L, -2);    /* get background[key] */
    // 从栈顶上获取到table中指定key的值
    result = (int)(lua_tonumberx(L, -1, &isnum) * MAX_COLOR);
```

* `void lua_setfield (lua_State *L, int index, const char *k)`

通过index找到table，然后给这个table设置key为k，value为栈顶值。

```lua
    int rc = luaL_newmetatable(state, typeid(T).name());
    lua_pushvalue(state, -1); -- 将创建的memtable拷贝一份放在-2位置
    lua_setfield(state, -2, "__index"); -- 给刚创建的memtable设置key为__index，value为memtable自己的字段
```
上面的代码中，首先创建了一个元表，放在了栈顶上，然后给这个元表设置一个key为`__index`，value为元表自己的字段。

> 

* `void luaL_register (lua_State *L, const char *libname, const luaL_Reg *l)`

如果libname为空，就简单的将l所对应的一系列function注册到栈顶的table中，否则就先去注册表中
找key为`_LOADED`的表(等同于`package.load[libname]`)的libname字段，如果找到了就把function注册到libname对应的表中，否则就去
全局表中查找key为libname的表。


```lua
static void luaL_register(lua_State*L,const char*libname, const luaL_Reg*l){
  luaI_openlib(L,libname,l,0);
}

static int libsize(const luaL_Reg*l){
    int size=0;
    for(;l->name;l++)size++;
    return size;
}

static void luaI_openlib(lua_State*L,const char*libname, const luaL_Reg*l,int nup){
    if(libname){
        int size=libsize(l);
        luaL_findtable(L,(-10000),"_LOADED",1);
        lua_getfield(L,-1,libname);
        if(!lua_istable(L,-1)){
        lua_pop(L,1);
            if(luaL_findtable(L,(-10002),libname,size)!=NULL)
            luaL_error(L,"name conflict for module "LUA_QL("%s"),libname);
            lua_pushvalue(L,-1);
            lua_setfield(L,-3,libname);
        }
        lua_remove(L,-2);
        lua_insert(L,-(nup+1));
    }
    for(;l->name;l++){
        int i;
        for(i=0;i<nup;i++)
        lua_pushvalue(L,-nup);
        lua_pushcclosure(L,l->func,nup);
        lua_setfield(L,-(nup+2),l->name);
    }
    lua_pop(L,nup);
}

```

* `void lua_getglobal (lua_State *L, const char *name);`
获取全局表中key为name的value值，也就是获取name全局变量


* `void lua_rawgeti (lua_State *L, int index, int n);`
raw前缀的方法都是直接访问表本身，不会访问元表，这个方法用来访问index所在表中的key为n的值。



## 元方法

* `__index` 

当访问表中的元素不存在时，会间接的访问这个表对应元表中的`__index`，如果元表中的`__index`字段是一个表就访问这个表对应的字段，如果是一个函数的话就会调用这个函数
并且会把table和键作为参数传递给这个函数。

```lua
-- __index是一个table
local MyClass = {} -- the table representing the class, which will double as the metatable for the instances
-- 惯用法，将table指向自己，MyClass会被作为其他表的元表，当访问不存在的字段就会自动访问MyClass对应的字段了。
MyClass.__index = MyClass -- failed table lookups on the instances should fallback to the class table, to get methods

-- syntax equivalent to "MyClass.new = function..."
function MyClass.new(init)
  -- 将MyClass 作为元表
  local self = setmetatable({}, MyClass)
  self.value = init
  return self
end

function MyClass.set_value(self, newval)
  self.value = newval
end

function MyClass.get_value(self)
  return self.value
end

local i = MyClass.new(5)
-- tbl:name(arg) is a shortcut for tbl.name(tbl, arg), except tbl is evaluated only once
-- get_value并不存在于i表中，因此会去访问MyClass元表的get_value字段
print(i:get_value()) --> 5
i:set_value(6)
print(i:get_value()) --> 6
```


```lua
-- __index是一个函数
mytable = setmetatable({key1 = "value1"}, {
  __index = function(mytable, key)
    if key == "key2" then
      return "metatablevalue"
    else
      return nil
    end
  end
})

print(mytable.key1,mytable.key2)

-- 输出结果 value1    metatablevalue
```