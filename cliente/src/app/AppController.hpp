#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

#include "engines/EngineMode.hpp"
#include "engines/IAnalysisEngine.hpp"

class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString engineMode READ engineMode WRITE setEngineMode NOTIFY engineModeChanged)
    Q_PROPERTY(QString engineName READ engineName NOTIFY engineModeChanged)
    Q_PROPERTY(bool showDevOptions READ showDevOptions CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(double aiProbability READ aiProbability NOTIFY analysisChanged)
    Q_PROPERTY(QString decision READ decision NOTIFY analysisChanged)
    Q_PROPERTY(QString explanation READ explanation NOTIFY analysisChanged)
    Q_PROPERTY(QString mediaType READ mediaType NOTIFY analysisChanged)
    Q_PROPERTY(QString comparisonReport READ comparisonReport NOTIFY comparisonReportChanged)
    Q_PROPERTY(QString analysisHistory READ analysisHistory NOTIFY analysisHistoryChanged)
    Q_PROPERTY(QString comparisonHistory READ comparisonHistory NOTIFY comparisonHistoryChanged)
    Q_PROPERTY(QString pyModelPath READ pyModelPath WRITE setPyModelPath NOTIFY pyModelPathChanged)
    Q_PROPERTY(QString nativeModelVersion READ nativeModelVersion NOTIFY nativeModelVersionChanged)
    Q_PROPERTY(QStringList pyModelOptions READ pyModelOptions NOTIFY pyModelOptionsChanged)
    Q_PROPERTY(int pyModelIndex READ pyModelIndex NOTIFY pyModelIndexChanged)

public:
    explicit AppController(QObject* parent = nullptr);

    QString engineMode() const;
    Q_INVOKABLE void setEngineMode(const QString& mode);

    QString engineName() const;
    bool showDevOptions() const;
    QString statusMessage() const;

    double aiProbability() const;
    QString decision() const;
    QString explanation() const;
    QString mediaType() const;
    QString comparisonReport() const;
    QString analysisHistory() const;
    QString comparisonHistory() const;
    QString pyModelPath() const;
    QString nativeModelVersion() const;
    QStringList pyModelOptions() const;
    int pyModelIndex() const;
    Q_INVOKABLE void setPyModelPath(const QString& path);
    Q_INVOKABLE void setPyModelIndex(int index);
    Q_INVOKABLE void refreshPyModelOptions();

    Q_INVOKABLE bool analyzeFile(const QString& filePath);
    Q_INVOKABLE bool compareEngines(const QString& filePath);

signals:
    void engineModeChanged();
    void statusMessageChanged();
    void analysisChanged();
    void comparisonReportChanged();
    void analysisHistoryChanged();
    void comparisonHistoryChanged();
    void pyModelPathChanged();
    void nativeModelVersionChanged();
    void pyModelOptionsChanged();
    void pyModelIndexChanged();

private:
    static QString modeToString(EngineMode mode);
    static EngineMode stringToMode(const QString& mode);

    void setStatus(const QString& message);
    void resetAnalysis();
    void rebuildEngine();
    void appendAnalysisHistory(const QString& entryJson, const QString& summaryLine);
    void refreshAnalysisHistory();
    void appendComparisonHistory(const QString& entryJson, const QString& summaryLine);
    void refreshComparisonHistory();
    void rebuildPyModelOptions();

    EngineMode mode_ = EngineMode::OnDeviceNative;
    std::unique_ptr<IAnalysisEngine> engine_;

    QString statusMessage_;
    QString comparisonReport_;
    QString analysisHistory_;
    QString comparisonHistory_;
    QString pyModelPath_;
    QString nativeModelVersion_;
    QStringList pyModelOptions_;
    QStringList pyModelPaths_;
    QStringList nativeModelVersions_;
    AnalysisResult lastResult_;
};
