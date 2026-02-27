#pragma once

#include <QObject>
#include <QString>

#include <memory>

#include "engines/EngineMode.hpp"
#include "engines/IAnalysisEngine.hpp"

class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString engineMode READ engineMode WRITE setEngineMode NOTIFY engineModeChanged)
    Q_PROPERTY(QString engineName READ engineName NOTIFY engineModeChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(double aiProbability READ aiProbability NOTIFY analysisChanged)
    Q_PROPERTY(QString decision READ decision NOTIFY analysisChanged)
    Q_PROPERTY(QString explanation READ explanation NOTIFY analysisChanged)
    Q_PROPERTY(QString mediaType READ mediaType NOTIFY analysisChanged)
    Q_PROPERTY(QString comparisonReport READ comparisonReport NOTIFY comparisonReportChanged)

public:
    explicit AppController(QObject* parent = nullptr);

    QString engineMode() const;
    Q_INVOKABLE void setEngineMode(const QString& mode);

    QString engineName() const;
    QString statusMessage() const;

    double aiProbability() const;
    QString decision() const;
    QString explanation() const;
    QString mediaType() const;
    QString comparisonReport() const;

    Q_INVOKABLE bool analyzeFile(const QString& filePath);
    Q_INVOKABLE bool compareEngines(const QString& filePath);

signals:
    void engineModeChanged();
    void statusMessageChanged();
    void analysisChanged();
    void comparisonReportChanged();

private:
    static QString modeToString(EngineMode mode);
    static EngineMode stringToMode(const QString& mode);

    void setStatus(const QString& message);
    void resetAnalysis();
    void rebuildEngine();

    EngineMode mode_ = EngineMode::OnDevicePy;
    std::unique_ptr<IAnalysisEngine> engine_;

    QString statusMessage_;
    QString comparisonReport_;
    AnalysisResult lastResult_;
};
