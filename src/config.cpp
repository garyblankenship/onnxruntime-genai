// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Modifications Copyright(C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
#include "generators.h"
#include "runtime_settings.h"
#include "json.h"
#include <fstream>
#include <sstream>

namespace Generators {

// Fix casing of certain historical names to match current Onnxruntime names
std::string_view NormalizeProviderName(std::string_view name) {
  if (name == "qnn") {
    return "QNN";
  } else if (name == "webgpu") {
    return "WebGPU";
  } else if (name == "dml") {
    return "DML";
  }
  return name;  // Return name unchanged
}
ONNXTensorElementDataType TranslateTensorType(std::string_view value) {
  if (value == "float32") {
    return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
  }
  if (value == "float16") {
    return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16;
  }
  throw std::runtime_error("Invalid tensor type: " + std::string(value));
}

struct NamedStrings_Element : JSON::Element {
  explicit NamedStrings_Element(std::vector<Config::NamedString>& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    v_.emplace_back(name, JSON::Get<std::string_view>(value));
  }

 private:
  std::vector<Config::NamedString>& v_;
};

struct Int_Array_Element : JSON::Element {
  explicit Int_Array_Element(std::vector<int>& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    v_.emplace_back(static_cast<int>(JSON::Get<double>(value)));
  }

 private:
  std::vector<int>& v_;
};

struct ProviderOptionsObject_Element : JSON::Element {
  explicit ProviderOptionsObject_Element(std::vector<Config::ProviderOptions>& v) : v_{v} {}

  JSON::Element& OnObject(std::string_view name) override {
    for (auto& v : v_) {
      if (v.name == name) {
        options_element_ = std::make_unique<NamedStrings_Element>(v.options);
        return *options_element_;
      }
    }

    auto& options = v_.emplace_back();
    options.name = name;
    options_element_ = std::make_unique<NamedStrings_Element>(options.options);
    return *options_element_;
  }

 private:
  std::vector<Config::ProviderOptions>& v_;
  std::unique_ptr<NamedStrings_Element> options_element_;
};

struct ProviderOptionsArray_Element : JSON::Element {
  explicit ProviderOptionsArray_Element(std::vector<Config::ProviderOptions>& v) : v_{v} {}

  JSON::Element& OnObject(std::string_view name) override { return object_; }

  void OnComplete(bool /*empty*/) override {
    // For backwards compatibility turn our old names like 'qnn' into 'QNN', and 'webgpu' to 'WebGPU'
    for (auto& v : v_) {
      v.name = NormalizeProviderName(v.name);
    }
  }

