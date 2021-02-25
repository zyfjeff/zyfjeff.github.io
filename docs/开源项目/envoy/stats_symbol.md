# Stats Symbol分析

Stats按照`.`号分割，每一段被称为一个`Symbol`，都会分配一个Symbol，相同的则共用一个`Symbol`

```cpp
using Symbol = uint32_t;
```

`SymbolTable`会根据下面两个map做name到Symbol的映射

```cpp

  struct SharedSymbol {
    SharedSymbol(Symbol symbol) : symbol_(symbol), ref_count_(1) {}

    Symbol symbol_;
    // 记录Symbol被引用的次数，当引用次数为0的时候才会删除
    uint32_t ref_count_;
  };

  // Bitmap implementation.
  // The encode map stores both the symbol and the ref count of that symbol.
  // Using absl::string_view lets us only store the complete string once, in the decode map.
  // 根据stats name查询对应的symbol
  using EncodeMap = absl::flat_hash_map<absl::string_view, SharedSymbol, StringViewHash>;
  // 根据symbol查询对应的stats name
  using DecodeMap = absl::flat_hash_map<Symbol, InlineStringPtr>;
  EncodeMap encode_map_ GUARDED_BY(lock_);
  DecodeMap decode_map_ GUARDED_BY(lock_);
```

把stats中的一段添加到SymbolTable的过程如下:

1. 将stat name 进行Encoding
2. 创建指定大小的Storage
3. 将Encoding后的内容存放到Storage中


```cpp
SymbolTable::StoragePtr SymbolTableImpl::encode(absl::string_view name) {
  Encoding encoding;
  // 将stat按照.号切割成一个个token，然后放到Encoding中
  addTokensToEncoding(name, encoding);
  // 接着创建一个Storage存储stats编码后的内容
  auto bytes = std::make_unique<Storage>(encoding.bytesRequired());
  encoding.moveToStorage(bytes.get());
  return bytes;
}
```

一个Storage就是一个`uint_8`的数组，`addTokensToEncoding`会将stats name按照.号切割成一个个token放到Encoding中

```cpp
  using Storage = uint8_t[];
  using StoragePtr = std::unique_ptr<Storage>;
```


这个是stats name进行Encoding的过程

1.  `absl::StrSplit(name, '.')`切割stats name
2.  `symbols.push_back(toSymbol(token))` 每一个stat name通过`toSymbol`转换为Symbol存起来
3.  `encoding.addSymbol(symbol)` 将所有的Symbol添加到Encoding中进行编码

```cpp
void SymbolTableImpl::addTokensToEncoding(const absl::string_view name, Encoding& encoding) {
  if (name.empty()) {
    return;
  }

  // We want to hold the lock for the minimum amount of time, so we do the
  // string-splitting and prepare a temp vector of Symbol first.
  const std::vector<absl::string_view> tokens = absl::StrSplit(name, '.');
  std::vector<Symbol> symbols;
  symbols.reserve(tokens.size());

  // Now take the lock and populate the Symbol objects, which involves bumping
  // ref-counts in this.
  {
    Thread::LockGuard lock(lock_);
    for (auto& token : tokens) {
      symbols.push_back(toSymbol(token));
    }
  }

  // Now efficiently encode the array of 32-bit symbols into a uint8_t array.
  for (Symbol symbol : symbols) {
    encoding.addSymbol(symbol);
  }
}
```

`toSymbol`的实现就依赖上文中提到的`EncodeMap`表，stats的每一段都要去查询这个map，如果已经存在就直接返回对应的Symbol
否则就创建一个Symbol。Symbol本质上就是一个递增的`uin32_t`类型的整数。

```cpp
Symbol SymbolTableImpl::toSymbol(absl::string_view sv) {
  Symbol result;
  auto encode_find = encode_map_.find(sv);
  // If the string segment doesn't already exist,
  if (encode_find == encode_map_.end()) {
    // We create the actual string, place it in the decode_map_, and then insert
    // a string_view pointing to it in the encode_map_. This allows us to only
    // store the string once. We use unique_ptr so copies are not made as
    // flat_hash_map moves values around.
    InlineStringPtr str = InlineString::create(sv);
    auto encode_insert = encode_map_.insert({str->toStringView(), SharedSymbol(next_symbol_)});
    ASSERT(encode_insert.second);
    auto decode_insert = decode_map_.insert({next_symbol_, std::move(str)});
    ASSERT(decode_insert.second);

    result = next_symbol_;
    newSymbol();
  } else {
    // If the insertion didn't take place, return the actual value at that location and up the
    // refcount at that location
    result = encode_find->second.symbol_;
    ++(encode_find->second.ref_count_);
  }
  return result;
}
```

上面代码中中的`next_symbol_`始终指向下一次分配的时候要使用的`Symbol`，是通过`newSymbol`生成的

```cpp
// pool_定义
std::stack<Symbol> pool_

void SymbolTableImpl::newSymbol() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
  if (pool_.empty()) {
    next_symbol_ = ++monotonic_counter_;
  } else {
    next_symbol_ = pool_.top();
    pool_.pop();
  }
  // This should catch integer overflow for the new symbol.
  ASSERT(monotonic_counter_ != 0);
}

void SymbolTableImpl::free(const StatName& stat_name) {
  // Before taking the lock, decode the array of symbols from the SymbolTable::Storage.
  const SymbolVec symbols = Encoding::decodeSymbols(stat_name.data(), stat_name.dataSize());

  Thread::LockGuard lock(lock_);
  for (Symbol symbol : symbols) {
    auto decode_search = decode_map_.find(symbol);
    ASSERT(decode_search != decode_map_.end());

    auto encode_search = encode_map_.find(decode_search->second->toStringView());
    ASSERT(encode_search != encode_map_.end());

    // If that was the last remaining client usage of the symbol, erase the
    // current mappings and add the now-unused symbol to the reuse pool.
    //
    // The "if (--EXPR.ref_count_)" pattern speeds up BM_CreateRace by 20% in
    // symbol_table_speed_test.cc, relative to breaking out the decrement into a
    // separate step, likely due to the non-trivial dereferences in EXPR.
    if (--encode_search->second.ref_count_ == 0) {
      decode_map_.erase(decode_search);
      encode_map_.erase(encode_search);
      // 回收Symbol
      pool_.push(symbol);
    }
  }
}
```

