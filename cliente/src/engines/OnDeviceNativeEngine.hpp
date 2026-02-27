#pragma once

#include <vector>

#include "engines/IAnalysisEngine.hpp"

class OnDeviceNativeEngine final : public IAnalysisEngine {
public:
    QString name() const override;
    AnalysisResult analyzeFile(const QString& filePath) override;

    static bool extractFeaturesForDebug(const QString& filePath, std::vector<double>& outFeatures, QString& error);
};
