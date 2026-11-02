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

#include <memory>
#include <string>
#include <vector>

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/utils/ilogger.h"
#include "src/utils/pp_option.h"

/// ONNX Runtime 推理封装。
/// 模型文件为 <model_dir>/<model_file_prefix>.onnx。
/// device=gpu 时使用 DirectML EP，创建失败会自动回退 CPU EP。
class OrtInfer {
public:
  OrtInfer(const std::string &model_name,
           const std::string &model_dir,
           const std::string &model_file_prefix,
           const PaddlePredictorOption &option);
  ~OrtInfer();

  absl::StatusOr<std::vector<cv::Mat>> Apply(const std::vector<cv::Mat> &x);

  bool usingDml() const { return using_dml_; }

private:
  absl::Status Create();

  std::string model_dir_;
  std::string model_file_prefix_;
  std::string model_name_;
  PaddlePredictorOption option_;

  std::unique_ptr<Ort::Session> session_;
  std::vector<std::string> input_names_;
  std::vector<std::string> output_names_;
  std::vector<ONNXTensorElementDataType> input_types_;
  std::vector<ONNXTensorElementDataType> output_types_;
  bool using_dml_ = false;
};
