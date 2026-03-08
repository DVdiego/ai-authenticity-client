#include "app/AppController.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QProcess>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

#include "domain/AnalysisTypes.hpp"
#include "engines/EngineFactory.hpp"
#include "engines/OnDeviceNativeEngine.hpp"

namespace {
QString appResourcesDirectoryPath() {
    const QString candidate = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources"));
    const QFileInfo info(candidate);
    return (info.exists() && info.isDir()) ? candidate : QString();
}

QString pythonExecutable() {
    QString python = qEnvironmentVariable("AI_AUTH_PYTHON");
    if (!python.isEmpty()) {
        return python;
    }
    const QString venvPy = QStringLiteral("/Users/macmini/tfg_ia_video/venv/bin/python3");
    return QFileInfo::exists(venvPy) ? venvPy : QStringLiteral("python3");
}

bool loadPyFeatures(const QString& filePath, std::vector<double>& outFeatures, QString& error) {
    outFeatures.clear();

    const QString python = pythonExecutable();

    const QString script = qEnvironmentVariable(
        "AI_AUTH_ONDEVICE_SCRIPT",
        QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/frontend/ai-authenticity-client/cliente/scripts/ondevice_infer.py")
    );
    const QString modelPath = qEnvironmentVariable("AI_AUTH_PY_IMAGE_MODEL").trimmed();

    QProcess process;
    QStringList args{script, QStringLiteral("--file"), filePath, QStringLiteral("--include-features")};
    if (!modelPath.isEmpty()) {
        args << QStringLiteral("--image-model") << modelPath;
    }
    process.start(python, args);
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

QString historyFilePath() {
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        QDir().mkpath(appData);
        return QDir(appData).filePath(QStringLiteral("compare_history.jsonl"));
    }
    return QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/frontend/ai-authenticity-client/cliente/runtime/compare_history.jsonl");
}

QString analysisHistoryFilePath() {
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        QDir().mkpath(appData);
        return QDir(appData).filePath(QStringLiteral("execute_history.jsonl"));
    }
    return QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/frontend/ai-authenticity-client/cliente/runtime/execute_history.jsonl");
}

QString modelsDirectoryPath() {
    return QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/backend/models");
}

QString modelPackageDirectoryPath() {
    const QString configured = qEnvironmentVariable("AIAUTH_MODEL_PACKAGE_DIR").trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }
    const QString resourcesDir = appResourcesDirectoryPath();
    if (!resourcesDir.isEmpty()) {
        const QString bundled = QDir(resourcesDir).filePath(QStringLiteral("model-package"));
        if (QFileInfo::exists(bundled)) {
            return bundled;
        }
    }
    return QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/ai-authenticity-mobile/model-package");
}

QString legalDocsDirectoryPath() {
    const QString resourcesDir = appResourcesDirectoryPath();
    if (!resourcesDir.isEmpty()) {
        const QString bundled = QDir(resourcesDir).filePath(QStringLiteral("legal"));
        if (QFileInfo::exists(bundled)) {
            return bundled;
        }
    }
    return QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/frontend/ai-authenticity-client/legal");
}

QString distributionDirectoryPath() {
    const QString configured = qEnvironmentVariable("AI_AUTH_DISTRIBUTION_DIR").trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }
    const QString resourcesDir = appResourcesDirectoryPath();
    if (!resourcesDir.isEmpty()) {
        const QString bundled = QDir(resourcesDir).filePath(QStringLiteral("distribution"));
        if (QFileInfo::exists(bundled)) {
            return bundled;
        }
    }
    return QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/frontend/ai-authenticity-client/distribution");
}

QByteArray hmacSha256(const QByteArray& key, const QByteArray& message) {
    constexpr int blockSize = 64;
    QByteArray normalizedKey = key;
    if (normalizedKey.size() > blockSize) {
        normalizedKey = QCryptographicHash::hash(normalizedKey, QCryptographicHash::Sha256);
    }
    normalizedKey = normalizedKey.leftJustified(blockSize, '\0', true);

    QByteArray oKeyPad(blockSize, char(0x5c));
    QByteArray iKeyPad(blockSize, char(0x36));
    for (int i = 0; i < blockSize; ++i) {
        oKeyPad[i] = oKeyPad[i] ^ normalizedKey[i];
        iKeyPad[i] = iKeyPad[i] ^ normalizedKey[i];
    }

    QByteArray inner = iKeyPad;
    inner.append(message);
    const QByteArray innerHash = QCryptographicHash::hash(inner, QCryptographicHash::Sha256);

    QByteArray outer = oKeyPad;
    outer.append(innerHash);
    return QCryptographicHash::hash(outer, QCryptographicHash::Sha256).toHex();
}