当分配的`Symbol`被回收的时候就会通过放到pool_中下次就可以复用了，这个机制类似linux内核中的`inode`分配一样。
到此为止`Symbol`到stat name的映射关系，以及`Symbol`的分配和释放就讲清楚了，下一步就是将这些`Symbol`进一步`Encoding来`减少存储的大小。
在`addTokensToEncoding`的实现中的最后一步就是通过`Encoding::addSymbol`方法将一个stats产生的所有Symbol进行Encoding。

```cpp
static const uint32_t SpilloverMask = 0x80;
static const uint32_t Low7Bits = 0x7f;
std::vector<uint8_t> vec_;

void SymbolTableImpl::Encoding::addSymbol(Symbol symbol) {
  // UTF-8-like encoding where a value 127 or less gets written as a single
  // byte. For higher values we write the low-order 7 bits with a 1 in
  // the high-order bit. Then we right-shift 7 bits and keep adding more bytes
  // until we have consumed all the non-zero bits in symbol.
  //
  // When decoding, we stop consuming uint8_t when we see a uint8_t with
  // high-order bit 0.
  do {
    if (symbol < (1 << 7)) {
      vec_.push_back(symbol); // symbols <= 127 get encoded in one byte.
    } else {
      vec_.push_back((symbol & Low7Bits) | SpilloverMask); // symbols >= 128 need spillover bytes.
    }
    symbol >>= 7;
  } while (symbol != 0);
}
```

整个编码的过程类似于`UTF-8`编码，会根据`Symbol`本身的值大小来决定是使用多少个字节来存储，如果是小于128的话，那么就按照一个字节来存储
，如果是大于128那么就会进行切割。首先通过和`Low7Bits`相与拿到低7位，然后和`SpilloverMask`相或将最高位设置为1。这里有个疑问为什么是小于128就用
单字节存储呢?这里为什么不是256呢?，`vec_`的类型其实是`uint8_t`的，完全可以用来存储256。但是这里只用到了低7位，最高的那一位是用来表示这个`Symbol`是否
是多个字节组成还是一个字节组成的。如果是0就表示这个Symbo是一个单字节的。所以当包含多个字节的时候，需要和`SpilloverMask`相或将最高位设置为1来表示是多字节的`Symbol`。


最后一个stat name被编程成了一个`std::vector<uint8_t>`，然后通过`Encoding::moveToStorage`方法将整个`std::vector<uint8_t>`存放到`Storage`中

```cpp
uint64_t SymbolTableImpl::Encoding::moveToStorage(SymbolTable::Storage symbol_array) {
  const uint64_t sz = dataBytesRequired();
  symbol_array = writeLengthReturningNext(sz, symbol_array);
  if (sz != 0) {
    memcpy(symbol_array, vec_.data(), sz * sizeof(uint8_t));
  }
  vec_.clear(); // Logically transfer ownership, enabling empty assert on destruct.
  return sz + StatNameSizeEncodingBytes;
}
```

到此为止一个`Stat name`被编码成了一个`Storage`，这个`Storage`可以被用来构造成`StatName`结构，但是不拥有Storage这是对其引用，真正拥有`Storage`的是`StatNameStorage`
而`StatLNameList`则是包含了一组顺序的`Stat Name`的容器。



最后来看一个，如果将一个`Symbol`数组转换为对应的`stat name`。

```cpp
std::string SymbolTableImpl::toString(const StatName& stat_name) const {
  return decodeSymbolVec(Encoding::decodeSymbols(stat_name.data(), stat_name.dataSize()));
}

SymbolVec SymbolTableImpl::Encoding::decodeSymbols(const SymbolTable::Storage array,
                                                   uint64_t size) {
  SymbolVec symbol_vec;
  Symbol symbol = 0;
  for (uint32_t shift = 0; size > 0; --size, ++array) {
    uint32_t uc = static_cast<uint32_t>(*array);

    // Inverse addSymbol encoding, walking down the bytes, shifting them into
    // symbol, until a byte with a zero high order bit indicates this symbol is
    // complete and we can move to the next one.
    symbol |= (uc & Low7Bits) << shift;
    if ((uc & SpilloverMask) == 0) {
      symbol_vec.push_back(symbol);
      shift = 0;
      symbol = 0;
    } else {
      shift += 7;
    }
  }
  return symbol_vec;
}
```

`decodeSymbols`会将`Storage`转换成一个`SymbolVec`，因为一个`Storage`可以包含多个`Symbol`。转换的过程如下:

1. 每次从Storage中那一个`uint8_t`，然后转换为`uint32_t`，因为Symbol的类型就是`uint32_t`
2. 接着通过和`Low7Bits`相或拿到低7位的值
3. 判断`SpilloverMask`位是否是0，如果是0那就是一个完整的`Symbol`直接放到SymbolVec即可
4. 如果是1表明，Symbol还要继续组装，再次读取一个uint8_t，然后取低7位，这个时候，需要向左移动7位，因为Symbol的每一个部分都是7位组成，依次排放的。