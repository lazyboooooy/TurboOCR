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

#include "static_infer.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "dml_provider_factory.h"
#include "src/utils/ilogger.h"
#include "src/utils/utility.h"

namespace {

Ort::Env &GlobalOrtEnv() {
  static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "TurboOCR");
  return env;
}

size_t ShapeElementCount(const std::vector<int64_t> &shape) {
  size_t count = 1;
  for (auto d : shape) {
    if (d <= 0) {
      return 0;
    }
    count *= static_cast<size_t>(d);
  }
  return count;
}

std::vector<int> ShapeToInt(const std::vector<int64_t> &shape) {
  std::vector<int> result;
  result.reserve(shape.size());
  for (auto d : shape) {
    result.push_back(static_cast<int>(d));
  }
  return result;
}

cv::Mat MakeOutputMat(const std::vector<int64_t> &shape64) {
  std::vector<int> shape = ShapeToInt(shape64);
  if (shape.empty()) {
    return cv::Mat(1, 1, CV_32F);
  }
  if (shape.size() == 1) {
    return cv::Mat(1, shape[0], CV_32F);
  }
  return cv::Mat(static_cast<int>(shape.size()), shape.data(), CV_32F);
}

} // namespace

OrtInfer::OrtInfer(const std::string &model_name,
                   const std::string &model_dir,
                   const std::string &model_file_prefix,
                   const PaddlePredictorOption &option)
    : model_dir_(model_dir), model_file_prefix_(model_file_prefix),
      model_name_(model_name), option_(option) {
  auto status = Create();
  if (!status.ok()) {
    INFOE("Create ONNX Runtime session failed: %s",
          status.ToString().c_str());
    throw std::runtime_error(status.ToString());
  }
}

OrtInfer::~OrtInfer() = default;

absl::Status OrtInfer::Create() {
  const std::string model_path =
      model_dir_ + PATH_SEPARATOR + model_file_prefix_ + ".onnx";
  auto exists = Utility::FileExists(model_path);
  if (!exists.ok()) {
    return absl::NotFoundError("ONNX model not found: " + model_path);
  }

  std::ifstream file(model_path, std::ios::binary | std::ios::ate);
  if (!file) {
    return absl::NotFoundError("Failed to open ONNX model: " + model_path);
  }
  const std::streamsize file_size = file.tellg();
  if (file_size <= 0) {
    return absl::InvalidArgumentError("Empty ONNX model: " + model_path);
  }
  file.seekg(0, std::ios::beg);
  std::vector<char> model_data(static_cast<size_t>(file_size));
  if (!file.read(model_data.data(), file_size)) {
    return absl::InternalError("Failed to read ONNX model: " + model_path);
  }
  file.close();

  const bool want_dml = option_.DeviceType() == "gpu";

  auto create_session = [&](bool use_dml) -> std::unique_ptr<Ort::Session> {
    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    if (use_dml) {
      // DirectML EP；失败由上层捕获后退回 CPU。
      Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(
          options, option_.DeviceId()));
    } else {
      options.SetIntraOpNumThreads((std::max)(1, option_.CpuThreads()));
    }
    return std::make_unique<Ort::Session>(GlobalOrtEnv(), model_data.data(),
                                          model_data.size(), options);
  };

  try {
    if (want_dml) {
      try {
        session_ = create_session(true);
        using_dml_ = true;
      } catch (const std::exception &e) {
        INFOW("DirectML EP unavailable (%s), falling back to CPU EP", e.what());
        session_ = create_session(false);
        using_dml_ = false;
      }
    } else {
      session_ = create_session(false);
      using_dml_ = false;
    }
  } catch (const std::exception &e) {
    return absl::InternalError(std::string("Create ORT session failed: ") +
                               e.what());
  }

  Ort::AllocatorWithDefaultOptions allocator;
  const size_t input_count = session_->GetInputCount();
  input_names_.clear();
  input_types_.clear();
  input_names_.reserve(input_count);
  input_types_.reserve(input_count);
  for (size_t i = 0; i < input_count; ++i) {
    auto name = session_->GetInputNameAllocated(i, allocator);
    input_names_.emplace_back(name.get());
    auto type_info = session_->GetInputTypeInfo(i);
    input_types_.push_back(
        type_info.GetTensorTypeAndShapeInfo().GetElementType());
  }

  const size_t output_count = session_->GetOutputCount();
  output_names_.clear();
  output_types_.clear();
  output_names_.reserve(output_count);
  output_types_.reserve(output_count);
  for (size_t i = 0; i < output_count; ++i) {
    auto name = session_->GetOutputNameAllocated(i, allocator);
    output_names_.emplace_back(name.get());
    auto type_info = session_->GetOutputTypeInfo(i);
    output_types_.push_back(
        type_info.GetTensorTypeAndShapeInfo().GetElementType());
  }

  INFO("ONNX model loaded: %s (device=%s, inputs=%zu, outputs=%zu)",
       model_path.c_str(), using_dml_ ? "directml" : "cpu", input_names_.size(),
       output_names_.size());
  return absl::OkStatus();
}