QByteArray licenseSigningKey() {
    return QByteArrayLiteral("ai_authenticity_client_tfg_license_v1");
}

void applySelectedModelEnv(const QString& pyPath, const QString& nativeVersion) {
    if (pyPath.isEmpty() || pyPath.startsWith(QStringLiteral("pkg:"))) {
        qunsetenv("AI_AUTH_PY_IMAGE_MODEL");
    } else {
        qputenv("AI_AUTH_PY_IMAGE_MODEL", pyPath.toUtf8());
    }

    if (nativeVersion.isEmpty()) {
        qunsetenv("AI_AUTH_NATIVE_MODEL_VERSION");
    } else {
        qputenv("AI_AUTH_NATIVE_MODEL_VERSION", nativeVersion.toUtf8());
    }
}

int expectedPyImageInputDim() {
    const QString backendDir = qEnvironmentVariable(
        "AI_AUTH_BACKEND_DIR",
        QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/backend")
    );
    if (!QFileInfo::exists(backendDir)) {
        return 90;
    }

    const QString python = pythonExecutable();
    QProcess process;

    const QString code = QStringLiteral(
        "import sys; "
        "sys.path.insert(0, sys.argv[1]); "
        "from app.services.feature_extraction import IMAGE_VECTOR_SIZE; "
        "print(int(IMAGE_VECTOR_SIZE))"
    );

    process.start(python, QStringList() << QStringLiteral("-c") << code << backendDir);
    if (!process.waitForStarted(5000)) {
        return 90;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        return 90;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return 90;
    }

    bool ok = false;
    const int dim = QString::fromUtf8(process.readAllStandardOutput()).trimmed().toInt(&ok);
    return ok && dim > 0 ? dim : 90;
}

int detectPtInputDim(const QString& modelPath) {
    const QString python = pythonExecutable();
    QProcess process;

    const QString code = QStringLiteral(
        "import sys, torch; "
        "p=sys.argv[1]; "
        "obj=torch.load(p, map_location='cpu'); "
        "sd=obj.get('state_dict', obj) if isinstance(obj, dict) else obj; "
        "w=sd.get('fc1.weight') if isinstance(sd, dict) else None; "
        "print(int(w.shape[1]) if w is not None and hasattr(w, 'shape') and len(w.shape) >= 2 else -1)"
    );

    process.start(python, QStringList() << QStringLiteral("-c") << code << modelPath);
    if (!process.waitForStarted(5000)) {
        return -1;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        return -1;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return -1;
    }

    bool ok = false;
    const int dim = QString::fromUtf8(process.readAllStandardOutput()).trimmed().toInt(&ok);
    return ok ? dim : -1;
}
} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent) {
    pyModelPath_ = qEnvironmentVariable("AI_AUTH_PY_IMAGE_MODEL");
    reloadDistributionMetadata();
    rebuildPyModelOptions();
    rebuildEngine();
    refreshAnalysisHistory();
    refreshComparisonHistory();
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
    return QStringLiteral("on_device_native");
}

