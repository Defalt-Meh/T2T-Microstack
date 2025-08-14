#include "lob.h"
#include <cassert>
#include <algorithm>
#include <cstddef> // size_t

namespace t2t::lob {

// -------- Side impl --------
Lob::Side::Side(bool buy)
: pool(static_cast<size_t>(MAX_ORDERS)),
  levels(static_cast<size_t>(MAX_LEVELS)),
  px2lvl(16384u, INT32_MIN),
  id2ord(1u << 20, 0u),
  best_level(-1),
  is_buy(buy),
  free_head(-1) {
  reset();
}

void Lob::Side::reset() {
  // pool free list
  free_head = 0;
  for (size_t i = 0; i < static_cast<size_t>(MAX_ORDERS); ++i) {
    pool[i] = OrderNode{};
    pool[i].next   = (i + 1u < static_cast<size_t>(MAX_ORDERS)) ? static_cast<int>(i + 1u) : -1;
    pool[i].prev   = -1;
    pool[i].level  = -1;
    pool[i].active = false;
  }
  // levels
  for (size_t i = 0; i < static_cast<size_t>(MAX_LEVELS); ++i) {
    levels[i] = PriceLevel{};
  }
  px2lvl.clear(); id2ord.clear(); best_level = -1;
}

int Lob::Side::alloc_node() {
  const int idx = free_head;
  assert(idx >= 0 && "order pool exhausted");
  const size_t sidx = static_cast<size_t>(idx);
  free_head = pool[sidx].next;
  pool[sidx].next = -1;
  pool[sidx].prev = -1;
  pool[sidx].active = true;
  return idx;
}

void Lob::Side::free_node(int idx) {
  if (idx < 0) return;
  const size_t sidx = static_cast<size_t>(idx);
  auto& n = pool[sidx];
  n.active = false;
  n.level  = -1;
  n.qty    = 0;
  n.prev   = -1;
  n.next   = free_head;
  free_head = idx;
}

// -------- Lob impl --------
Lob::Lob() : bid_(true), ask_(false) {}
void Lob::reset() { bid_.reset(); ask_.reset(); }

int Lob::best_bid() const {
  if (bid_.best_level < 0) return INT32_MIN;
  return bid_.levels[static_cast<size_t>(bid_.best_level)].px;
}
int Lob::best_ask() const {
  if (ask_.best_level < 0) return INT32_MAX;
  return ask_.levels[static_cast<size_t>(ask_.best_level)].px;
}

int Lob::ensure_level(Side& s, int32_t px) {
  int lvl = s.px2lvl.get(px);
  if (lvl >= 0) return lvl;

  // find a free level slot
  for (size_t i = 0; i < static_cast<size_t>(MAX_LEVELS); ++i) {
    auto& L = s.levels[i];
    if (!L.active) {
      L = PriceLevel{};
      L.active = true; L.px = px; L.head = -1; L.tail = -1; L.total_qty = 0;
      s.px2lvl.put(px, static_cast<int>(i));
      if (s.best_level < 0) {
        s.best_level = static_cast<int>(i);
      } else {
        const int bp = s.levels[static_cast<size_t>(s.best_level)].px;
        if (s.is_buy ? (px > bp) : (px < bp)) s.best_level = static_cast<int>(i);
      }
      return static_cast<int>(i);
    }
  }
  assert(false && "no free price level");
  return -1;
}

void Lob::enqueue(Side& s, const Order& o) {
  const int lvl = ensure_level(s, o.px);
  const size_t slvl = static_cast<size_t>(lvl);
  const int idx = s.alloc_node();
  const size_t sidx = static_cast<size_t>(idx);

  auto& n = s.pool[sidx];
  n.id=o.id; n.px=o.px; n.qty=o.qty; n.ts=o.ts; n.is_buy=o.is_buy; n.level=lvl;

  auto& L = s.levels[slvl];
  n.prev = L.tail;
  n.next = -1;
  if (L.tail >= 0) {
    s.pool[static_cast<size_t>(L.tail)].next = idx;
  } else {
    L.head = idx;
  }
  L.tail = idx; L.total_qty += o.qty;

  s.id2ord.put(o.id, idx);

  // update best
  if (s.best_level < 0) {
    s.best_level = lvl;
  } else {
    const int bp = s.levels[static_cast<size_t>(s.best_level)].px;
    if (s.is_buy ? (o.px > bp) : (o.px < bp)) s.best_level = lvl;
  }
}

void Lob::remove_idx(Side& s, int idx) {
  if (idx < 0) return;
  const size_t sidx = static_cast<size_t>(idx);
  auto& n = s.pool[sidx]; if (!n.active) return;

  const int lvl_idx = n.level;
  auto& L = s.levels[static_cast<size_t>(lvl_idx)];

  if (n.prev >= 0) s.pool[static_cast<size_t>(n.prev)].next = n.next; else L.head = n.next;
  if (n.next >= 0) s.pool[static_cast<size_t>(n.next)].prev = n.prev; else L.tail = n.prev;

  L.total_qty -= n.qty;
  s.id2ord.erase(n.id);
  s.free_node(idx);

  if (L.total_qty <= 0) {
    // deactivate level
    s.px2lvl.erase(L.px);
    L = PriceLevel{};
    if (s.best_level == lvl_idx) {
      // recompute best
      s.best_level = -1;
      for (size_t i = 0; i < static_cast<size_t>(MAX_LEVELS); ++i) {
        if (!s.levels[i].active) continue;
        if (s.best_level < 0) {
          s.best_level = static_cast<int>(i);
        } else {
          const int cur = s.levels[static_cast<size_t>(s.best_level)].px;
          if (s.is_buy ? (s.levels[i].px > cur) : (s.levels[i].px < cur)) {
            s.best_level = static_cast<int>(i);
          }
        }
      }
    }
  }
}

void Lob::add(const Order& o) {
  Side& s = o.is_buy ? bid_ : ask_;
  enqueue(s, o);
}

void Lob::cancel(uint32_t id) {
  int idx = bid_.id2ord.get(id);
  if (idx >= 0) { remove_idx(bid_, idx); return; }
  idx = ask_.id2ord.get(id);
  if (idx >= 0) { remove_idx(ask_, idx); return; }
  // idempotent if not found
}

int Lob::best_index(const Side& s) const { return s.best_level; }

bool Lob::match_top(Exec& e) {
  const int bi = best_index(bid_), ai = best_index(ask_);
  if (bi < 0 || ai < 0) return false;
  auto& B = bid_.levels[static_cast<size_t>(bi)];
  auto& A = ask_.levels[static_cast<size_t>(ai)];
  if (!B.active || !A.active) return false;
  if (B.px < A.px) return false;  // not crossed

  const int bidx = B.head, aidx = A.head;
  if (bidx < 0 || aidx < 0) return false;

  auto& b = bid_.pool[static_cast<size_t>(bidx)];
  auto& a = ask_.pool[static_cast<size_t>(aidx)];

  const int32_t qty = (b.qty < a.qty) ? b.qty : a.qty;
  const int32_t px  = (b.ts <= a.ts) ? a.px : b.px;

  e.ts = (b.ts < a.ts ? a.ts : b.ts);
  e.id = (b.ts < a.ts ? b.id : a.id);
  e.qty = qty;
  e.px  = px;

  b.qty -= qty; a.qty -= qty;
  B.total_qty -= qty; A.total_qty -= qty;

  if (b.qty == 0) remove_idx(bid_, bidx);
  if (a.qty == 0) remove_idx(ask_, aidx);
  return true;
}

} // namespace t2t::lob
