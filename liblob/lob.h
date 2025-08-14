#pragma once
#include <cstdint>
#include <vector>
#include <climits>

namespace t2t::lob {

// ------------ Public API ------------
struct Order {
  uint64_t ts;
  uint32_t id;
  int32_t  px;
  int32_t  qty;
  bool     is_buy;
};
struct Exec {
  uint64_t ts;
  uint32_t id;   // id of aggressor (simplified)
  int32_t  px;
  int32_t  qty;
};

class Lob {
public:
  Lob();
  void reset();

  void add(const Order& o);       // price-time priority at each level
  void cancel(uint32_t id);       // idempotent; safe if already gone
  bool match_top(Exec& e);        // consume at top if crossed; 1 exec per call

  int  best_bid() const;          // INT32_MIN if empty
  int  best_ask() const;          // INT32_MAX if empty

private:
  // ------------ Internal structures ------------
  static constexpr int MAX_ORDERS = 2'000'000;   // per side pool
  static constexpr int MAX_LEVELS = 8192;        // per side levels
  template <typename K>
  struct FixedMap {
    struct Node { K key; int val; };
    std::vector<Node> tab;
    K empty_key;
    explicit FixedMap(size_t pow2, K empty) : tab(pow2), empty_key(empty) {
      for (auto& n : tab) { n.key = empty_key; n.val = -1; }
    }
    inline void clear() { for (auto& n: tab) { n.key=empty_key; n.val=-1; } }
    inline int  get(const K& k) const {
      size_t m = tab.size()-1, h = (size_t)1469598103934665603ull ^ (size_t)k;
      for (size_t p=0;p<tab.size();++p) { auto& n = tab[(h+p)&m]; if (n.key==k) return n.val; if (n.key==empty_key) return -1; }
      return -1;
    }
    inline void put(const K& k, int v) {
      size_t m = tab.size()-1, h = (size_t)1469598103934665603ull ^ (size_t)k;
      for (size_t p=0;p<tab.size();++p) { auto& n = tab[(h+p)&m]; if (n.key==empty_key || n.key==k) { n.key=k; n.val=v; return; } }
    }
    inline void erase(const K& k) {
      size_t m = tab.size()-1, h = (size_t)1469598103934665603ull ^ (size_t)k;
      for (size_t p=0;p<tab.size();++p) { auto& n = tab[(h+p)&m]; if (n.key==k) { n.key=empty_key; n.val=-1; return; } if (n.key==empty_key) return; }
    }
  };

  struct OrderNode {
    uint32_t id{0};
    int32_t  px{0};
    int32_t  qty{0};
    uint64_t ts{0};
    int      next{-1};   // FIFO queue (linked by index)
    int      prev{-1};
    int      level{-1};  // owning price level index
    bool     is_buy{true};
    bool     active{false};
  };

  struct PriceLevel {
    int32_t px{0};
    int     head{-1};
    int     tail{-1};
    int     total_qty{0};
    bool    active{false};
  };

  struct Side {
    std::vector<OrderNode> pool;
    std::vector<PriceLevel> levels;
    FixedMap<int32_t>   px2lvl;   // price -> level index
    FixedMap<uint32_t>  id2ord;   // id -> order index
    int best_level{-1};           // index of best (max for bid, min for ask)
    bool is_buy{true};
    int  free_head{-1};           // free list head for pool indices
    explicit Side(bool buy);
    void reset();
    int  alloc_node();            // from free list
    void free_node(int idx);      // return to free list
  };

  Side bid_, ask_;

  int  ensure_level(Side& s, int32_t px);
  void enqueue(Side& s, const Order& o);
  void remove_idx(Side& s, int idx);
  int  best_index(const Side& s) const;
};

} // namespace t2t::lob
