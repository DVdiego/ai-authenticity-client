#include "app/AppController.hpp"

#include <cmath>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QFileInfo>

#include "domain/AnalysisTypes.hpp"
#include "engines/EngineFactory.hpp"
#include "engines/OnDeviceNativeEngine.hpp"

namespace {
bool loadPyFeatures(const QString& filePath, std::vector<double>& outFeatures, QString& error) {
    outFeatures.clear();

    QString python = qEnvironmentVariable("AI_AUTH_PYTHON");
    if (python.isEmpty()) {
        const QString venvPy = QStringLiteral("/Users/macmini/tfg_ia_video/venv/bin/python3");
        python = QFileInfo::exists(venvPy) ? venvPy : QStringLiteral("python3");
    }

    const QString script = qEnvironmentVariable(
        "AI_AUTH_ONDEVICE_SCRIPT",
        QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/frontend/ai-authenticity-client/cliente/scripts/ondevice_infer.py")
    );

    QProcess process;
    process.start(python, QStringList{script, QStringLiteral("--file"), filePath, QStringLiteral("--include-features")});
    if (!process.waitForStarted(5000)) {
        error = QStringLiteral("Failed to start Python feature extractor.");
        return false;
    }
    if (!process.waitForFinished(60000)) {
        process.kill();
        error = QStringLiteral("Python feature extractor timed out.");
        return false;
    }

    const QByteArray stdoutData = process.readAllStandardOutput().trimmed();
    if (stdoutData.isEmpty()) {
        error = QStringLiteral("Python feature extractor returned empty output.");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stdoutData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        error = QStringLiteral("Invalid JSON from Python feature extractor.");
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("error"))) {
        error = root.value(QStringLiteral("error")).toString(QStringLiteral("Python feature extractor failed."));
        return false;
    }

    const QJsonArray arr = root.value(QStringLiteral("debug_features")).toArray();
    if (arr.isEmpty()) {
        error = QStringLiteral("Python debug_features missing.");
        return false;
    }

    outFeatures.reserve(static_cast<size_t>(arr.size()));
    for (const QJsonValue& v : arr) {
        outFeatures.push_back(v.toDouble(0.0));
    }
    return true;
}
} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent) {
    rebuildEngine();
    setStatus(QStringLiteral("Ready."));
}

QString AppController::modeToString(EngineMode mode) {
    switch (mode) {
    case EngineMode::OnDevicePy:
        return QStringLiteral("on_device_py");
    case EngineMode::OnDeviceNative:
        return QStringLiteral("on_device_native");
    case EngineMode::Api:
        return QStringLiteral("api");
    }
    return QStringLiteral("on_device_py");
}

EngineMode AppController::stringToMode(const QString& mode) {
    if (mode == QStringLiteral("api")) {
        return EngineMode::Api;
    }
    if (mode == QStringLiteral("on_device_native")) {
        return EngineMode::OnDeviceNative;
    }
    return EngineMode::OnDevicePy;
}

QString AppController::engineMode() const {
    return modeToString(mode_);
}

void AppController::setEngineMode(const QString& mode) {
    const EngineMode requested = stringToMode(mode);
    if (requested == mode_) {
        return;
    }

    mode_ = requested;
    rebuildEngine();
    resetAnalysis();
    setStatus(QStringLiteral("Engine switched to %1.").arg(engineName()));
    emit engineModeChanged();
}

QString AppController::engineName() const {
    return engine_ ? engine_->name() : QStringLiteral("none");
}

QString AppController::statusMessage() const {
    return statusMessage_;
}

double AppController::aiProbability() const {
    return lastResult_.ok ? lastResult_.aiProbabilityPct : 0.0;
}

QString AppController::decision() const {
    return lastResult_.ok ? decisionToString(lastResult_.decision) : QStringLiteral("-");
}

QString AppController::explanation() const {
    return lastResult_.ok ? lastResult_.explanation : QStringLiteral("-");
}

QString AppController::mediaType() const {
    return lastResult_.ok ? lastResult_.mediaType : QStringLiteral("-");
}

QString AppController::comparisonReport() const {
    return comparisonReport_;
}

