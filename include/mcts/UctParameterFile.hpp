#ifndef RAY_MCTS_UCT_PARAMETER_FILE_HPP
#define RAY_MCTS_UCT_PARAMETER_FILE_HPP

#include <cstddef>
#include "mcts/UctRating.hpp"

// Targets are in the section order defined by the version 1 file format.
struct UctParameterTarget {
  const char *name;
  fm_t *parameters;
  std::size_t capacity;
  int *md2_indices;
  index_hash_t *large_indices;
};

// Decode at initialization; search continues to use the existing double values.
// Throws std::runtime_error on unsupported, truncated or invalid input.
void LoadUctParameterFile(const char *path, const UctParameterTarget *targets,
                          std::size_t count);

#endif