 private:
  std::vector<Config::ProviderOptions>& v_;
  ProviderOptionsObject_Element object_{v_};
};

GraphOptimizationLevel GetGraphOptimizationLevel(std::string_view name) {
  if (name == "ORT_DISABLE_ALL") {
    return ORT_DISABLE_ALL;
  } else if (name == "ORT_ENABLE_BASIC") {
    return ORT_ENABLE_BASIC;
  } else if (name == "ORT_ENABLE_EXTENDED") {
    return ORT_ENABLE_EXTENDED;
  } else if (name == "ORT_ENABLE_ALL") {
    return ORT_ENABLE_ALL;
  } else {
    throw std::runtime_error("Unrecognized value:" + std::string(name));
  }
}

struct SessionOptions_Element : JSON::Element {
  explicit SessionOptions_Element(Config::SessionOptions& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "log_id") {
      v_.log_id = JSON::Get<std::string_view>(value);
    } else if (name == "enable_profiling") {
      v_.enable_profiling = JSON::Get<std::string_view>(value);
    } else if (name == "ep_context_embed_mode") {
      v_.ep_context_embed_mode = JSON::Get<std::string_view>(value);
    } else if (name == "ep_context_file_path") {
      v_.ep_context_file_path = JSON::Get<std::string_view>(value);
    } else if (name == "intra_op_num_threads") {
      v_.intra_op_num_threads = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "inter_op_num_threads") {
      v_.inter_op_num_threads = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "log_severity_level") {
      v_.log_severity_level = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "enable_cpu_mem_arena") {
      v_.enable_cpu_mem_arena = JSON::Get<bool>(value);
    } else if (name == "enable_mem_pattern") {
      v_.enable_mem_pattern = JSON::Get<bool>(value);
    } else if (name == "disable_cpu_ep_fallback") {
      v_.disable_cpu_ep_fallback = JSON::Get<bool>(value);
    } else if (name == "disable_quant_qdq") {
      v_.disable_quant_qdq = JSON::Get<bool>(value);
    } else if (name == "enable_quant_qdq_cleanup") {
      v_.enable_quant_qdq_cleanup = JSON::Get<bool>(value);
    } else if (name == "ep_context_enable") {
      v_.ep_context_enable = JSON::Get<bool>(value);
    } else if (name == "use_env_allocators") {
      v_.use_env_allocators = JSON::Get<bool>(value);
    } else if (name == "graph_optimization_level") {
      v_.graph_optimization_level = GetGraphOptimizationLevel(JSON::Get<std::string_view>(value));
    } else if (name == "custom_ops_library") {
      v_.custom_ops_library = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

  JSON::Element& OnObject(std::string_view name) override {
    if (name == "config_entries")
      return config_entries_;
    throw JSON::unknown_value_error{};
  }

  JSON::Element& OnArray(std::string_view name) override {
    if (name == "provider_options") {
      return provider_options_;
    }
    throw JSON::unknown_value_error{};
  }

 private:
  Config::SessionOptions& v_;
  ProviderOptionsArray_Element provider_options_{v_.provider_options};
  NamedStrings_Element config_entries_{v_.config_entries};
};

struct EncoderInputs_Element : JSON::Element {
  explicit EncoderInputs_Element(Config::Model::Encoder::Inputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "input_ids") {
      v_.input_ids = JSON::Get<std::string_view>(value);
    } else if (name == "inputs_embeds") {
      v_.embeddings = JSON::Get<std::string_view>(value);
    } else if (name == "attention_mask") {
      v_.attention_mask = JSON::Get<std::string_view>(value);
    } else if (name == "position_ids") {
      v_.position_ids = JSON::Get<std::string_view>(value);
    } else if (name == "audio_features") {
      v_.audio_features = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Encoder::Inputs& v_;
};

struct EncoderOutputs_Element : JSON::Element {
  explicit EncoderOutputs_Element(Config::Model::Encoder::Outputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "encoder_hidden_states") {
      v_.hidden_states = JSON::Get<std::string_view>(value);
    } else if (name == "encoder_outputs") {
      v_.encoder_outputs = JSON::Get<std::string_view>(value);
    } else if (name == "cross_present_key_names") {
      v_.cross_present_key_names = JSON::Get<std::string_view>(value);
    } else if (name == "cross_present_value_names") {
      v_.cross_present_value_names = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Encoder::Outputs& v_;
};

struct DecoderInputs_Element : JSON::Element {
  explicit DecoderInputs_Element(Config::Model::Decoder::Inputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "input_ids") {
      v_.input_ids = JSON::Get<std::string_view>(value);
    } else if (name == "inputs_embeds") {
      v_.embeddings = JSON::Get<std::string_view>(value);
    } else if (name == "attention_mask") {
      v_.attention_mask = JSON::Get<std::string_view>(value);
    } else if (name == "position_ids") {
      v_.position_ids = JSON::Get<std::string_view>(value);
    } else if (name == "past_key_names") {
      v_.past_key_names = JSON::Get<std::string_view>(value);
    } else if (name == "past_value_names") {
      v_.past_value_names = JSON::Get<std::string_view>(value);
    } else if (name == "past_names") {
      v_.past_names = JSON::Get<std::string_view>(value);
    } else if (name == "cross_past_key_names") {
      v_.cross_past_key_names = JSON::Get<std::string_view>(value);
    } else if (name == "cross_past_value_names") {
      v_.cross_past_value_names = JSON::Get<std::string_view>(value);
    } else if (name == "past_sequence_length") {
      v_.past_sequence_length = JSON::Get<std::string_view>(value);
    } else if (name == "current_sequence_length") {
      v_.current_sequence_length = JSON::Get<std::string_view>(value);
    } else if (name == "total_sequence_length") {
      v_.total_sequence_length = JSON::Get<std::string_view>(value);
    } else if (name == "encoder_hidden_states") {
      v_.encoder_hidden_states = JSON::Get<std::string_view>(value);
    } else if (name == "encoder_attention_mask") {
      v_.encoder_attention_mask = JSON::Get<std::string_view>(value);
    } else if (name == "rnn_states_prev") {
      v_.rnn_prev_states = JSON::Get<std::string_view>(value);
    } else if (name == "past_key_values_length") {
      v_.past_key_values_length = JSON::Get<std::string_view>(value);
    } else if (name == "cache_indirection") {
      v_.cache_indirection = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Decoder::Inputs& v_;
};

struct DecoderOutputs_Element : JSON::Element {
  explicit DecoderOutputs_Element(Config::Model::Decoder::Outputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "logits") {
      v_.logits = JSON::Get<std::string_view>(value);
    } else if (name == "present_key_names") {
      v_.present_key_names = JSON::Get<std::string_view>(value);
    } else if (name == "present_value_names") {
      v_.present_value_names = JSON::Get<std::string_view>(value);
    } else if (name == "present_names") {
      v_.present_names = JSON::Get<std::string_view>(value);
    } else if (name == "output_cross_qk_names") {
      v_.output_cross_qk_names = JSON::Get<std::string_view>(value);
    } else if (name == "rnn_states") {
      v_.rnn_states = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Decoder::Outputs& v_;
};

struct StringArray_Element : JSON::Element {
  explicit StringArray_Element(std::vector<std::string>& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    v_.push_back(std::string{JSON::Get<std::string_view>(value)});
  }

