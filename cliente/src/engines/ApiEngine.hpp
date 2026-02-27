#pragma once

#include <QNetworkAccessManager>

#include "engines/IAnalysisEngine.hpp"

class ApiEngine final : public IAnalysisEngine {
public:
    QString name() const override;
    AnalysisResult analyzeFile(const QString& filePath) override;

private:
    static DecisionLevel decisionFromString(const QString& value);
    static QUrl endpointUrl();

    QNetworkAccessManager network_;
};
