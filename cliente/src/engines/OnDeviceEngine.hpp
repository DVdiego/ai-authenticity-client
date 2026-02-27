#pragma once

#include "engines/IAnalysisEngine.hpp"

class OnDeviceEngine final : public IAnalysisEngine {
public:
    QString name() const override;
    AnalysisResult analyzeFile(const QString& filePath) override;
};
