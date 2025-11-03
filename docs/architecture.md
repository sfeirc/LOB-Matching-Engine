# LOB Architecture Documentation

## System Overview

```
┌─────────────────────────────────────────────────────────┐
│                   CSV Input File                        │
│              (ts_ns, MsgType, Side, ...)                │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│                  CSV Reader                             │
│  - Parse messages                                       │
│  - Convert to Msg struct                               │
│  - Timestamp: CSV Read Time                             │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│              Matching Engine (OrderBook)                │
│                                                          │
│  ┌──────────────┐      ┌──────────────┐                │
│  │   Bid Book   │      │   Ask Book   │                │
│  │ (Price→Level)│      │ (Price→Level)│                │
│  │  std::map    │      │  std::map    │                │
│  └──────┬───────┘      └──────┬───────┘                │
│         │                     │                         │
│         ▼                     ▼                         │
│  ┌─────────────────────────────────┐                   │
│  │      PriceLevel (FIFO Queue)    │                   │
│  │  - Intrusive Doubly-Linked List │                   │
│  │  - O(1) cancel via pointers     │                   │
│  │  - Cached quantity totals        │                   │
│  └─────────────────────────────────┘                   │
│                                                          │
│  ┌─────────────────────────────────┐                   │
│  │      Object Pool (Orders)        │                   │
│  │  - Pre-allocated 2M Orders      │                   │
│  │  - Zero hot-path allocations     │                   │
│  └─────────────────────────────────┘                   │
│                                                          │
│  ┌─────────────────────────────────┐                   │
│  │    Order Lookup (Cancel)         │                   │
│  │  - unordered_map<Id, Order*>    │                   │
│  │  - O(1) hash lookup             │                   │
│  └─────────────────────────────────┘                   │
│                                                          │
│  Timestamp: Engine Processing Time                      │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│                    Trades Output                        │
│         (buy_id, sell_id, price, qty, ts)                │
└─────────────────────────────────────────────────────────┘
```

## Matching Algorithm Flow

### Limit Order Matching

```
┌─────────────────────────────────────────────────────────┐
│         NewLimit Order Arrives                          │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
        ┌────────────────────────┐
        │  Allocate from Pool    │
        │  (O(1), zero alloc)    │
        └────────────┬───────────┘
                     │
                     ▼
        ┌────────────────────────┐
        │  Match against          │
        │  Opposite Side          │
        └────────────┬───────────┘
                     │
                     ▼
    ┌────────────────────────────────┐
    │  While (qty > 0 && book !empty)│
    └────────────┬───────────────────┘
                 │
        ┌────────┴────────┐
        │                 │
        ▼                 ▼
┌──────────────┐  ┌──────────────┐
│ Get Best     │  │ Price Check  │
│ Price Level  │  │ Compatible?  │
└──────┬───────┘  └──────┬───────┘
       │                 │
       └────────┬─────────┘
                │
                ▼
    ┌────────────────────────┐
    │  Match FIFO Orders     │
    │  in Price Level        │
    └────────────┬───────────┘
                 │
        ┌────────┴────────┐
        │                 │
        ▼                 ▼
┌──────────────┐  ┌──────────────┐
│ Partial Fill │  │ Full Fill    │
│ - Update qty │  │ - Remove     │
│ - Continue   │  │ - Next order │
└──────────────┘  └──────────────┘
        │                 │
        └────────┬────────┘
                 │
                 ▼
        ┌────────────────────────┐
        │  Residual Quantity?     │
        │  → Insert into Book     │
        └────────────────────────┘
```

## Data Structure Layout

### Order Structure (32-byte aligned)

```
┌─────────────────────────────────────┐
│         Order (32 bytes)             │
├─────────────────────────────────────┤
│ OrderId id       │ 8 bytes          │
│ Price price      │ 8 bytes          │
│ Quantity qty     │ 8 bytes          │
│ Side side        │ 1 byte (+7 pad)  │
│ Order* next      │ 8 bytes          │
│ Order* prev      │ 8 bytes          │
└─────────────────────────────────────┘
```

### Price Level Structure

```
┌─────────────────────────────────────┐
│      PriceLevel                      │
├─────────────────────────────────────┤
│ Order* head_      │ Front (oldest)   │
│ Order* tail_      │ Back (newest)    │
│ size_t count_     │ Number of orders │
│ Quantity cached_  │ Total quantity   │
└─────────────────────────────────────┘
         │
         ▼ (intrusive list)
    ┌─────────┐    ┌─────────┐    ┌─────────┐
    │ Order 1 │───▶│ Order 2 │───▶│ Order 3 │
    │ (head)  │    │         │    │ (tail)  │
    └─────────┘    └─────────┘    └─────────┘
         ▲             ▲                ▲
         └─────────────┴────────────────┘
            (doubly-linked, O(1) remove)
```