EngineMode AppController::stringToMode(const QString& mode) {
    if (mode == QStringLiteral("api")) {
        return EngineMode::Api;
    }
    if (mode == QStringLiteral("on_device_native")) {
        return EngineMode::OnDeviceNative;
    }
    return EngineMode::OnDeviceNative;
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

bool AppController::showDevOptions() const {
    return qEnvironmentVariableIntValue("AI_AUTH_SHOW_DEV_UI") == 1;
}

QString AppController::legalDocsPath() const {
    return legalDocsDirectoryPath();
}

bool AppController::licenseValid() const {
    return licenseValid_;
}

QString AppController::licenseSummary() const {
    return licenseSummary_;
}

QString AppController::buildSummary() const {
    return buildSummary_;
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

QString AppController::analysisHistory() const {
    return analysisHistory_;
}

QString AppController::comparisonHistory() const {
    return comparisonHistory_;
}

QString AppController::pyModelPath() const {
    return pyModelPath_;
}

QString AppController::nativeModelVersion() const {
    return nativeModelVersion_;
}

QStringList AppController::pyModelOptions() const {
    return pyModelOptions_;
}

int AppController::pyModelIndex() const {
    return pyModelPaths_.indexOf(pyModelPath_);
}

void AppController::setPyModelPath(const QString& path) {
    const QString normalized = path.trimmed();
    const int selectedIndex = pyModelPaths_.indexOf(normalized);
    const QString nextNativeVersion =
        (selectedIndex >= 0 && selectedIndex < nativeModelVersions_.size())
            ? nativeModelVersions_.at(selectedIndex)
            : QString();

    if (pyModelPath_ == normalized && nativeModelVersion_ == nextNativeVersion) {
        return;
    }

    const QString previousNativeVersion = nativeModelVersion_;
    pyModelPath_ = normalized;
    nativeModelVersion_ = nextNativeVersion;
    applySelectedModelEnv(pyModelPath_, nativeModelVersion_);
    emit pyModelPathChanged();
    if (previousNativeVersion != nativeModelVersion_) {
        emit nativeModelVersionChanged();
    }
    emit pyModelIndexChanged();
}

void AppController::setPyModelIndex(int index) {
    if (index < 0 || index >= pyModelPaths_.size()) {
        return;
    }
    setPyModelPath(pyModelPaths_.at(index));
}

void AppController::refreshPyModelOptions() {
    rebuildPyModelOptions();
}

bool AppController::analyzeFile(const QString& filePath) {
    if (!licenseValid_) {
        setStatus(QStringLiteral("License metadata missing or invalid. Open Legal for details."));
        return false;
    }

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

    QJsonObject history;
    history.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
    history.insert(QStringLiteral("file"), filePath);
    history.insert(QStringLiteral("engine"), engineName());
    history.insert(QStringLiteral("prob"), lastResult_.aiProbabilityPct);
    history.insert(QStringLiteral("raw"), lastResult_.scores.aiModelRaw);
    history.insert(QStringLiteral("decision"), decisionToString(lastResult_.decision));
    history.insert(QStringLiteral("media"), lastResult_.mediaType);
    history.insert(QStringLiteral("model_label"), lastResult_.modelLabel);
    history.insert(QStringLiteral("model_path"), lastResult_.modelPath);
    appendAnalysisHistory(
        QString::fromUtf8(QJsonDocument(history).toJson(QJsonDocument::Compact)),
        QStringLiteral("%1 | %2 | %3 | %4 | %5")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                 QFileInfo(filePath).fileName(),
                 engineName(),
                 QString::number(lastResult_.aiProbabilityPct, 'f', 2),
                 decisionToString(lastResult_.decision))
    );

    setStatus(QStringLiteral("Analysis completed with %1.").arg(engineName()));
    return true;
}

bool AppController::compareEngines(const QString& filePath) {
    if (!licenseValid_) {
        setStatus(QStringLiteral("License metadata missing or invalid. Open Legal for details."));
        return false;
    }

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

        lines << QStringLiteral("- %1: prob=%2 raw=%3 decision=%4 media=%5")
                     .arg(modeToString(m),
                          QString::number(res.aiProbabilityPct, 'f', 2),
                          QString::number(res.scores.aiModelRaw, 'f', 6),
                          decisionToString(res.decision),
                          res.mediaType);
    }

    if (py.ok && api.ok) {
        const double probDelta = std::fabs(py.aiProbabilityPct - api.aiProbabilityPct);
        lines << QStringLiteral("delta(on_device_py vs api): %1").arg(QString::number(probDelta, 'f', 4));
        const double rawDelta = std::fabs(py.scores.aiModelRaw - api.scores.aiModelRaw);
        lines << QStringLiteral("raw_delta(on_device_py vs api): %1").arg(QString::number(rawDelta, 'f', 6));
    }
    if (native.ok && api.ok) {
        const double probDelta = std::fabs(native.aiProbabilityPct - api.aiProbabilityPct);
        lines << QStringLiteral("delta(on_device_native vs api): %1").arg(QString::number(probDelta, 'f', 4));
        const double rawDelta = std::fabs(native.scores.aiModelRaw - api.scores.aiModelRaw);
        lines << QStringLiteral("raw_delta(on_device_native vs api): %1").arg(QString::number(rawDelta, 'f', 6));
    }
    if (py.ok && native.ok) {
        const double probDelta = std::fabs(py.aiProbabilityPct - native.aiProbabilityPct);
        lines << QStringLiteral("delta(on_device_py vs on_device_native): %1").arg(QString::number(probDelta, 'f', 4));
        const double rawDelta = std::fabs(py.scores.aiModelRaw - native.scores.aiModelRaw);
        lines << QStringLiteral("raw_delta(on_device_py vs on_device_native): %1").arg(QString::number(rawDelta, 'f', 6));
    }

    std::vector<double> pyFeatures;
    std::vector<double> nativeFeatures;
    QString featureError;

    const bool pyFeatOk = loadPyFeatures(filePath, pyFeatures, featureError);
    if (!pyFeatOk) {
        lines << QStringLiteral("feature_debug(py): ERROR -> %1").arg(featureError);
    } else {
        py.debugFeatures = pyFeatures;
    }

    featureError.clear();
    const bool nativeFeatOk = OnDeviceNativeEngine::extractFeaturesForDebug(filePath, nativeFeatures, featureError);
    if (!nativeFeatOk) {
        lines << QStringLiteral("feature_debug(native): ERROR -> %1").arg(featureError);
    } else {
        native.debugFeatures = nativeFeatures;
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
    } else if (pyFeatOk && nativeFeatOk) {
        lines << QStringLiteral("feature_diff(py vs native): SKIPPED (size mismatch py=%1 native=%2)")
                     .arg(static_cast<int>(pyFeatures.size()))
                     .arg(static_cast<int>(nativeFeatures.size()));
    }

    lines << QStringLiteral("models:");
    lines << QStringLiteral("  on_device_py -> %1").arg(py.modelPath.isEmpty() ? QStringLiteral("N/A") : py.modelPath);
    lines << QStringLiteral("  on_device_native -> %1").arg(native.modelPath.isEmpty() ? QStringLiteral("N/A") : native.modelPath);
    lines << QStringLiteral("  api -> %1").arg(api.modelPath.isEmpty() ? QStringLiteral("N/A") : api.modelPath);

    comparisonReport_ = lines.join("\n");
    emit comparisonReportChanged();

    QJsonObject history;
    history.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
    history.insert(QStringLiteral("file"), filePath);
    history.insert(QStringLiteral("selected_py_model"), pyModelPath_);
    auto resultToJson = [](const AnalysisResult& r) {
        QJsonObject o;
        o.insert(QStringLiteral("ok"), r.ok);
        o.insert(QStringLiteral("prob"), r.aiProbabilityPct);
        o.insert(QStringLiteral("decision"), decisionToString(r.decision));
        o.insert(QStringLiteral("media"), r.mediaType);
        o.insert(QStringLiteral("model_label"), r.modelLabel);
        o.insert(QStringLiteral("model_path"), r.modelPath);
        o.insert(QStringLiteral("model_sha256"), r.modelSha256);
        o.insert(QStringLiteral("model_loaded"), r.modelLoaded);
        QJsonArray feat;
        for (double v : r.debugFeatures) {
            feat.append(v);
        }
        o.insert(QStringLiteral("features"), feat);
        if (!r.error.isEmpty()) {
            o.insert(QStringLiteral("error"), r.error);
        }
        return o;
    };
    history.insert(QStringLiteral("on_device_py"), resultToJson(py));
    history.insert(QStringLiteral("on_device_native"), resultToJson(native));
    history.insert(QStringLiteral("api"), resultToJson(api));
    appendComparisonHistory(
        QString::fromUtf8(QJsonDocument(history).toJson(QJsonDocument::Compact)),
        QStringLiteral("%1 | %2 | py=%3 | native=%4 | api=%5")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                 QFileInfo(filePath).fileName(),
                 QString::number(py.aiProbabilityPct, 'f', 2),
                 QString::number(native.aiProbabilityPct, 'f', 2),
                 QString::number(api.aiProbabilityPct, 'f', 2))
    );

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

