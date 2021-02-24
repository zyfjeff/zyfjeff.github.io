## Lua filter实现分析

### 核心数据结构

* `ThreadLocalState`

通过这个数据结构给每个worker线程创建对应的`lua_State`结构

```c++
ThreadLocalState::ThreadLocalState(const std::string& code, ThreadLocal::SlotAllocator& tls)
    : tls_slot_(ThreadLocal::TypedSlot<LuaThreadLocal>::makeUnique(tls)) {

  // First verify that the supplied code can be parsed.
  CSmartPtr<lua_State, lua_close> state(lua_open());
  RELEASE_ASSERT(state.get() != nullptr, "unable to create new Lua state object");
  luaL_openlibs(state.get());

  if (0 != luaL_dostring(state.get(), code.c_str())) {
    throw LuaException(fmt::format("script load error: {}", lua_tostring(state.get(), -1)));
  }

  // Now initialize on all threads.
  tls_slot_->set([code](Event::Dispatcher&) { return std::make_shared<LuaThreadLocal>(code); });
}
```

类型注册，给每个worker线程的`lua_State`都注册一些`userData`。
```c++
template <class T> void registerType() {
tls_slot_->runOnAllThreads(
     // 调用具体类型的registerType静态方法
    [](OptRef<LuaThreadLocal> tls) { T::registerType(tls->state_.get()); });
}

/**
 * Register a type with Lua.
 * @param state supplies the state to register with.
 */
static void registerType(lua_State* state) {
std::vector<luaL_Reg> to_register;

// Fetch all of the functions to be exported to Lua so that we can register them in the
// metatable.
ExportedFunctions functions = T::exportedFunctions();
for (auto function : functions) {
    to_register.push_back({function.first, function.second});
}

// Always register a __gc method so that we can run the object's destructor. We do this
// manually because the memory is raw and was allocated by Lua.
to_register.push_back(
    {"__gc", [](lua_State* state) {
        T* object = alignAndCast<T>(luaL_checkudata(state, 1, typeid(T).name()));
        ENVOY_LOG(trace, "destroying {} at {}", typeid(T).name(), static_cast<void*>(object));
        object->~T();
        return 0;
        }});

// Add the sentinel.
to_register.push_back({nullptr, nullptr});

// Register the type by creating a new metatable, setting __index to itself, and then
// performing the register.
ENVOY_LOG(debug, "registering new type: {}", typeid(T).name());
int rc = luaL_newmetatable(state, typeid(T).name());
ASSERT(rc == 1);

lua_pushvalue(state, -1);
lua_setfield(state, -2, "__index");
luaL_register(state, nullptr, to_register.data());
}
```


* `LuaThreadLocal`

每个线程的thread local都会保存这个数据结构，这个数据结构中保存了`lus_State`

```c++
ThreadLocalState::LuaThreadLocal::LuaThreadLocal(const std::string& code) : state_(lua_open()) {
  RELEASE_ASSERT(state_.get() != nullptr, "unable to create new Lua state object");
  luaL_openlibs(state_.get());
  int rc = luaL_dostring(state_.get(), code.c_str());
  ASSERT(rc == 0);
}
```