bool AppController::analyzeFile(const QString& filePath) {
    if (!engine_) {
        setStatus(QStringLiteral("Engine not initialized."));
        return false;
    }

    if (filePath.trimmed().isEmpty()) {
        setStatus(QStringLiteral("Select a file path first."));
        return false;
    }

    lastResult_ = engine_->analyzeFile(filePath);
    emit analysisChanged();

    if (!lastResult_.ok) {
        setStatus(lastResult_.error.isEmpty() ? QStringLiteral("Analysis failed.") : lastResult_.error);
        return false;
    }

    setStatus(QStringLiteral("Analysis completed with %1.").arg(engineName()));
    return true;
}

bool AppController::compareEngines(const QString& filePath) {
    if (filePath.trimmed().isEmpty()) {
        setStatus(QStringLiteral("Select a file path first."));
        return false;
    }

    const EngineMode current = mode_;
    const EngineMode modes[] = {EngineMode::OnDevicePy, EngineMode::OnDeviceNative, EngineMode::Api};

    AnalysisResult py;
    AnalysisResult native;
    AnalysisResult api;

    QStringList lines;
    lines << QStringLiteral("Engine comparison report");
    lines << QStringLiteral("file: %1").arg(filePath);

    for (const EngineMode m : modes) {
        auto engine = EngineFactory::create(m);
        const AnalysisResult res = engine->analyzeFile(filePath);

        if (m == EngineMode::OnDevicePy) {
            py = res;
        } else if (m == EngineMode::OnDeviceNative) {
            native = res;
        } else if (m == EngineMode::Api) {
            api = res;
        }

        if (!res.ok) {
            lines << QStringLiteral("- %1: ERROR -> %2").arg(modeToString(m), res.error);
            continue;
        }

        lines << QStringLiteral("- %1: prob=%2 decision=%3 media=%4")
                     .arg(modeToString(m),
                          QString::number(res.aiProbabilityPct, 'f', 2),
                          decisionToString(res.decision),
                          res.mediaType);
    }

    if (py.ok && api.ok) {
        const double probDelta = std::fabs(py.aiProbabilityPct - api.aiProbabilityPct);
        lines << QStringLiteral("delta(on_device_py vs api): %1").arg(QString::number(probDelta, 'f', 4));
    }
    if (native.ok && api.ok) {
        const double probDelta = std::fabs(native.aiProbabilityPct - api.aiProbabilityPct);
        lines << QStringLiteral("delta(on_device_native vs api): %1").arg(QString::number(probDelta, 'f', 4));
    }
    if (py.ok && native.ok) {
        const double probDelta = std::fabs(py.aiProbabilityPct - native.aiProbabilityPct);
        lines << QStringLiteral("delta(on_device_py vs on_device_native): %1").arg(QString::number(probDelta, 'f', 4));
    }

    std::vector<double> pyFeatures;
    std::vector<double> nativeFeatures;
    QString featureError;

    const bool pyFeatOk = loadPyFeatures(filePath, pyFeatures, featureError);
    if (!pyFeatOk) {
        lines << QStringLiteral("feature_debug(py): ERROR -> %1").arg(featureError);
    }

    featureError.clear();
    const bool nativeFeatOk = OnDeviceNativeEngine::extractFeaturesForDebug(filePath, nativeFeatures, featureError);
    if (!nativeFeatOk) {
        lines << QStringLiteral("feature_debug(native): ERROR -> %1").arg(featureError);
    }

    if (pyFeatOk && nativeFeatOk && pyFeatures.size() == nativeFeatures.size()) {
        lines << QStringLiteral("feature_diff(py vs native):");
        for (size_t i = 0; i < pyFeatures.size(); ++i) {
            const double d = std::fabs(pyFeatures[i] - nativeFeatures[i]);
            lines << QStringLiteral("  f%1 py=%2 native=%3 diff=%4")
                         .arg(static_cast<int>(i))
                         .arg(QString::number(pyFeatures[i], 'f', 6))
                         .arg(QString::number(nativeFeatures[i], 'f', 6))
                         .arg(QString::number(d, 'f', 6));
        }
    }

    comparisonReport_ = lines.join("\n");
    emit comparisonReportChanged();

    mode_ = current;
    rebuildEngine();
    emit engineModeChanged();

    setStatus(QStringLiteral("Comparison completed."));
    return true;
}

void AppController::setStatus(const QString& message) {
    if (statusMessage_ == message) {
        return;
    }
    statusMessage_ = message;
    emit statusMessageChanged();
}

void AppController::resetAnalysis() {
    lastResult_ = AnalysisResult{};
    emit analysisChanged();
}

void AppController::rebuildEngine() {
    engine_ = EngineFactory::create(mode_);
}