void AppController::appendAnalysisHistory(const QString& entryJson, const QString& summaryLine) {
    const QString path = analysisHistoryFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << entryJson << '\n';
    }

    if (!analysisHistory_.isEmpty()) {
        analysisHistory_.prepend(summaryLine + '\n');
    } else {
        analysisHistory_ = summaryLine;
    }

    const QStringList lines = analysisHistory_.split('\n', Qt::SkipEmptyParts);
    analysisHistory_ = lines.mid(0, std::min<qsizetype>(10, lines.size())).join('\n');
    emit analysisHistoryChanged();
}

void AppController::appendComparisonHistory(const QString& entryJson, const QString& summaryLine) {
    const QString path = historyFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << entryJson << '\n';
    }

    if (!comparisonHistory_.isEmpty()) {
        comparisonHistory_.prepend(summaryLine + '\n');
    } else {
        comparisonHistory_ = summaryLine;
    }

    const QStringList lines = comparisonHistory_.split('\n', Qt::SkipEmptyParts);
    comparisonHistory_ = lines.mid(0, std::min<qsizetype>(10, lines.size())).join('\n');
    emit comparisonHistoryChanged();
}

void AppController::refreshAnalysisHistory() {
    const QString path = analysisHistoryFilePath();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        analysisHistory_.clear();
        emit analysisHistoryChanged();
        return;
    }

    QStringList summaries;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }
        const QJsonObject o = doc.object();
        const QString fileName = QFileInfo(o.value(QStringLiteral("file")).toString()).fileName();
        summaries.prepend(QStringLiteral("%1 | %2 | %3 | %4 | %5")
                              .arg(o.value(QStringLiteral("timestamp")).toString(),
                                   fileName.isEmpty() ? QStringLiteral("unknown") : fileName,
                                   o.value(QStringLiteral("engine")).toString(QStringLiteral("unknown")),
                                   QString::number(o.value(QStringLiteral("prob")).toDouble(0.0), 'f', 2),
                                   o.value(QStringLiteral("decision")).toString(QStringLiteral("-"))));
    }
    analysisHistory_ = summaries.mid(0, std::min<qsizetype>(10, summaries.size())).join('\n');
    emit analysisHistoryChanged();
}