### Order Book Memory Layout

```
┌─────────────────────────────────────────────────┐
│           OrderBook Memory                      │
├─────────────────────────────────────────────────┤
│                                                 │
│  ┌───────────────────────────────────────┐     │
│  │ Object Pool (64 MB)                   │     │
│  │ [Order][Order][Order]...[Order]       │     │
│  │  2M pre-allocated orders              │     │
│  └───────────────────────────────────────┘     │
│                                                 │
│  ┌───────────────────────────────────────┐     │
│  │ Bid Book (std::map)                   │     │
│  │ Price 100050 → PriceLevel             │     │
│  │ Price 100049 → PriceLevel             │     │
│  │ ... (~500 price levels)              │     │
│  └───────────────────────────────────────┘     │
│                                                 │
│  ┌───────────────────────────────────────┐     │
│  │ Ask Book (std::map)                   │     │
│  │ Price 100051 → PriceLevel             │     │
│  │ Price 100052 → PriceLevel             │     │
│  │ ... (~500 price levels)              │     │
│  └───────────────────────────────────────┘     │
│                                                 │
│  ┌───────────────────────────────────────┐     │
│  │ Order Lookup (1.6 MB)                 │     │
│  │ unordered_map<OrderId, Order*>        │     │
│  │ ~100K active orders                   │     │
│  └───────────────────────────────────────┘     │
│                                                 │
│  ┌───────────────────────────────────────┐     │
│  │ Trade Vector (480 MB)                 │     │
│  │ [Trade][Trade]...[Trade]              │     │
│  │ 10M capacity (pre-allocated)          │     │
│  └───────────────────────────────────────┘     │
│                                                 │
└─────────────────────────────────────────────────┘

Total: ~545 MB for 10M message dataset
```

## Performance Characteristics

### Time Complexity

| Operation        | Complexity | Worst Case      |
| ---------------- | :--------: | --------------- |
| Insert Limit     | O(log n)   | n = price levels|
| Cancel Order     | O(1)       | Hash lookup     |
| Match Limit      | O(k)       | k = levels sweep|
| Match Market     | O(k·m)     | k·m = total orders|
| Get Best Bid/Ask | O(1)       | map.begin()     |

### Space Complexity

| Component        | Size (10M msgs) | Notes              |
| ---------------- | :-------------: | ------------------ |
| Object Pool      | 64 MB           | 2M Orders × 32B    |
| Bid/Ask Books    | 16 KB           | ~500 levels × 32B  |
| Order Lookup     | 1.6 MB          | ~100K orders × 16B |
| Trade Vector     | 480 MB          | 10M × 48B          |
| **Total**        | **545 MB**      | Mostly pre-allocation|

## Optimization Techniques

### 1. Object Pooling

```
Before: malloc/new per order (hot path)
After:  Pre-allocated pool (zero allocations)

Allocation Pattern:
┌─────────────────────────────────────┐
│ pool_next_ = 0                      │
│ pool_next_++ (atomic increment)     │
│ return &order_pool_[pool_next_]     │
└─────────────────────────────────────┘
```

### 2. Intrusive Lists

```
Before: std::deque (vector erase = O(n))
After:  Doubly-linked list (remove = O(1))

Cancel Operation:
┌─────────────────────────────────────┐
│ hash_map.find(order_id) → Order*    │
│ Order* → next_in_level, prev_in_level│
│ Update pointers: O(1)                │
│ Remove from map: O(1)                │
└─────────────────────────────────────┘
```

### 3. Branch Prediction

```cpp
// LIKELY path (most orders match)
if (LIKELY(order->qty > 0 && !asks_.empty())) {
    // 95% of cases
}

// UNLIKELY path (edge cases)
if (UNLIKELY(resting->qty <= 0)) {
    // 5% of cases
}
```

### 4. Memory Prefetching

```cpp
// Prefetch next order while processing current
if (LIKELY(resting->next_in_level)) {
    PREFETCH(resting->next_in_level);  // Fetch into cache
}
```

### 5. Cache Alignment

```
Order size: 32 bytes (perfect cache line alignment)
Cache line: 64 bytes (2 orders per line)

Memory Layout:
┌──────────────┬──────────────┐
│   Order 1    │   Order 2    │  ← Single cache line
│  (32 bytes)  │  (32 bytes)  │
└──────────────┴──────────────┘
```