 private:
  std::vector<std::string>& v_;
};

struct StringStringMap_Element : JSON::Element {
  explicit StringStringMap_Element(std::unordered_map<std::string, std::string>& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    v_[std::string(name)] = std::string(JSON::Get<std::string_view>(value));
  }

 private:
  std::unordered_map<std::string, std::string>& v_;
};

struct PipelineModel_Element : JSON::Element {
  explicit PipelineModel_Element(Config::Model::Decoder::PipelineModel& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "filename") {
      v_.filename = JSON::Get<std::string_view>(value);
    } else if (name == "run_on_prompt") {
      v_.run_on_prompt = JSON::Get<bool>(value);
    } else if (name == "run_on_token_gen") {
      v_.run_on_token_gen = JSON::Get<bool>(value);
    } else if (name == "reset_session_idx") {
      v_.reset_session_idx = static_cast<int>(JSON::Get<double>(value));
    } else {
      throw JSON::unknown_value_error{};
    }
  }

  JSON::Element& OnObject(std::string_view name) override {
    if (name == "session_options") {
      v_.session_options = Config::SessionOptions{};
      session_options_ = std::make_unique<SessionOptions_Element>(*v_.session_options);
      return *session_options_;
    } else if (name == "output_names_forwarder") {
      return output_names_forwarder_;
    }
    throw JSON::unknown_value_error{};
  }

  Element& OnArray(std::string_view name) override {
    if (name == "inputs") {
      return inputs_;
    } else if (name == "outputs") {
      return outputs_;
    }
    throw JSON::unknown_value_error{};
  }

 private:
  Config::Model::Decoder::PipelineModel& v_;
  std::unique_ptr<SessionOptions_Element> session_options_;
  StringArray_Element inputs_{v_.inputs};
  StringArray_Element outputs_{v_.outputs};
  StringStringMap_Element output_names_forwarder_{v_.output_names_forwarder};
};

struct PipelineModelObject_Element : JSON::Element {
  explicit PipelineModelObject_Element(std::vector<Config::Model::Decoder::PipelineModel>& v) : v_{v} {}

  Element& OnObject(std::string_view name) override {
    auto& model = v_.emplace_back();
    model.model_id = name;
    pipeline_model_elements_.emplace_back(model);
    return pipeline_model_elements_.back();
  }

 private:
  std::vector<Config::Model::Decoder::PipelineModel>& v_;
  std::vector<PipelineModel_Element> pipeline_model_elements_;
};

struct Pipeline_Element : JSON::Element {
  explicit Pipeline_Element(std::vector<Config::Model::Decoder::PipelineModel>& v) : v_{v} {}

  Element& OnObject(std::string_view name) override {
    return object_;
  }

