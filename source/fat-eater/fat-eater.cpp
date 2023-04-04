#include <cstdint>
#include <fat-eater/fat-eater.hpp>
#include <fat-eater/file8.3.hpp>
#include <fstream>
#include <memory>
#include <tabulate/table.hpp>
#include <thread>
#include <vector>

namespace fat_eater {
  FatEater::FatEater() : path("") {}

  void FatEater::load(const std::string& path) {
    this->path = path;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      fmt::print("Failed to open file: {}\n", path);
      return;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
      fmt::print("Failed to read file: {}\n", path);
      return;
    }

    this->image_header = std::make_unique<const FAT16_BootSector>(
        *reinterpret_cast<const FAT16_BootSector*>(buffer.data()));

    // FAT Table location variables
    uint32_t fat_table_start
        = this->image_header->reserved_sector_count * this->image_header->bytes_per_sector;
    uint32_t fat_table_end
        = fat_table_start
          + (this->image_header->fat_size_16 * this->image_header->bytes_per_sector);

    // Root directory location variables
    uint32_t root_directory_start
        = this->image_header->fat_count * (fat_table_end - fat_table_start) + fat_table_start;
    uint32_t root_directory_end
        = root_directory_start + (this->image_header->root_entry_count * 32);

    // Data region location variables
    uint32_t data_region_start = root_directory_end;
    uint32_t data_region_end = buffer.size();

    // Copy the FAT table
    this->fat_table = std::make_unique<std::vector<uint8_t>>(buffer.begin() + fat_table_start,
                                                             buffer.begin() + fat_table_end);

    // Copy the root directory
    this->root_directory = std::make_unique<std::vector<uint8_t>>(
        buffer.begin() + root_directory_start, buffer.begin() + root_directory_end);

