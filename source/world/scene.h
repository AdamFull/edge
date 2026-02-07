#ifndef EDGE_SCENE_H
#define EDGE_SCENE_H

#include <string_view.hpp>

namespace edge::world {
struct Scene {
  bool import(NotNull<const Allocator*> alloc, StringView<char8_t> path);
};
}

#endif
