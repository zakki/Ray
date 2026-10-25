#include "mcts/UctParameterFile.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
enum Encoding { LOG8 = 1, LOG16 = 2, FLOAT16 = 3 };

std::uint64_t LittleEndian(const unsigned char *bytes, std::size_t size) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < size; ++i) {
    value |= std::uint64_t(bytes[i]) << (i * 8);
  }
  return value;
}

void Read(std::istream &input, void *destination, std::size_t size) {
  if (!input.read(static_cast<char *>(destination), size)) {
    throw std::runtime_error("truncated UCT parameter file");
  }
}

std::uint32_t Read32(std::istream &input) {
  unsigned char bytes[4];
  Read(input, bytes, sizeof(bytes));
  return static_cast<std::uint32_t>(LittleEndian(bytes, sizeof(bytes)));
}

double ReadDouble(std::istream &input) {
  unsigned char bytes[8];
  Read(input, bytes, sizeof(bytes));
  const std::uint64_t bits = LittleEndian(bytes, sizeof(bytes));
  double value;
  static_assert(sizeof(value) == sizeof(bits), "binary64 double required");
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

std::vector<unsigned char> ReadColumn(std::istream &input, std::size_t rows,
                                      std::size_t width) {
  std::vector<unsigned char> bytes(rows * width);
  Read(input, bytes.data(), bytes.size());
  return bytes;
}

double HalfToDouble(std::uint16_t bits) {
  const int exponent = (bits >> 10) & 31;
  const int mantissa = bits & 1023;
  if (exponent == 31) throw std::runtime_error("non-finite float16 parameter");
  const double value = exponent == 0 ? std::ldexp(double(mantissa), -24)
                                     : std::ldexp(double(1024 + mantissa), exponent - 25);
  return (bits & 0x8000) ? -value : value;
}
}  // namespace

void LoadUctParameterFile(const char *path, const UctParameterTarget *targets,
                          std::size_t count) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot open UCT parameter file");
  char magic[8];
  Read(input, magic, sizeof(magic));
  if (std::memcmp(magic, "RAYUCT\0\0", sizeof(magic)) != 0 || Read32(input) != 1) {
    throw std::runtime_error("unsupported UCT parameter file version");
  }
  const std::uint32_t encoding = Read32(input);
  if (encoding != LOG8 && encoding != LOG16 && encoding != FLOAT16) {
    throw std::runtime_error("unsupported UCT parameter encoding");
  }
  if (Read32(input) != BTFM_DIMENSION + 1 || Read32(input) != count) {
    throw std::runtime_error("UCT parameter schema mismatch");
  }
  for (std::size_t section = 0; section < count; ++section) {
    const UctParameterTarget &target = targets[section];
    char name[32];
    Read(input, name, sizeof(name));
    const std::uint32_t rows = Read32(input);
    const std::uint32_t kind = Read32(input);
    const std::uint32_t expected_kind = target.md2_indices ? 1 : target.large_indices ? 2 : 0;
    if (name[31] != '\0' || std::strcmp(name, target.name) != 0 ||
        kind != expected_kind || rows == 0 || rows > target.capacity ||
        (kind == 0 && rows != target.capacity)) {
      throw std::runtime_error(std::string("invalid UCT parameter section: ") + target.name);
    }
    if (kind != 0) {
      const auto indices = ReadColumn(input, rows, 4);
      if (kind == 1) {
        std::fill(target.md2_indices, target.md2_indices + MD2_MAX, -1);
        for (std::size_t row = 0; row < rows; ++row) {
          const std::uint32_t index = static_cast<std::uint32_t>(LittleEndian(&indices[row * 4], 4));
          if (index >= MD2_MAX) throw std::runtime_error("MD2 pattern index out of range");
          unsigned int transposed[16];
          MD2Transpose16(index, transposed);
          for (const unsigned int equivalent : transposed) {
            target.md2_indices[equivalent] = static_cast<int>(row);
          }
        }
      } else {
        const auto hashes = ReadColumn(input, rows, 8);
        for (std::size_t i = 0; i < HASH_MAX; ++i) {
          target.large_indices[i].hash = 0;
          target.large_indices[i].index = -1;
        }
        for (std::size_t row = 0; row < rows; ++row) {
          const std::uint32_t index = static_cast<std::uint32_t>(LittleEndian(&indices[row * 4], 4));
          if (index >= HASH_MAX || target.large_indices[index].index != -1) {
            throw std::runtime_error("invalid large pattern index");
          }
          target.large_indices[index].hash = LittleEndian(&hashes[row * 8], 8);
          target.large_indices[index].index = static_cast<int>(row);
        }
      }
    }
    double minimum[BTFM_DIMENSION + 1] = {};
    double scale[BTFM_DIMENSION + 1] = {};
    if (encoding != FLOAT16) {
      for (int column = 0; column <= BTFM_DIMENSION; ++column) {
        minimum[column] = ReadDouble(input);
        scale[column] = ReadDouble(input);
        if (!std::isfinite(minimum[column]) || !std::isfinite(scale[column]) || scale[column] < 0) {
          throw std::runtime_error("invalid logarithmic quantization range");
        }
      }
    }
    const std::size_t width = encoding == LOG8 ? 1 : 2;
    for (int column = 0; column <= BTFM_DIMENSION; ++column) {
      const auto bytes = ReadColumn(input, rows, width);
      for (std::size_t row = 0; row < rows; ++row) {
        const std::uint16_t stored = static_cast<std::uint16_t>(LittleEndian(&bytes[row * width], width));
        const double value = encoding == FLOAT16 ? HalfToDouble(stored)
                             : std::exp(minimum[column] + stored * scale[column]);
        if (!std::isfinite(value) || value <= 0) {
          throw std::runtime_error("UCT parameters must be finite and positive");
        }
        if (column == 0) target.parameters[row].w = value;
        else target.parameters[row].v[column - 1] = value;
      }
    }
  }
  if (input.peek() != std::char_traits<char>::eof()) {
    throw std::runtime_error("unexpected trailing UCT parameter data");
  }
}
