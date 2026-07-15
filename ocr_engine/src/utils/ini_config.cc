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

#include "ini_config.h"

#include <algorithm>
#include <cctype>
#include <fstream>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define PATH_SEPARATOR '\\'
#else
#include <unistd.h>
#define PATH_SEPARATOR '/'
#endif

std::string IniConfig::Trim(const std::string &s) {
  auto start = s.begin();
  while (start != s.end() && std::isspace(static_cast<unsigned char>(*start)))
    ++start;
  auto end = s.end();
  while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1))))
    --end;
  return std::string(start, end);
}

bool IniConfig::IsAbsolutePath(const std::string &path) {
  if (path.empty()) return false;
#ifdef _WIN32
  // "C:\..." or "C:/..." or "\\server\..."
  if (path.size() >= 2 && path[1] == ':') return true;
  if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') return true;
  return false;
#else
  return path[0] == '/' || path[0] == '~';
#endif
}

bool IniConfig::LoadFromFile(const std::string &file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) return false;

  // Store base directory
  size_t pos = file_path.find_last_of("\\/");
  if (pos != std::string::npos) {
    base_dir_ = file_path.substr(0, pos);
  } else {
    base_dir_ = ".";
  }

  data_.clear();
  section_order_.clear();

  std::string line;
  std::string current_section;

  while (std::getline(file, line)) {
    std::string trimmed = Trim(line);

    if (trimmed.empty()) continue;
    if (trimmed[0] == '#' || trimmed[0] == ';') continue;

    // Section header [section]
    if (trimmed[0] == '[') {
      size_t end = trimmed.find(']');
      if (end != std::string::npos) {
        current_section = Trim(trimmed.substr(1, end - 1));
        section_order_.push_back(current_section);
      }
      continue;
    }

    // Key = value
    size_t eq = trimmed.find('=');
    if (eq != std::string::npos && !current_section.empty()) {
      std::string key = Trim(trimmed.substr(0, eq));
      std::string val = Trim(trimmed.substr(eq + 1));

      // Remove inline comment only if preceded by whitespace
      size_t comment = val.find(" #");
      if (comment == std::string::npos) comment = val.find("\t#");
      if (comment == std::string::npos) comment = val.find(" ;");
      if (comment == std::string::npos) comment = val.find("\t;");
      if (comment != std::string::npos) {
        val = Trim(val.substr(0, comment));
      }

      data_[current_section][key] = val;
    }
  }

  return true;
}

bool IniConfig::HasSection(const std::string &section) const {
  return data_.find(section) != data_.end();
}

std::string IniConfig::Get(const std::string &section, const std::string &key,
                           const std::string &default_val) const {
  auto sec = data_.find(section);
  if (sec == data_.end()) return default_val;
  auto kv = sec->second.find(key);
  if (kv == sec->second.end()) return default_val;
  return kv->second;
}

std::vector<std::string> IniConfig::GetSections() const {
  return section_order_;
}

std::string IniConfig::ResolvePath(const std::string &path) const {
  if (path.empty() || IsAbsolutePath(path)) return path;
  if (base_dir_.empty() || base_dir_ == ".") return path;

  std::string resolved = base_dir_;
  if (resolved.back() != PATH_SEPARATOR && resolved.back() != '/') {
    resolved += PATH_SEPARATOR;
  }
  resolved += path;

  // Normalize double separators
  for (size_t i = 0; i + 1 < resolved.size(); ) {
    if ((resolved[i] == '/' || resolved[i] == '\\') &&
        (resolved[i + 1] == '/' || resolved[i + 1] == '\\')) {
      resolved.erase(i, 1);
    } else {
      ++i;
    }
  }
  return resolved;
}