void AppController::refreshComparisonHistory() {
    const QString path = historyFilePath();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        comparisonHistory_.clear();
        emit comparisonHistoryChanged();
        return;
    }

    QStringList summaries;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }
        const QJsonObject o = doc.object();
        const QJsonObject py = o.value(QStringLiteral("on_device_py")).toObject();
        const QJsonObject native = o.value(QStringLiteral("on_device_native")).toObject();
        const QJsonObject api = o.value(QStringLiteral("api")).toObject();
        const QString fileName = QFileInfo(o.value(QStringLiteral("file")).toString()).fileName();
        summaries.prepend(QStringLiteral("%1 | %2 | py=%3 | native=%4 | api=%5")
                              .arg(o.value(QStringLiteral("timestamp")).toString(),
                                   fileName.isEmpty() ? QStringLiteral("unknown") : fileName,
                                   QString::number(py.value(QStringLiteral("prob")).toDouble(0.0), 'f', 2),
                                   QString::number(native.value(QStringLiteral("prob")).toDouble(0.0), 'f', 2),
                                   QString::number(api.value(QStringLiteral("prob")).toDouble(0.0), 'f', 2)));
    }
    comparisonHistory_ = summaries.mid(0, std::min<qsizetype>(10, summaries.size())).join('\n');
    emit comparisonHistoryChanged();
}

