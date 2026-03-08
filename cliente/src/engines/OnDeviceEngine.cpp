#include "engines/OnDeviceEngine.hpp"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>

namespace {
DecisionLevel decisionFromString(const QString& value) {
    if (value == QStringLiteral("RealLikely")) {
        return DecisionLevel::RealLikely;
    }
    if (value == QStringLiteral("AILikely")) {
        return DecisionLevel::AILikely;
    }
    if (value == QStringLiteral("AIHigh")) {
        return DecisionLevel::AIHigh;
    }
    return DecisionLevel::Inconclusive;
}

QString detectPythonExecutable() {
    const QString configured = qEnvironmentVariable("AI_AUTH_PYTHON");
    if (!configured.isEmpty()) {
        return configured;
    }

    const QString venvPy = QStringLiteral("/Users/macmini/tfg_ia_video/venv/bin/python3");
    if (QFileInfo::exists(venvPy)) {
        return venvPy;
    }
    return QStringLiteral("python3");
}

QString scriptPath() {
    return qEnvironmentVariable(
        "AI_AUTH_ONDEVICE_SCRIPT",
        QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/frontend/ai-authenticity-client/cliente/scripts/ondevice_infer.py")
    );
}

QString configuredPyImageModel() {
    return qEnvironmentVariable("AI_AUTH_PY_IMAGE_MODEL");
}
} // namespace

QString OnDeviceEngine::name() const {
    return QStringLiteral("on_device");
}

AnalysisResult OnDeviceEngine::analyzeFile(const QString& filePath) {
    AnalysisResult out;

    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        out.error = QStringLiteral("File not found.");
        return out;
    }

    const QMimeDatabase db;
    const QString mimeType = db.mimeTypeForFile(info).name();
    if (!mimeType.startsWith(QStringLiteral("image/"))) {
        out.error = QStringLiteral("Only image files are supported in on-device mode right now.");
        return out;
    }

    const QString py = detectPythonExecutable();
    const QString script = scriptPath();

    if (!QFileInfo::exists(script)) {
        out.error = QStringLiteral("On-device script not found: %1").arg(script);
        return out;
    }

    QProcess process;
    QStringList args{script, QStringLiteral("--file"), filePath};
    const QString modelPath = configuredPyImageModel().trimmed();
    if (!modelPath.isEmpty()) {
        args << QStringLiteral("--image-model") << modelPath;
    }
    process.start(py, args);

    if (!process.waitForStarted(5000)) {
        out.error = QStringLiteral("Failed to start on-device process.");
        return out;
    }

    if (!process.waitForFinished(60000)) {
        process.kill();
        out.error = QStringLiteral("On-device inference timed out.");
        return out;
    }

    const QByteArray stdoutData = process.readAllStandardOutput().trimmed();
    const QByteArray stderrData = process.readAllStandardError().trimmed();

    if (stdoutData.isEmpty()) {
        out.error = QStringLiteral("On-device process returned no output. %1").arg(QString::fromUtf8(stderrData));
        return out;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stdoutData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = QStringLiteral("Invalid JSON from on-device process: %1").arg(QString::fromUtf8(stdoutData));
        return out;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("ok")).isBool() && !root.value(QStringLiteral("ok")).toBool()) {
        out.error = root.value(QStringLiteral("error")).toString(QStringLiteral("On-device process failed."));
        return out;
    }

    const QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
    const QJsonObject scores = root.value(QStringLiteral("scores")).toObject();
    const QJsonArray debugFeatures = root.value(QStringLiteral("debug_features")).toArray();

    out.ok = true;
    out.aiProbabilityPct = root.value(QStringLiteral("ai_probability")).toDouble(0.0);
    out.explanation = root.value(QStringLiteral("explanation")).toString(QStringLiteral("-"));
    out.mediaType = metadata.value(QStringLiteral("media_type")).toString(QStringLiteral("image"));
    out.decision = decisionFromString(metadata.value(QStringLiteral("decision")).toString());
    out.modelLabel = metadata.value(QStringLiteral("model_filename")).toString(metadata.value(QStringLiteral("model_used")).toString(QStringLiteral("ImageNet")));
    out.modelPath = metadata.value(QStringLiteral("model_path")).toString(modelPath);
    out.modelSha256 = metadata.value(QStringLiteral("model_sha256")).toString(QStringLiteral("N/A"));
    out.modelLoaded = metadata.value(QStringLiteral("model_loaded")).toString(QStringLiteral("unknown"));
    const QString modelError = metadata.value(QStringLiteral("model_error")).toString();
    if (out.modelLoaded != QStringLiteral("true")) {
        out.error = modelError.isEmpty()
                        ? QStringLiteral("Selected Python model was not loaded by backend runtime.")
                        : QStringLiteral("Selected Python model was not loaded: %1").arg(modelError);
        out.ok = false;
        return out;
    }
    out.scores.spectral = scores.value(QStringLiteral("spectral")).toDouble(0.0);
    out.scores.visual = scores.value(QStringLiteral("visual")).toDouble(0.0);
    out.scores.temporal = scores.value(QStringLiteral("temporal")).toDouble(0.0);
    out.scores.aiModelRaw = scores.value(QStringLiteral("ai_model")).toDouble(0.0);
    for (const QJsonValue& v : debugFeatures) {
        out.debugFeatures.push_back(v.toDouble(0.0));
    }

    return out;
}
