#include "scene.h"

#include <cgltf.h>
#include <ranges>

namespace edge::world {
namespace detail {
void *cgltf_alloc_fn(void *user, const cgltf_size size) {
  return static_cast<Allocator *>(user)->malloc(size, 1);
}

void cgltf_free_fn(void *user, void *ptr) {
  static_cast<Allocator *>(user)->free(ptr);
}

cgltf_memory_options
create_cgltf_memory_options(const NotNull<const Allocator *> alloc) {
  return {.alloc_func = cgltf_alloc_fn,
          .free_func = cgltf_free_fn,
          .user_data = (void *)alloc.m_ptr};
}

cgltf_result cgltf_fread(const cgltf_memory_options *mopt,
                         const cgltf_file_options *fopt, const char *path,
                         cgltf_size *size, void **data) {
  FILE *stream = fopen(path, "rb");
  if (!stream) {
    return cgltf_result_io_error;
  }

  fseek(stream, 0, SEEK_END);
  cgltf_size stream_size = ftell(stream);
  fseek(stream, 0, SEEK_SET);

  *data = mopt->alloc_func(mopt->user_data, stream_size);
  if (!*data) {
    return cgltf_result_out_of_memory;
  }

  *size = stream_size;

  const usize readed_bytes = fread(*data, 1, stream_size, stream);
  if (readed_bytes != stream_size) {
    mopt->free_func(mopt->user_data, *data);
    *data = nullptr;
    *size = 0;
    fclose(stream);
    return cgltf_result_io_error;
  }

  fclose(stream);
  return cgltf_result_success;
}

void cgltf_frelease(const cgltf_memory_options *mopt,
                    const cgltf_file_options *fopt, void *data,
                    cgltf_size size) {
  if (data) {
    mopt->free_func(mopt->user_data, data);
  }
}

cgltf_file_options create_cgltf_file_options() {
  return {.read = cgltf_fread, .release = cgltf_frelease};
}

cgltf_options create_cgltf_options(const NotNull<const Allocator *> alloc) {
  return {.type = cgltf_file_type_invalid,
          .memory = create_cgltf_memory_options(alloc),
          .file = create_cgltf_file_options()};
}
} // namespace detail

bool Scene::import(const NotNull<const Allocator *> alloc,
                   const StringView<char8_t> path) {
  const cgltf_options options = detail::create_cgltf_options(alloc);

  cgltf_data *gltf_data = nullptr;
  cgltf_result result = cgltf_parse_file(
      &options, reinterpret_cast<const char *>(path.data()), &gltf_data);

  if (result != cgltf_result_success) {
    return false;
  }

  result = cgltf_validate(gltf_data);
  if (result != cgltf_result_success) {
    return false;
  }

  // TODO: Import assets. Need to implement filesystem first.

  cgltf_free(gltf_data);
  return true;
}
} // namespace edge::world