#pragma once

#include <fat-eater/fat16.hpp>
#include <fat-eater/file8.3.hpp>
#include <istream>
#include <memory>
#include <prepucio/repl.hpp>
#include <string_view>
#include <tabulate/table.hpp>
#include <vector>

namespace fat_eater {
  class FatEater {
  private:
    // The file bytes
    std::unique_ptr<std::istream> file;

    // The path to the file
    std::string_view path;

    // The image header
    std::unique_ptr<const FAT16_BootSector> image_header;

    // The FAT table
    std::unique_ptr<std::vector<uint8_t>> fat_table;

    // The root directory
    std::unique_ptr<std::vector<uint8_t>> root_directory;

    // The data region
    std::unique_ptr<std::vector<uint8_t>> data_region;

    // Store the current directory offset in memory
    using clusters = std::vector<uint32_t>;
    using directory_name = std::string;
    std::vector<std::pair<clusters, directory_name>> directory_stack{{{0}, ""}};

    prepucio::REPL::Builder builder;
    std::unique_ptr<prepucio::REPL> repl;

  public:
    explicit FatEater();
    explicit FatEater(const std::string& path)
        : repl(builder
                   .addCommand("ls", "List files from root directory",
                               [this]() { this->list_files(); })
                   .addCommand("cd", "Change directory",
                               [this](std::string directory) { this->change_directory(directory); })
                   .addCommand("cat", "Print file contents",
                               [this](std::string file) { this->cat(file); })
                   .addCommand("header", "Print FAT16 header", [this]() { this->print_header(); })
                   .addCommand("pwd", "Print current directory", [this]() { fmt::print("{}\n", this->pwd()); })
                   .build()),
          path(path) {
      this->load(path);
      this->repl.get()->show_penis()->prompt("fat-eater:/ ");
      this->repl->run();
    }

    void load(const std::string& path);
    FatEater* print_header();
    std::vector<FileFormat83> list_files(bool print = true);
    void change_directory(const std::string& directory_name);
    void cat(const std::string& file_name);
    std::string pwd();

    ~FatEater();

  private:
    std::vector<FileFormat83> list_files_in_root_directory(tabulate::Table& table);
    std::vector<FileFormat83> list_files_in_data_directory(tabulate::Table& table);
  };
}  // namespace fat_eater