 private:
  std::vector<Config::Model::Decoder::PipelineModel>& v_;
  PipelineModelObject_Element object_{v_};
};

struct SlidingWindow_Element : JSON::Element {
  explicit SlidingWindow_Element(std::optional<Config::Model::Decoder::SlidingWindow>& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "window_size") {
      v_->window_size = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "pad_value") {
      v_->pad_value = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "alignment") {
      v_->alignment = JSON::Get<std::string_view>(value);
    } else if (name == "slide_key_value_cache") {
      v_->slide_key_value_cache = JSON::Get<bool>(value);
    } else if (name == "slide_inputs") {
      v_->slide_inputs = JSON::Get<bool>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  std::optional<Config::Model::Decoder::SlidingWindow>& v_;
};

struct Encoder_Element : JSON::Element {
  explicit Encoder_Element(Config::Model::Encoder& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "filename") {
      v_.filename = JSON::Get<std::string_view>(value);
    } else if (name == "hidden_size") {
      v_.hidden_size = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "num_attention_heads") {
      v_.num_attention_heads = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "num_hidden_layers") {
      v_.num_hidden_layers = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "num_key_value_heads") {
      v_.num_key_value_heads = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "head_size") {
      v_.head_size = static_cast<int>(JSON::Get<double>(value));
    } else {
      throw JSON::unknown_value_error{};
    }
  }

  Element& OnObject(std::string_view name) override {
    if (name == "session_options") {
      return session_options_;
    }
    if (name == "inputs") {
      return inputs_;
    }
    if (name == "outputs") {
      return outputs_;
    }
    throw JSON::unknown_value_error{};
  }

 private:
  Config::Model::Encoder& v_;
  SessionOptions_Element session_options_{v_.session_options};
  EncoderInputs_Element inputs_{v_.inputs};
  EncoderOutputs_Element outputs_{v_.outputs};
};

struct Decoder_Element : JSON::Element {
  explicit Decoder_Element(Config::Model::Decoder& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "filename") {
      v_.filename = JSON::Get<std::string_view>(value);
    } else if (name == "hidden_size") {
      v_.hidden_size = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "num_attention_heads") {
      v_.num_attention_heads = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "num_key_value_heads") {
      v_.num_key_value_heads = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "num_hidden_layers") {
      v_.num_hidden_layers = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "head_size") {
      v_.head_size = static_cast<int>(JSON::Get<double>(value));
    } else {
      throw JSON::unknown_value_error{};
    }
  }

  Element& OnObject(std::string_view name) override {
    if (name == "session_options") {
      return session_options_;
    }
    if (name == "inputs") {
      return inputs_;
    }
    if (name == "outputs") {
      return outputs_;
    }
    if (name == "sliding_window") {
      v_.sliding_window = Config::Model::Decoder::SlidingWindow{};
      return sliding_window_;
    }
    throw JSON::unknown_value_error{};
  }

  Element& OnArray(std::string_view name) override {
    if (name == "pipeline") {
      return pipeline_;
    }
    throw JSON::unknown_value_error{};
  }

