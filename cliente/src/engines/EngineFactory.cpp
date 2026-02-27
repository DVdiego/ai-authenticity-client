#include "engines/EngineFactory.hpp"

#include "engines/ApiEngine.hpp"
#include "engines/OnDeviceEngine.hpp"
#include "engines/OnDeviceNativeEngine.hpp"

std::unique_ptr<IAnalysisEngine> EngineFactory::create(EngineMode mode) {
    switch (mode) {
    case EngineMode::OnDevicePy:
        return std::make_unique<OnDeviceEngine>();
    case EngineMode::OnDeviceNative:
        return std::make_unique<OnDeviceNativeEngine>();
    case EngineMode::Api:
        return std::make_unique<ApiEngine>();
    }
    return std::make_unique<OnDeviceEngine>();
}