void AppController::rebuildPyModelOptions() {
    const QString previousPath = pyModelPath_;
    const QString previousNativeVersion = nativeModelVersion_;
    const int expectedDim = expectedPyImageInputDim();
    const bool devMode = showDevOptions();
    pyModelOptions_.clear();
    pyModelPaths_.clear();
    nativeModelVersions_.clear();

    struct ModelEntry {
        QString display;
        QString pyPath;
        QString nativeVersion;
        QDateTime timestamp;
    };

    QVector<ModelEntry> entries;
    QDir packageImagesDir(modelPackageDirectoryPath() + QStringLiteral("/image"));
    const QFileInfoList packageDirs = packageImagesDir.entryInfoList(
        QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot,
        QDir::NoSort
    );

    for (const QFileInfo& packageInfo : packageDirs) {
        QFile metaFile(packageInfo.absoluteFilePath() + QStringLiteral("/meta.json"));
        if (!metaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }

        const QJsonObject meta = doc.object();
        const QString modelVersion = meta.value(QStringLiteral("model_version")).toString(packageInfo.fileName());
        const QJsonObject runtime = meta.value(QStringLiteral("runtime")).toObject();
        const int runtimeInputDim = runtime.value(QStringLiteral("input_dim")).toInt(-1);
        if (runtimeInputDim != expectedDim) {
            continue;
        }

        const QJsonObject sourceCheckpoint = meta.value(QStringLiteral("source_checkpoint")).toObject();
        QString pyPath = sourceCheckpoint.value(QStringLiteral("path")).toString().trimmed();
        QFileInfo pyInfo(pyPath);
        if (!pyPath.isEmpty() && !pyInfo.isAbsolute()) {
            pyInfo = QFileInfo(QDir(modelsDirectoryPath()), pyPath);
        }
        QDateTime ts = packageInfo.lastModified();
        QString display;
        QString keyPath;

        if (pyInfo.exists() && pyInfo.isFile()) {
            const int inputDim = detectPtInputDim(pyInfo.absoluteFilePath());
            if (inputDim != expectedDim) {
                continue;
            }

            ts = pyInfo.birthTime();
            if (!ts.isValid()) {
                ts = pyInfo.metadataChangeTime();
            }
            if (!ts.isValid()) {
                ts = pyInfo.lastModified();
            }

            display = devMode
                          ? QStringLiteral("%1 | %2f | %3").arg(modelVersion, QString::number(expectedDim), pyInfo.fileName())
                          : QStringLiteral("%1 | %2f | packaged").arg(modelVersion, QString::number(expectedDim));
            keyPath = pyInfo.absoluteFilePath();
        } else if (!devMode) {
            display = QStringLiteral("%1 | %2f | packaged").arg(modelVersion, QString::number(expectedDim));
            keyPath = QStringLiteral("pkg:%1").arg(modelVersion);
        } else {
            continue;
        }

        entries.push_back({display, keyPath, modelVersion, ts});
    }

    std::sort(entries.begin(), entries.end(), [](const ModelEntry& a, const ModelEntry& b) {
        if (a.timestamp == b.timestamp) {
            return a.display.toLower() < b.display.toLower();
        }
        return a.timestamp > b.timestamp;
    });

    for (const ModelEntry& entry : entries) {
        pyModelOptions_.push_back(entry.display);
        pyModelPaths_.push_back(entry.pyPath);
        nativeModelVersions_.push_back(entry.nativeVersion);
    }

    if (pyModelPath_.isEmpty() && !pyModelPaths_.isEmpty()) {
        pyModelPath_ = pyModelPaths_.first();
        nativeModelVersion_ = nativeModelVersions_.value(0);
        applySelectedModelEnv(pyModelPath_, nativeModelVersion_);
    } else if (!pyModelPath_.isEmpty() && !pyModelPaths_.contains(pyModelPath_)) {
        if (!pyModelPaths_.isEmpty()) {
            pyModelPath_ = pyModelPaths_.first();
            nativeModelVersion_ = nativeModelVersions_.value(0);
            applySelectedModelEnv(pyModelPath_, nativeModelVersion_);
        } else {
            pyModelPath_.clear();
            nativeModelVersion_.clear();
            applySelectedModelEnv(pyModelPath_, nativeModelVersion_);
        }
    } else if (!pyModelPath_.isEmpty()) {
        const int index = pyModelPaths_.indexOf(pyModelPath_);
        nativeModelVersion_ = index >= 0 ? nativeModelVersions_.value(index) : QString();
        applySelectedModelEnv(pyModelPath_, nativeModelVersion_);
    }

    emit pyModelOptionsChanged();
    if (previousPath != pyModelPath_) {
        emit pyModelPathChanged();
    }
    if (previousNativeVersion != nativeModelVersion_) {
        emit nativeModelVersionChanged();
    }
    emit pyModelIndexChanged();
}

