#include <vector>
#include <array>
#include <memory>
#include <algorithm>

#include "small_vector.h"
#include "math.h"


template <typename type_t>
struct rect_t {
  type_t x0, y0, x1, y1;
};


struct triangle_t {
  std::array<float4, 3> p;
  std::array<float2, 3> t;
  std::array<float4, 3> c;
};


struct bin_manager_t {
  static const uint32_t max_triangles = 1024;
  static const uint32_t max_fill = 512;
  static const uint32_t group_size = 32;

  small_vector<triangle_t, max_triangles> triangle;

  std::unique_ptr<uint32_t[]> index;
  std::unique_ptr<uint32_t[]> fill;

  uint32_t cells_x, cells_y, cells;

  bin_manager_t()
    : cells_x(0)
    , cells_y(0)
    , cells(0)
  {}

  void partition(int32_t w, int32_t h) {
    cells_x = (((w - 1) | (group_size - 1)) + 1) / group_size;
    cells_y = (((h - 1) | (group_size - 1)) + 1) / group_size;
    cells = cells_x * cells_y;
    index = std::make_unique<uint32_t[]>(cells * max_fill);
    fill = std::make_unique<uint32_t[]>(cells);
  }

  triangle_t &emplate() {
    return triangle.emplace_back();
  }

  void clear() {
    triangle.clear();
    assert(fill);
    for (uint32_t i = 0; i < cells; ++i)
      fill[i] = 0;
  }

  void assign();

protected:
  void _push_tri(uint32_t cell, uint32_t tri_index);
  bool _tri_intersect(const triangle_t &t, rect_t<float> &box);
};


namespace {

template <typename type_t> rect_t<type_t> tri_bound(const triangle_t &t) {
  return rect_t<type_t>{
      std::min<type_t>({type_t(t.c[0].x), type_t(t.c[1].x), type_t(t.c[2].x)}),
      std::min<type_t>({type_t(t.c[0].y), type_t(t.c[1].y), type_t(t.c[2].y)}),
      std::max<type_t>({type_t(t.c[0].x), type_t(t.c[1].x), type_t(t.c[2].x)}),
      std::max<type_t>({type_t(t.c[0].y), type_t(t.c[1].y), type_t(t.c[2].y)})};
}

float is_small_tri(const triangle_t &t) {

  // XXX: use combined edge length as heuristic?

  const auto r = tri_bound<float>(t);
  return fabsf(r.x1 - r.x0) * fabsf(r.y1 - r.y0);
}

// return true if a triangle is backfacing
template <typename vec>
bool is_backface(const vec &a, const vec &b, const vec &c) {
  const float v1 = a.x - b.x;
  const float v2 = a.y - b.y;
  const float w1 = c.x - b.x;
  const float w2 = c.y - b.y;
  return 0.f > (v1 * w2 - v2 * w1);
}

} // namespace


// return true if a triangle intersect unit square [0,0] -> [1,1]
template <typename vec>
bool tri_vis(const vec &a, const vec &b, const vec &c) {

  // cohen-sutherland style trivial box clipping
  {
    const auto classify = [](const vec &p) -> int {
      return (p.x < 0.f ? 1 : 0) | (p.x > 1.f ? 2 : 0) | (p.y < 0.f ? 4 : 0) |
             (p.y > 1.f ? 8 : 0);
    };

    const int ca = classify(a), cb = classify(b), cc = classify(c);

    if (0 == (ca | cb | cc)) {
      // all in center, no clipping
      return true;
    }

    const int code = ca & cb & cc;
    if ((code & 1) || (code & 2) || (code & 4) || (code & 8)) {
      // all outside one plane
      return false;
    }
  }

  // reject backfaces
  // note: they mess up the plane equation test below with inverted normals
  if (is_backface(a, b, c)) {
    return false;
  }

  // triangle edge distance rejection
  {
    struct plane2d_t {

      plane2d_t(const coord_t &a, const coord_t &b)
          : nx(b.y - a.y), ny(a.x - b.x), d(a.x * nx + a.y * ny) {
        // fixme: invert to correct normals
      }

      bool test(const coord_t &p) const { return d > (p.x * nx + p.y * ny); }

      // trivial rejection based on edge normal and closest box vertex
      bool trivial() const {
        switch ((nx > 0.f) | ((ny > 0.f) << 1)) {
        case 3: return d > (1.f * nx + 1.f * ny); // - nx  - ny
        case 2: return d > (0.f * nx + 1.f * ny); // + nx  - ny
        case 1: return d > (1.f * nx + 0.f * ny); // - nx  + ny
        case 0: return d > (0.f * nx + 0.f * ny); // + nx  + ny
        }
        return false; // keep compiler happy
      }

      float nx, ny, d;
    };

    const plane2d_t pab{a, b}, pbc{b, c}, pca{c, a};

    // perform trivial reject tests
    if (pab.trivial() || pbc.trivial() || pca.trivial()) {
      return false;
    }
  }

  // in, but needs clipping
  return true;
}


void bin_manager_t::_push_tri(uint32_t cell, uint32_t tri_index) {
}

bool bin_manager_t::_tri_intersect(const triangle_t &t, rect_t<float> &box) {
  return false;
}

void bin_manager_t::assign() {
  uint32_t counter = 0;
  for (const auto &t : triangle) {
    // cell space bounding box
    const auto r = tri_bound<int32_t>(t);
    const auto v = rect_t<int32_t>{
      std::max<int32_t>(r.x0 / group_size, 0),
      std::max<int32_t>(r.y0 / group_size, 0),
      std::min<int32_t>(r.x1 / group_size, cells_x-1),
      std::min<int32_t>(r.y1 / group_size, cells_y-1)
    };
    // dont be aggressive with very small triangles
    const bool is_small = is_small_tri(t);
    // loop over cells in bounding box
    for (int32_t x = v.x0; x <= v.x1; ++x) {
      for (int32_t y = v.y0; y <= v.y1; ++y) {
        // find this cell index
        const uint32_t cell_index = x + y * cells_x;
        // find this cell bounding box
        rect_t<float> box = {
          (x + 0) * group_size,
          (y + 0) * group_size,
          (x + 1) * group_size,
          (y + 1) * group_size
        };
        // push this intersection
        if (is_small || _tri_intersect(t, box)) {
          _push_tri(cell_index, counter);
        }
      }
    }
    ++counter;
  }
}

int main() {

  bin_manager_t bins;
  auto &t0 = bins.emplate();
  auto &t1 = bins.emplate();
  auto &t2 = bins.emplate();

  bins.assign();
  bins.clear();

  return 0;
}