    // Copy the data region
    this->data_region = std::make_unique<std::vector<uint8_t>>(buffer.begin() + data_region_start,
                                                               buffer.begin() + data_region_end);
  }

  FatEater* FatEater::print_header() {
    this->image_header->print();
    return this;
  }

  std::vector<FileFormat83> FatEater::list_files(bool print) {
    tabulate::Table table;
    table.add_row({"File Name", "File Extension", "File Size", "Attribute", "First Cluster"});

    std::vector<FileFormat83> files;

    auto [clusters, name] = this->directory_stack.back();

    // If the top element of the stack is first cluster 0, then we are in the root directory
    if (clusters.at(0) == 0)
      files = this->list_files_in_root_directory(table);
    else
      files = this->list_files_in_data_directory(table);

    table.column(3).format().multi_byte_characters(true);

    if (print) std::cout << table << std::endl;

    return files;
  }

  std::vector<FileFormat83> FatEater::list_files_in_root_directory(tabulate::Table& table) {
    std::vector<FileFormat83> files;

    for (uint32_t i = 0; i < this->image_header->root_entry_count; i++) {
      uint32_t offset = i * 32;

      // Read the long file name or 8.3 file name format
      if (this->root_directory->at(offset) == 0x00) {
        // End of directory
        break;
      } else if (this->root_directory->at(offset) == 0xE5) {
        // Deleted file
        continue;
      } else if (this->root_directory->at(offset) == 0x2E) {
        // Current directory
        continue;
      } else if (this->root_directory->at(offset + 11) == 0x0F) {
        // Long file name
        continue;
      } else {
        // 8.3 file name
        FileFormat83 file
            = *reinterpret_cast<const FileFormat83*>(this->root_directory->data() + offset);

        add_to_ls(file, table);
        files.push_back(file);
      }
    }

    return files;
  }

  std::vector<FileFormat83> FatEater::list_files_in_data_directory(tabulate::Table& table) {
    std::vector<FileFormat83> files;
    auto [clusters, name] = this->directory_stack.back();

    // Process for every cluster
    for (auto& cluster : clusters) {
      uint32_t index = 0;
      uint32_t cluster_offset = (cluster - 2) * this->image_header->bytes_per_sector
                                * this->image_header->sectors_per_cluster;

      while (true) {
        uint32_t offset = cluster_offset + (index * 32);

        // Read the long file name or 8.3 file name format
        if (this->data_region->at(offset) == 0x00) {
          // End of directory
          break;
        } else if (this->data_region->at(offset) == 0xE5) {
          // Deleted file
          index++;
          continue;
        } else if (this->data_region->at(offset) == 0x2E) {
          // Current directory
          FileFormat83 file
              = *reinterpret_cast<const FileFormat83*>(this->data_region->data() + offset);

          add_to_ls(file, table);
          files.push_back(file);
          index++;
          continue;
        } else if (this->data_region->at(offset + 11) == 0x0F) {
          // Long file name
          index++;
          continue;
        } else {
          // 8.3 file name
          FileFormat83 file
              = *reinterpret_cast<const FileFormat83*>(this->data_region->data() + offset);

          add_to_ls(file, table);
          files.push_back(file);
          index++;
          continue;
        }
      }
    }

    return files;
  }

  void FatEater::change_directory(const std::string& directory_name) {
    // Check if this directory exists
    // If it does, change the current directory offset
    auto current_directory_files = this->list_files(false);

    for (auto& file : current_directory_files) {
      // Print every character in the file name
      std::string file_name = normalize_directory_name(std::string{file.file_name});
      std::string normalized_directory_name = normalize_directory_name(directory_name);

      if (file_name == normalized_directory_name) {
        // Get the first cluster
        uint32_t first_cluster = file.first_cluster_low;

        std::vector<uint32_t> clusters{first_cluster};

        while (true) {
          auto fat_offset = clusters.back() * 2;
          uint32_t next_cluster = this->fat_table->at(fat_offset);

          if (next_cluster == 0x0FFFFFF8) {
            // End of file
            break;
          } else if (next_cluster == 0x0FFFFFFF) {
            // Bad cluster
            break;
          } else if (next_cluster == 0x0FFFFFF7) {
            // Reserved cluster
            break;
          } else if (next_cluster == 0x0) {
            // Unused cluster
            break;
          } else if (next_cluster == 0xFF) {
            // 0xFF is not a valid cluster
            break;
          } else {
            // Add the next cluster
            clusters.push_back(next_cluster);
          }
        }

        // If the file cluster is already in the stack, pop it
        if (std::find_if(
                this->directory_stack.begin(), this->directory_stack.end(),
                [&clusters](const auto& dir) { return dir.first.front() == clusters.front(); })
            != this->directory_stack.end()) {
          this->directory_stack.pop_back();
        } else {
          this->directory_stack.push_back({clusters, file_name});
        }

        this->repl.get()->show_penis()->prompt(fmt::format("fat-eater:{} ", this->pwd()));

        return;
      }
    }
  }

  void FatEater::cat(const std::string& file_name) {
    // Check if this directory exists
    // If it does, change the current directory offset
    auto current_directory_files = this->list_files(false);
    uint32_t selected_file_size = 0;

    for (auto& file : current_directory_files) {
      // Print every character in the file name
      std::string normalized_file_or_dir_name = normalize_file_name(std::string{file.file_name});
      std::string normalized_file_name = normalize_file_name(file_name);

      if (normalized_file_or_dir_name == normalized_file_name) {
        // Get the first cluster
        uint32_t first_cluster = file.first_cluster_low;
        selected_file_size = file.file_size;

        // Fetch all clusters
        std::vector<uint32_t> clusters{first_cluster};

        while (true) {
          auto fat_offset = clusters.back() * 2;
          uint32_t next_cluster = this->fat_table->at(fat_offset);

          if (next_cluster == 0x0FFFFFF8) {
            // End of file
            break;
          } else if (next_cluster == 0x0FFFFFFF) {
            // Bad cluster
            break;
          } else if (next_cluster == 0x0FFFFFF7) {
            // Reserved cluster
            break;
          } else if (next_cluster == 0x0) {
            // Unused cluster
            break;
          } else {
            // Add the next cluster
            clusters.push_back(next_cluster);
          }
        }

        std::vector<uint8_t> file_data;
        uint32_t bytes_read = 0;
        uint32_t cluster_size
            = this->image_header->sectors_per_cluster * this->image_header->bytes_per_sector;

        // Read the whole cluster if the file size - bytes read is greater than the cluster size
        // otherwise read the remaining bytes
        for (auto& cluster : clusters) {
          uint32_t offset = (cluster - 2) * this->image_header->sectors_per_cluster
                            * this->image_header->bytes_per_sector;

          auto diff = selected_file_size - bytes_read;
          int32_t to_read = diff > cluster_size ? cluster_size : diff;

          file_data.insert(file_data.end(), this->data_region->begin() + offset,
                           this->data_region->begin() + offset + to_read);
          bytes_read += to_read;
        }

        // Print the file data
        for (auto& byte : file_data) fmt::print("{:c}", byte);
        fmt::print("\n");

        return;
      }
    }
  }

  std::string FatEater::pwd() {
    std::vector<std::string> directory_names;
    for (auto& [clusters, name] : this->directory_stack) {
      directory_names.push_back(name);
    }

    return fmt::format("{}/", fmt::join(directory_names, "/"));
  }

  FatEater::~FatEater() {}
}  // namespace fat_eater
