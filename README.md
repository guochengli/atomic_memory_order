
# 六种 memory_order

Key Points:
- 要从"防止编译器重排"与"防止CPU乱序"两个角度去理解 memory_order
- release semantics 一般表示 "最后 Store"
- acquire semantics 一般表示 "最先 Load"
- 注意 x86_64 CPU 一般[^1]都满足 strong memory model 特性：CPU 四种可能发生的乱序中，只允许StoreLoad乱序(或称为StoreLoad重排)

<p align="center">
  <img src="http://45.32.60.168/lgc/2019-07-27_18.38.17.png">
</p>

## 1）与 atomic 连用

**relaxed**：atomic variable 的 **relaxed** 操作和普通变量普通操作有何区别？
- 普通变量的SL操作并不保证原子性，而 atomic 变量的所有操作都是保证原子性的。(TODO: 甚至不会被中断打断？)
- x86_64 实测：atomic 变量结合任意 memory_order 都能有效防止编译器重排。

**consume**：所有后续 data-dependent 的 S/L 操作禁止被 re-order 到本 L 前面。和 release-S 连用。

**acquire**：所有后续 S/L 操作禁止被 re-order 到本 L 前面。和 release-S 连用。
- acquire 操作强调本 L 一定是"最先 Load"。
- x86_64 实测：
  - a.load(acquire/relaxed) 产生的机器码相同。猜测是由于 x86_64 的 strong-memory-model 性质，禁止 **L**-S 和 **L**-L 重排，其任何 Load 都具有 acquire 语义。
  - UB 操作：a.store(acquire)，则机器码会在 S 后面添加一个 mfence。此时实际产生的语义为：所有后续 S/L 操作禁止被 re-order 到本 S 前面。猜测是由于 x86_64 CPU 允许 S-L re-order，那么要保证此 S 后面的 S/L 不被重排到本 S 前，那么只能加入 fence 指令。
  - UB 操作：a.load(release) 产生的机器码等同于 a.load(acquire)。(注意并不符合类似 S-release 的语义 "所有前面 S/L 操作禁止被 re-order 到本 L 后面，如果要符合的话必须在本 L 前面加 mfence)"
  - UB 操作：a.load() 接所有 memory_order 都等效于 a.load(acquire)，即编译产生同样的机器码。

**release**：所有前面 S/L 操作禁止被 re-order 到本 S 后面。和 acquire/consume-L 连用。
- release 操作强调本 S 操作一定是"最后 Store"。
- x86_64 实测：
  - a.store(release/relaxed) 产生的机器码相同。猜测是由于 x86_64 的 strong-memory-model 性质，禁止 S-**S** 和 L-**S** 重排，其任何 Store 都具有 release 语义。
  - UB 操作：a.store(acquire/consume/acq_rel/seq_cst) 产生的机器码相同，都是在本 S 后面加入一个 `mfence` 指令。猜测是由于 x86_64 中，只需要在 S 后面加一个 mfence，该 S 就能同时满足"本 S 前所有的 S/L 禁止被重排到本 S 之后(即 release 语义)"与"本 S 后所有的 S/L 禁止被重排到本 S 之前(类似 L-acquire 语义)"的两个条件

**acq_rel**：适用于RMW操作。
- 1)本线程所有 S/L 操作(无论前后)禁止被 re-order 到本 S 前或后。
  - 为什的只提到S？(TODO: 这个RMW操作一定是保证原子性的吗？)
  - 其实是只需要关心 S 即可。因为RMW操作的特性类似一个 LS 操作，这里 S 已经作为了同步点，本线程 L 前的 S/L 即使重排也只能重排在本 L 后且本 S 前，即使发生这种重排也是没有关系的。而本 S 后所有的 S/L 都不可被重排到本 S 前，这样就间接保证了不可被重排到本 L 前，从而也保证了 acquire 语义。
- 2)执行了相应 release 操作的其他线程，其 release 前的所有 S 一定发生在本 S 前。
  - 这句话其实暗含了其他线程造成的该原子变量本身的 S 一定是发生在本 L 前的。因为如果其他线程对该原子变量的改动还没有被本线程观察到的话，可以认为其他线程并没有写它，也就不存在同步问题。既然观察到了变化，那么这个变化伴随的更改顺序关系才需要被讨论。
  - 由于 S 比 L 慢，因此这个语义只要求其他线程被 release 的 S 一定是发生在本 S 之前就可以，没必要严格到必须发生在本 L 之前，那样效率会变低。不过这个要求已经比单纯的 acquire 严格了，即要求其他线程被 release 的 S 可以发生在本 L 后但必须发生在本 S 前，而单纯的 acquire 只暗含本 L 之后，就能看到相关线程所有被 release 的 S 了
- 术语情景：x=1; y.store(1, release); 此时称为 x=1 和 y=1 这些 S 最后执行了 release 操作。或者说 x=1 和 y=1 都是被该线程 release 的 S 操作，且同步点是 Y。

**seq_cst**：
- 对于 L 执行 acquire 语义
- 对于 S 执行 release 语义
- 对于 RMW 执行 acq_rel 语义，并且对所有线程执行了 seq_cst 操作的 S，都存在一个 TSO
  - TSO：Total Single Order，所有线程观察到的 S 顺序都是一样的？
- x86_64 实测：
  - seq_cst 与 load 操作连用时，与所有 memory_order 都等效。即编译产生同样的机器码。
  - seq_cst 与 store 操作连用时，与 acquire/consume/acq_rel 等效。

## 2) 与 atomic_thread_fence 连用

TODO
