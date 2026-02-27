#pragma once

#include <QString>

#include "domain/AnalysisTypes.hpp"

class IAnalysisEngine {
public:
    virtual ~IAnalysisEngine() = default;
    virtual QString name() const = 0;
    virtual AnalysisResult analyzeFile(const QString& filePath) = 0;
};