absl::StatusOr<std::vector<cv::Mat>>
OrtInfer::Apply(const std::vector<cv::Mat> &x) {
  if (!session_) {
    return absl::FailedPreconditionError("ORT session is not initialized");
  }
  if (x.size() < input_names_.size()) {
    return absl::InvalidArgumentError("Not enough input tensors");
  }

  Ort::MemoryInfo memory_info =
      Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  std::vector<Ort::Value> input_values;
  input_values.reserve(input_names_.size());

  // 保持转换后的数据存活到 Run() 结束。
  std::vector<cv::Mat> converted_mats;
  converted_mats.reserve(x.size());
  std::vector<std::vector<int64_t>> int64_buffers;
  int64_buffers.reserve(x.size());

  for (size_t i = 0; i < input_names_.size(); ++i) {
    const cv::Mat &mat = x[i];
    if (mat.empty()) {
      return absl::InvalidArgumentError("Empty input image");
    }

    std::vector<int64_t> shape;
    shape.reserve(static_cast<size_t>(mat.dims));
    for (int d = 0; d < mat.dims; ++d) {
      shape.push_back(static_cast<int64_t>(mat.size[d]));
    }
    const size_t count = mat.total();
    const ONNXTensorElementDataType type =
        i < input_types_.size() ? input_types_[i]
                                : ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;

    switch (type) {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: {
      const float *data = nullptr;
      if (mat.type() == CV_32F && mat.isContinuous()) {
        data = mat.ptr<float>();
      } else {
        cv::Mat f;
        mat.convertTo(f, CV_32F);
        if (!f.isContinuous()) {
          f = f.clone();
        }
        converted_mats.push_back(f);
        data = converted_mats.back().ptr<float>();
      }
      input_values.push_back(Ort::Value::CreateTensor<float>(
          memory_info, const_cast<float *>(data), count, shape.data(),
          shape.size()));
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: {
      const int *data = nullptr;
      if (mat.type() == CV_32S && mat.isContinuous()) {
        data = mat.ptr<int>();
      } else {
        cv::Mat i32;
        mat.convertTo(i32, CV_32S);
        if (!i32.isContinuous()) {
          i32 = i32.clone();
        }
        converted_mats.push_back(i32);
        data = converted_mats.back().ptr<int>();
      }
      input_values.push_back(Ort::Value::CreateTensor<int32_t>(
          memory_info, const_cast<int32_t *>(
                           reinterpret_cast<const int32_t *>(data)),
          count, shape.data(), shape.size()));
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: {
      std::vector<int64_t> buffer(count);
      if (mat.type() == CV_32S) {
        const int *p = mat.ptr<int>();
        for (size_t k = 0; k < count; ++k) {
          buffer[k] = static_cast<int64_t>(p[k]);
        }
      } else if (mat.type() == CV_32F) {
        const float *p = mat.ptr<float>();
        for (size_t k = 0; k < count; ++k) {
          buffer[k] = static_cast<int64_t>(p[k]);
        }
      } else if (mat.type() == CV_64F) {
        const double *p = mat.ptr<double>();
        for (size_t k = 0; k < count; ++k) {
          buffer[k] = static_cast<int64_t>(p[k]);
        }
      } else {
        return absl::InvalidArgumentError("Unsupported int64 input type");
      }
      int64_buffers.push_back(std::move(buffer));
      input_values.push_back(Ort::Value::CreateTensor<int64_t>(
          memory_info, int64_buffers.back().data(), count, shape.data(),
          shape.size()));
      break;
    }
    default:
      return absl::InvalidArgumentError("Unsupported ONNX input data type");
    }
  }

  std::vector<const char *> input_names;
  input_names.reserve(input_names_.size());
  for (const auto &name : input_names_) {
    input_names.push_back(name.c_str());
  }
  std::vector<const char *> output_names;
  output_names.reserve(output_names_.size());
  for (const auto &name : output_names_) {
    output_names.push_back(name.c_str());
  }

  std::vector<Ort::Value> outputs;
  try {
    outputs = session_->Run(Ort::RunOptions{nullptr}, input_names.data(),
                            input_values.data(), input_values.size(),
                            output_names.data(), output_names.size());
  } catch (const std::exception &e) {
    return absl::InternalError(std::string("ORT Run failed: ") + e.what());
  }

  std::vector<cv::Mat> result;
  result.reserve(outputs.size());
  for (auto &output : outputs) {
    auto info = output.GetTensorTypeAndShapeInfo();
    const std::vector<int64_t> shape64 = info.GetShape();
    const ONNXTensorElementDataType type = info.GetElementType();
    cv::Mat out_mat = MakeOutputMat(shape64);
    const size_t count = ShapeElementCount(shape64);
    if (count == 0 && !shape64.empty()) {
      return absl::InternalError("Invalid ONNX output shape");
    }
    const size_t actual_count = count == 0 ? 1 : count;
    if (type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
      std::memcpy(out_mat.ptr<float>(), output.GetTensorData<float>(),
                  actual_count * sizeof(float));
    } else if (type == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64) {
      const int64_t *p = output.GetTensorData<int64_t>();
      float *dst = out_mat.ptr<float>();
      for (size_t k = 0; k < actual_count; ++k) {
        dst[k] = static_cast<float>(p[k]);
      }
    } else if (type == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32) {
      const int32_t *p = output.GetTensorData<int32_t>();
      float *dst = out_mat.ptr<float>();
      for (size_t k = 0; k < actual_count; ++k) {
        dst[k] = static_cast<float>(p[k]);
      }
    } else {
      return absl::InvalidArgumentError("Unsupported ONNX output data type");
    }
    result.push_back(std::move(out_mat));
  }

  return result;
}