 private:
  Config::Model::Decoder& v_;
  SessionOptions_Element session_options_{v_.session_options};
  DecoderInputs_Element inputs_{v_.inputs};
  DecoderOutputs_Element outputs_{v_.outputs};
  Pipeline_Element pipeline_{v_.pipeline};
  SlidingWindow_Element sliding_window_{v_.sliding_window};
};

struct VisionInputs_Element : JSON::Element {
  explicit VisionInputs_Element(Config::Model::Vision::Inputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "pixel_values") {
      v_.pixel_values = JSON::Get<std::string_view>(value);
    } else if (name == "image_sizes") {
      v_.image_sizes = JSON::Get<std::string_view>(value);
    } else if (name == "attention_mask") {
      v_.attention_mask = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Vision::Inputs& v_;
};

struct VisionOutputs_Element : JSON::Element {
  explicit VisionOutputs_Element(Config::Model::Vision::Outputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "image_features") {
      v_.image_features = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Vision::Outputs& v_;
};

struct Vision_Element : JSON::Element {
  explicit Vision_Element(Config::Model::Vision& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "filename") {
      v_.filename = JSON::Get<std::string_view>(value);
    } else if (name == "config_filename") {
      v_.config_filename = JSON::Get<std::string_view>(value);
    } else if (name == "adapter_filename") {
      v_.adapter_filename = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

  Element& OnObject(std::string_view name) override {
    if (name == "inputs") {
      return inputs_;
    } else if (name == "outputs") {
      return outputs_;
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Vision& v_;
  VisionInputs_Element inputs_{v_.inputs};
  VisionOutputs_Element outputs_{v_.outputs};
};

struct SpeechInputs_Element : JSON::Element {
  explicit SpeechInputs_Element(Config::Model::Speech::Inputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "audio_embeds") {
      v_.audio_embeds = JSON::Get<std::string_view>(value);
    } else if (name == "attention_mask") {
      v_.attention_mask = JSON::Get<std::string_view>(value);
    } else if (name == "audio_sizes") {
      v_.audio_sizes = JSON::Get<std::string_view>(value);
    } else if (name == "audio_projection_mode") {
      v_.audio_projection_mode = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Speech::Inputs& v_;
};

struct SpeechOutputs_Element : JSON::Element {
  explicit SpeechOutputs_Element(Config::Model::Speech::Outputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "audio_features") {
      v_.audio_features = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Speech::Outputs& v_;
};

struct Speech_Element : JSON::Element {
  explicit Speech_Element(Config::Model::Speech& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "filename") {
      v_.filename = JSON::Get<std::string_view>(value);
    } else if (name == "config_filename") {
      v_.config_filename = JSON::Get<std::string_view>(value);
    } else if (name == "adapter_filename") {
      v_.adapter_filename = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

  Element& OnObject(std::string_view name) override {
    if (name == "inputs") {
      return inputs_;
    } else if (name == "outputs") {
      return outputs_;
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Speech& v_;
  SpeechInputs_Element inputs_{v_.inputs};
  SpeechOutputs_Element outputs_{v_.outputs};
};

struct EmbeddingInputs_Element : JSON::Element {
  explicit EmbeddingInputs_Element(Config::Model::Embedding::Inputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "input_ids") {
      v_.input_ids = JSON::Get<std::string_view>(value);
    } else if (name == "image_features") {
      v_.image_features = JSON::Get<std::string_view>(value);
    } else if (name == "audio_features") {
      v_.audio_features = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Embedding::Inputs& v_;
};

struct EmbeddingOutputs_Element : JSON::Element {
  explicit EmbeddingOutputs_Element(Config::Model::Embedding::Outputs& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "inputs_embeds") {
      v_.embeddings = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Embedding::Outputs& v_;
};

struct Embedding_Element : JSON::Element {
  explicit Embedding_Element(Config::Model::Embedding& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "filename") {
      v_.filename = JSON::Get<std::string_view>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

  Element& OnObject(std::string_view name) override {
    if (name == "inputs") {
      return inputs_;
    } else if (name == "outputs") {
      return outputs_;
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Model::Embedding& v_;
  EmbeddingInputs_Element inputs_{v_.inputs};
  EmbeddingOutputs_Element outputs_{v_.outputs};
};

struct Model_Element : JSON::Element {
  explicit Model_Element(Config::Model& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "type") {
      v_.type = JSON::Get<std::string_view>(value);
    } else if (name == "vocab_size") {
      v_.vocab_size = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "context_length") {
      v_.context_length = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "pad_token_id") {
      v_.pad_token_id = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "eos_token_id") {
      v_.eos_token_id.assign(1, static_cast<int>(JSON::Get<double>(value)));
    } else if (name == "bos_token_id") {
      v_.bos_token_id = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "decoder_start_token_id") {
      v_.decoder_start_token_id = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "sep_token_id") {
      v_.sep_token_id = static_cast<int>(JSON::Get<double>(value));
    } else {
      throw JSON::unknown_value_error{};
    }
  }

  Element& OnArray(std::string_view name) override {
    if (name == "eos_token_id")
      return eos_token_id_;
    throw JSON::unknown_value_error{};
  }

  Element& OnObject(std::string_view name) override {
    if (name == "encoder") {
      return encoder_;
    }
    if (name == "decoder") {
      return decoder_;
    }
    if (name == "vision") {
      return vision_;
    }
    if (name == "embedding") {
      return embedding_;
    }
    if (name == "speech") {
      return speech_;
    }
    throw JSON::unknown_value_error{};
  }

 private:
  Config::Model& v_;
  Encoder_Element encoder_{v_.encoder};
  Decoder_Element decoder_{v_.decoder};
  Int_Array_Element eos_token_id_{v_.eos_token_id};
  Vision_Element vision_{v_.vision};
  Embedding_Element embedding_{v_.embedding};
  Speech_Element speech_{v_.speech};
};

struct Search_Element : JSON::Element {
  explicit Search_Element(Config::Search& v) : v_{v} {}

  void OnValue(std::string_view name, JSON::Value value) override {
    if (name == "min_length") {
      v_.min_length = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "max_length") {
      v_.max_length = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "batch_size") {
      v_.batch_size = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "num_beams") {
      v_.num_beams = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "num_return_sequences") {
      v_.num_return_sequences = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "top_k") {
      v_.top_k = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "top_p") {
      v_.top_p = static_cast<float>(JSON::Get<double>(value));
    } else if (name == "temperature") {
      v_.temperature = static_cast<float>(JSON::Get<double>(value));
    } else if (name == "repetition_penalty") {
      v_.repetition_penalty = static_cast<float>(JSON::Get<double>(value));
    } else if (name == "length_penalty") {
      v_.length_penalty = static_cast<float>(JSON::Get<double>(value));
    } else if (name == "no_repeat_ngram_size") {
      v_.no_repeat_ngram_size = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "diversity_penalty") {
      v_.diversity_penalty = static_cast<float>(JSON::Get<double>(value));
    } else if (name == "length_penalty") {
      v_.length_penalty = static_cast<float>(JSON::Get<double>(value));
    } else if (name == "random_seed") {
      v_.random_seed = static_cast<int>(JSON::Get<double>(value));
    } else if (name == "do_sample") {
      v_.do_sample = JSON::Get<bool>(value);
    } else if (name == "past_present_share_buffer") {
      v_.past_present_share_buffer = JSON::Get<bool>(value);
    } else if (name == "early_stopping") {
      v_.early_stopping = JSON::Get<bool>(value);
    } else {
      throw JSON::unknown_value_error{};
    }
  }

 private:
  Config::Search& v_;
};

void SetSearchNumber(Config::Search& search, std::string_view name, double value) {
  try {
    Search_Element(search).OnValue(name, value);
  } catch (...) {
    JSON::TranslateException(name);
  }
}

void SetSearchBool(Config::Search& search, std::string_view name, bool value) {
  try {
    Search_Element(search).OnValue(name, value);
  } catch (...) {
    JSON::TranslateException(name);
  }
}

void ClearProviders(Config& config) {
  config.model.decoder.session_options.providers.clear();
}

void SetProviderOption(Config& config, std::string_view provider_name, std::string_view option_name, std::string_view option_value) {
  if (auto normalized_provider = NormalizeProviderName(provider_name); !contains(config.model.decoder.session_options.providers, normalized_provider))
    config.model.decoder.session_options.providers.push_back(std::string(normalized_provider));

  std::ostringstream json;
  json << R"({")" << provider_name << R"(":{)";
  if (!option_name.empty()) {
    json << R"(")" << option_name << R"(":")" << option_value << R"(")";
  }
  json << R"(}})";

  ProviderOptionsArray_Element element{config.model.decoder.session_options.provider_options};
  JSON::Parse(element, json.str());
}

bool IsGraphCaptureEnabled(const Config::SessionOptions& session_options) {
  for (const auto& provider : session_options.providers) {
    const auto provider_options = std::find_if(session_options.provider_options.begin(),
                                               session_options.provider_options.end(),
                                               [&provider](const Config::ProviderOptions& po) {
                                                 return po.name == provider;
                                               });
    if (provider_options != session_options.provider_options.end()) {
      if (provider_options->name == "cuda") {
        // Graph Capture is currently broken for CUDA
        for (const auto& value : provider_options->options) {
          if (value.first == "enable_cuda_graph" && value.second == "1") {
            throw std::runtime_error("Graph Capture is currently unsupported for CUDA");
          }
        }
      } else if (provider_options->name == "DML") {
        return true;
      } else if (provider_options->name == "NvTensorRtRtx") {
        for (const auto& value : provider_options->options) {
          if (value.first == "enable_cuda_graph" && value.second == "1") {
            return true;
          }
        }
        return false;
      }
    }
  }

  return false;
}

bool IsMultiProfileEnabled(const Config::SessionOptions& session_options) {
  for (const auto& provider : session_options.providers) {
    const auto provider_options = std::find_if(session_options.provider_options.begin(),
                                               session_options.provider_options.end(),
                                               [&provider](const Config::ProviderOptions& po) {
                                                 return po.name == provider;
                                               });
    if (provider_options != session_options.provider_options.end()) {
      if (provider_options->name == "NvTensorRtRtx") {
        for (const auto& value : provider_options->options) {
          if (value.first == "nv_multi_profile_enable" && value.second == "1") {
            return true;
          }
        }
      }
    }
  }
  return false;
}

struct Root_Element : JSON::Element {
  explicit Root_Element(Config& config) : config_{config} {}

  void OnValue(std::string_view name, JSON::Value value) override {
  }

  Element& OnObject(std::string_view name) override {
    if (name == "model") {
      return model_element_;
    }
    if (name == "search") {
      return search_element_;
    }
    throw JSON::unknown_value_error{};
  }

  Config& config_;
  Model_Element model_element_{config_.model};
  Search_Element search_element_{config_.search};
};

struct RootObject_Element : JSON::Element {
  explicit RootObject_Element(JSON::Element& t) : t_{t} {}

  Element& OnObject(std::string_view /*name*/) override {
    return t_;
  }

  JSON::Element& t_;
};

void ParseConfig(const fs::path& filename, std::string_view json_overlay, Config& config) {
  std::ifstream file = filename.open(std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Error opening " + filename.string());
  }
  std::streamsize const size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (!file.read(buffer.data(), size)) {
    throw std::runtime_error("Error reading " + filename.string());
  }

  Root_Element root{config};
  RootObject_Element root_object{root};
  try {
    JSON::Parse(root_object, std::string_view(buffer.data(), buffer.size()));
  } catch (const std::exception& message) {
    std::ostringstream oss;
    oss << "Error encountered while parsing '" << filename.string() << "' " << message.what();
    throw std::runtime_error(oss.str());
  }

  if (!json_overlay.empty()) {
    try {
      JSON::Parse(root_object, json_overlay);
    } catch (const std::exception& message) {
      std::ostringstream oss;
      oss << "Error encountered while parsing config overlay: " << message.what();
      throw std::runtime_error(oss.str());
    }
  }
}

void OverlayConfig(Config& config, std::string_view json) {
  Root_Element root{config};
  RootObject_Element element{root};
  JSON::Parse(element, json);
}

Config::Config(const fs::path& path, std::string_view json_overlay) : config_path{path} {
  ParseConfig(path / "genai_config.json", json_overlay, *this);

  if (model.context_length == 0) {
    throw std::runtime_error("model context_length is 0 or was not set. It must be greater than 0");
  }

  if (search.max_length == 0) {
    search.max_length = model.context_length;
  }

  // If no eos_token_id was set, set it to the pad token id
  if (model.eos_token_id.empty()) {
    model.eos_token_id.push_back(model.pad_token_id);
  }

  for (const auto& provider_option : model.decoder.session_options.provider_options) {
    model.decoder.session_options.providers.push_back(provider_option.name);
  }

  for (const auto& provider_option : model.encoder.session_options.provider_options) {
    model.encoder.session_options.providers.push_back(provider_option.name);
  }
}

void Config::AddMapping(const std::string& nominal_name, const std::string& graph_name) {
  auto [it, emplaced] = nominal_names_to_graph_names_.emplace(nominal_name, graph_name);
  if (it->second != graph_name) {
    std::ostringstream oss;
    oss << "Duplicate nominal name: " << nominal_name << " with graph names: "
        << graph_name << " and " << it->second;
    throw std::runtime_error(oss.str());
  }
}

std::pair<std::string, bool> Config::GetGraphName(const std::string& nominal_name) const {
  auto it = nominal_names_to_graph_names_.find(nominal_name);
  if (it == nominal_names_to_graph_names_.end()) {
    return {nominal_name, false};
  }
  return {it->second, true};
}

}  // namespace Generators