void AppController::reloadDistributionMetadata() {
    const QString distributionDir = distributionDirectoryPath();
    const QString buildInfoPath = QDir(distributionDir).filePath(QStringLiteral("build_info.json"));
    const QString licensePath = QDir(distributionDir).filePath(QStringLiteral("license.json"));
    const QString signaturePath = QDir(distributionDir).filePath(QStringLiteral("license.sig"));

    QString nextBuildSummary = QStringLiteral("Build metadata unavailable");
    QString nextLicenseSummary = QStringLiteral("Distribution license missing");
    bool nextLicenseValid = false;

    QFile buildInfoFile(buildInfoPath);
    if (buildInfoFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(buildInfoFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject build = doc.object();
            const QString appVersion = build.value(QStringLiteral("app_version")).toString(QStringLiteral("dev"));
            const QString releaseTag = build.value(QStringLiteral("release_tag")).toString(QStringLiteral("unreleased"));
            const QString gitCommit = build.value(QStringLiteral("git_commit")).toString(QStringLiteral("unknown"));
            const QString modelVersion = build.value(QStringLiteral("model_version")).toString(QStringLiteral("unknown"));
            const QString builtAt = build.value(QStringLiteral("built_at")).toString(QStringLiteral("unknown date"));
            const QString channel = build.value(QStringLiteral("distribution_channel")).toString(QStringLiteral("local"));
            nextBuildSummary = QStringLiteral("%1 | %2 | %3 | model %4 | built %5")
                                   .arg(appVersion, releaseTag, gitCommit.left(7), modelVersion, channel);
            if (!builtAt.isEmpty()) {
                nextBuildSummary.append(QStringLiteral(" | %1").arg(builtAt));
            }
        }
    }

    QFile licenseFile(licensePath);
    QFile signatureFile(signaturePath);
    if (!licenseFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        nextLicenseSummary = QStringLiteral("Distribution license missing");
    } else if (!signatureFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        nextLicenseSummary = QStringLiteral("Distribution signature missing");
    } else {
        QJsonParseError parseError;
        const QByteArray rawLicense = licenseFile.readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(rawLicense, &parseError);
        const QByteArray signature = signatureFile.readAll().trimmed().toLower();

        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            nextLicenseSummary = QStringLiteral("Distribution license is not valid JSON");
        } else {
            const QJsonObject license = doc.object();
            const QByteArray expected = hmacSha256(licenseSigningKey(), rawLicense.trimmed());
            const QString issuedTo = license.value(QStringLiteral("issued_to")).toString(QStringLiteral("Unknown recipient"));
            const QString licenseId = license.value(QStringLiteral("license_id")).toString(QStringLiteral("unknown"));
            const QString allowedUse = license.value(QStringLiteral("allowed_use")).toString(QStringLiteral("unspecified"));
            const QString expiresAt = license.value(QStringLiteral("expires_at")).toString();
            const QString modelVersion = license.value(QStringLiteral("model_version")).toString();

            nextLicenseSummary = QStringLiteral("%1 | %2 | %3")
                                     .arg(issuedTo, licenseId, allowedUse);
            if (!modelVersion.isEmpty()) {
                nextLicenseSummary.append(QStringLiteral(" | %1").arg(modelVersion));
            }
            if (!expiresAt.isEmpty()) {
                nextLicenseSummary.append(QStringLiteral(" | expires %1").arg(expiresAt));
            }

            if (signature != expected) {
                nextLicenseSummary.prepend(QStringLiteral("Invalid signature | "));
            } else {
                bool expired = false;
                if (!expiresAt.isEmpty()) {
                    const QDateTime expiry = QDateTime::fromString(expiresAt, Qt::ISODate);
                    expired = expiry.isValid() && expiry < QDateTime::currentDateTimeUtc();
                }
                if (expired) {
                    nextLicenseSummary.prepend(QStringLiteral("Expired | "));
                } else {
                    nextLicenseValid = true;
                }
            }
        }
    }

    const bool changed = licenseValid_ != nextLicenseValid
        || licenseSummary_ != nextLicenseSummary
        || buildSummary_ != nextBuildSummary;

    licenseValid_ = nextLicenseValid;
    licenseSummary_ = nextLicenseSummary;
    buildSummary_ = nextBuildSummary;

    if (changed) {
        emit licenseMetadataChanged();
    }
}
