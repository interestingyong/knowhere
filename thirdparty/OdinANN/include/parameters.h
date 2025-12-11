#pragma once
#include <sstream>
#include <typeinfo>
#include <unordered_map>

namespace pipeann {

  class Parameters {
   public:
    Parameters() {
      int *p = new int;
      *p = 0;
      params["num_threads"] = p;
    }

    template<typename ParamType>
    inline void Set(const std::string &name, const ParamType &value) {
      ParamType *ptr = new ParamType;
      *ptr = value;
      auto iter = params.find(name);
      if (iter != params.end() && iter->second != nullptr) {
        delete static_cast<ParamType*>(iter->second);
      }
      params[name] = static_cast<void*>(ptr);
    }

    template<typename ParamType>
    inline ParamType Get(const std::string &name) const {
      auto item = params.find(name);
      if (item == params.end()) {
        throw std::invalid_argument("Invalid parameter name.");
      } else {
        // return ConvertStrToValue<ParamType>(item->second);
        if (item->second == nullptr) {
          throw std::invalid_argument(std::string("Parameter ") + name + " has value null.");
        } else {
          return *(static_cast<ParamType *>(item->second));
        }
      }
    }

    template<typename ParamType>
    inline ParamType Get(const std::string &name, const ParamType &default_value) const {
      try {
        return Get<ParamType>(name);
      } catch (const std::invalid_argument&) {
        return default_value;
      }
    }

    ~Parameters() {
      for (auto& pair : params) {
        if (pair.second != nullptr) {
          // 由于类型擦除，无法安全地调用delete，但这是原始设计的限制
          // 在实际应用中，应考虑使用智能指针或类型安全的容器
          delete static_cast<int*>(pair.second); // 假设主要是int类型，如构造函数所示
        }
      }
    }

   private:
    std::unordered_map<std::string, void *> params;

    Parameters(const Parameters &);
    Parameters &operator=(const Parameters &);

    template<typename ParamType>
    inline ParamType ConvertStrToValue(const std::string &str) const {
      std::stringstream sstream(str);
      ParamType value;
      if (!(sstream >> value) || !sstream.eof()) {
        std::stringstream err;
        err << "Failed to convert value '" << str << "' to type: " << typeid(value).name();
        throw std::runtime_error(err.str());
      }
      return value;
    }
  };
}  // namespace pipeann
