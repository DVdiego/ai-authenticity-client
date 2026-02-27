#pragma once

#include <memory>

#include "engines/EngineMode.hpp"
#include "engines/IAnalysisEngine.hpp"

class EngineFactory {
public:
    static std::unique_ptr<IAnalysisEngine> create(EngineMode mode);
};
