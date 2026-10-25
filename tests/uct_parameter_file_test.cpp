#include "mcts/UctParameterFile.hpp"
#include <cstdio>
#include <exception>
#include <iostream>

// Small coefficient arrays with real index-table dimensions exercise both mappings.
static fm_t dense[2], md2[2], md3[2];
static int md2_indices[MD2_MAX];
static index_hash_t md3_indices[HASH_MAX];

int main(int argc, char **argv) {
  if (argc != 3) return 2;
  const UctParameterTarget targets[] = {
    {"Dense.txt", dense, 2, nullptr, nullptr},
    {"MD2.txt", md2, 2, md2_indices, nullptr},
    {"MD3.txt", md3, 2, nullptr, md3_indices},
  };
  try {
    LoadUctParameterFile(argv[1], targets, 3);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  FILE *output = std::fopen(argv[2], "wb");
  if (!output) return 2;
  for (const fm_t *section : {dense, md2, md3}) {
    for (int row = 0; row < 2; ++row) {
      std::fwrite(&section[row].w, sizeof(double), 1, output);
      std::fwrite(section[row].v, sizeof(double), BTFM_DIMENSION, output);
    }
  }
  for (const int index : {0, MD2_MAX - 1, 1}) {
    std::fwrite(&md2_indices[index], sizeof(int), 1, output);
  }
  for (const int index : {0, HASH_MAX - 1}) {
    std::fwrite(&md3_indices[index].hash, sizeof(unsigned long long), 1, output);
    std::fwrite(&md3_indices[index].index, sizeof(int), 1, output);
  }
  std::fclose(output);
}
