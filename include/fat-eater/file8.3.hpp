#pragma once

#include <fmt/core.h>

#include <cstdint>
#include <string_view>
#include <tabulate/table.hpp>

namespace fat_eater {
#pragma pack(push, 1)  // ensure that the structure is packed
  struct FileFormat83 {
    char file_name[8];
    char file_extension[3];
    uint8_t attributes;
    uint16_t reserved;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
  };
#pragma pack(pop)  // restore original packing

  inline void add_to_ls(const FileFormat83& file, tabulate::Table& table) {
    std::string file_name{file.file_name, 8};
    std::string file_extension{file.file_extension, 3};
    std::string file_size{std::to_string(file.file_size)};
    std::string attributes = "";
    std::string first_cluster = fmt::format("{:04X}", file.first_cluster_low);

    if (file.attributes & 0x01) attributes += "󱤀  ";
    if (file.attributes & 0x02) attributes += "󰘓 ";
    if (file.attributes & 0x04) attributes += " ";
    if (file.attributes & 0x08) attributes += " ";
    if (file.attributes & 0x10) attributes += " ";
    if (file.attributes & 0x20) attributes += " ";

    table.add_row({file_name, file_extension, file_size, attributes, first_cluster});
  }

  inline std::string remove_0x10_0x20(const std::string& str) {
    std::string result = "";
    for (char c : str) {
      if (c != 0x10 && c != 0x20) {
        result += c;
      }
    }
    return result;
  }

  inline std::string normalize_directory_name(const std::string& directory_name) {
    // Lower case and trim the file name
    std::string normalized_directory_name = directory_name;
    std::transform(normalized_directory_name.begin(), normalized_directory_name.end(),
                   normalized_directory_name.begin(), ::tolower);
    normalized_directory_name = remove_0x10_0x20(normalized_directory_name);
    return normalized_directory_name;
  }

  inline std::string normalize_file_name(const std::string& file_name) {
    // Remove any . from the file name
    std::string normalized_file_name = file_name;
    normalized_file_name.erase(
        std::remove(normalized_file_name.begin(), normalized_file_name.end(), '.'),
        normalized_file_name.end());
    return normalize_directory_name(normalized_file_name);
  }
}  // namespace fat_eater
