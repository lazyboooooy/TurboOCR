// Copyright (c) 2025 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <map>
#include <string>
#include <vector>

/// @brief Lightweight INI file parser (C++11, no external dependencies).
///
/// Supports:
///   [section]   — section headers
///   key = value  — key-value pairs (spaces around = are trimmed)
///   # comment    — comment lines (also ;)
///   blank lines  — ignored
///
/// Usage:
///   IniConfig ini;
///   ini.LoadFromFile("config.ini");
///   std::string val = ini.Get("v6", "text_det_thresh", "0.3");
///   std::string abs_path = ini.ResolvePath("models/PP-OCRv5_server_det");
class IniConfig {
public:
  IniConfig() = default;

  /// Load and parse an INI file. Returns false if the file cannot be opened.
  /// Stores the file's directory for relative path resolution.
  bool LoadFromFile(const std::string &file_path);

  /// Check whether a section exists.
  bool HasSection(const std::string &section) const;

  /// Get a value. Returns default_val if section/key not found.
  std::string Get(const std::string &section, const std::string &key,
                  const std::string &default_val = "") const;

  /// Get all section names (in file order).
  std::vector<std::string> GetSections() const;

  /// Check if the config is empty (no file loaded).
  bool Empty() const { return data_.empty(); }

  /// Get the directory containing the loaded INI file.
  const std::string &BaseDir() const { return base_dir_; }

  /// Resolve a path: if relative, make it absolute based on the INI file's
  /// directory. If already absolute (e.g. "C:\..." or "/..."), return as-is.
  std::string ResolvePath(const std::string &path) const;

private:
  // data_[section][key] = value
  std::map<std::string, std::map<std::string, std::string>> data_;
  std::vector<std::string> section_order_;
  std::string base_dir_;

  static std::string Trim(const std::string &s);
  static bool IsAbsolutePath(const std::string &path);
};
