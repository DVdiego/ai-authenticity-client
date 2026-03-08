#include "engines/OnDeviceNativeEngine.hpp"

#include <runtime_api.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QStringList>

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

namespace {
constexpr int kSpectralFeatures = 24;
constexpr int kVisualFeatures = 32;

QString appResourcesDirectoryPath() {
    const QString candidate = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources"));
    const QFileInfo info(candidate);
    return (info.exists() && info.isDir()) ? candidate : QString();
}

double envDouble(const char* name, double fallback) {
    bool ok = false;
    const double value = qEnvironmentVariable(name).toDouble(&ok);
    return ok ? value : fallback;
}

double lowConfThreshold() {
    return std::clamp(envDouble("TFG_LOW_CONF_THRESHOLD", 0.40), 0.0, 1.0);
}

double highConfThreshold() {
    return std::clamp(envDouble("TFG_HIGH_CONF_THRESHOLD", envDouble("TFG_DEFAULT_THRESHOLD", 0.60)), 0.0, 1.0);
}

double modelMaxConfidence() {
    return std::clamp(envDouble("TFG_MODEL_MAX_CONFIDENCE", 0.81), 0.0, 1.0);
}

QString nativeModelVersion() {
    return qEnvironmentVariable("AI_AUTH_NATIVE_MODEL_VERSION", QStringLiteral("image_featurenet_v1"));
}

QString defaultModelPackageDir() {
    const QString resourcesDir = appResourcesDirectoryPath();
    if (!resourcesDir.isEmpty()) {
        const QString bundled = QDir(resourcesDir).filePath(QStringLiteral("model-package"));
        if (QFileInfo::exists(bundled)) {
            return bundled;
        }
    }
    return QStringLiteral("/Users/macmini/tfg_ia_video/desarrollo/ai-authenticity-mobile/model-package");
}

QString modelPackageDir() {
    const QString configured = qEnvironmentVariable("AIAUTH_MODEL_PACKAGE_DIR").trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }
    return defaultModelPackageDir();
}

bool ensureModelPackageDir(QString& error) {
    const QString dir = modelPackageDir();
    const QFileInfo info(dir);
    if (!info.exists() || !info.isDir()) {
        error = QStringLiteral("Native model package directory not found: %1").arg(dir);
        return false;
    }
    qputenv("AIAUTH_MODEL_PACKAGE_DIR", dir.toUtf8());
    return true;
}

QString packageModelPath(const QString& modelVersion) {
    return QStringLiteral("%1/image/%2/model.onnx").arg(modelPackageDir(), modelVersion);
}

std::string sha256File(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return "N/A";
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        hash.addData(f.read(1024 * 1024));
    }
    return hash.result().toHex().toStdString();
}

DecisionLevel decisionFromRaw(double rawScore) {
    if (rawScore < lowConfThreshold()) {
        return DecisionLevel::RealLikely;
    }
    if (rawScore < highConfThreshold()) {
        return DecisionLevel::Inconclusive;
    }
    if (rawScore >= modelMaxConfidence()) {
        return DecisionLevel::AIHigh;
    }
    return DecisionLevel::AILikely;
}

QString explanationFromRaw(double rawScore, double spectralScore, double visualScore) {
    if (rawScore < lowConfThreshold()) {
        return QStringLiteral(
            "The image shows high structural consistency. Noise patterns are typical of physical sensors."
        );
    }
    if (rawScore < highConfThreshold()) {
        QStringList reasons;
        if (spectralScore > 0.60) {
            reasons << QStringLiteral("compression / frequency noise");
        }
        if (visualScore < 0.45) {
            reasons << QStringLiteral("over-smooth textures");
        }
        const QString detail = reasons.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(reasons.join(QStringLiteral(", ")));
        return QStringLiteral(
            "Analysis is inconclusive%1. We detected subtle artifacts that are typical of post-processing. "
            "This can happen when a real photo has been edited with enhancement or retouching tools "
            "(including AI-assisted edits that modify small regions like eyes, skin, or facial hair), "
            "or when the file has been re-exported with strong compression, resizing, filtering, denoise, or sharpening. "
            "Similar micro-patterns can also appear in fully AI-generated media, so this score alone is not definitive. "
            "For a clearer result, compare with the original capture or a minimally edited export."
        ).arg(detail);
    }
    if (rawScore < modelMaxConfidence()) {
        return QStringLiteral(
            "Suspicious image: Detected frequency and texture patterns commonly found in AI-generated media."
        );
    }
    return QStringLiteral(
        "High probability of AI generation. The image exhibits strong mathematical signatures typical of Generative Models."
    );
}

double scoreSlice(const std::vector<double>& features, size_t begin, size_t count) {
    if (features.size() < begin + count || count == 0) {
        return 0.0;
    }
    const auto start = features.begin() + static_cast<std::ptrdiff_t>(begin);
    const auto end = start + static_cast<std::ptrdiff_t>(count);
    return std::accumulate(start, end, 0.0) / static_cast<double>(count);
}

bool analyzeNativeImage(const QString& filePath, AnalysisResult& out, QString* errorOut = nullptr) {
    QString error;
    if (!ensureModelPackageDir(error)) {
        if (errorOut != nullptr) {
            *errorOut = error;
        }
        out.error = error;
        return false;
    }

    const QString modelVersion = nativeModelVersion();
    const QString resolvedModelPath = packageModelPath(modelVersion);
    if (!QFileInfo::exists(resolvedModelPath)) {
        error = QStringLiteral("Native model package missing model.onnx: %1").arg(resolvedModelPath);
        if (errorOut != nullptr) {
            *errorOut = error;
        }
        out.error = error;
        return false;
    }

    ai_authenticity::RuntimeApi api;
    ai_authenticity::AnalysisRequest request;
    request.file_path = filePath.toStdString();
    request.model_version = modelVersion.toStdString();
    request.media_type = "image";

    const ai_authenticity::AnalysisResult native = api.AnalyzeImage(request);
    if (!native.ok) {
        error = QString::fromStdString(
            native.error_message.empty() ? native.error_code : native.error_message
        );
        if (error.isEmpty()) {
            error = QStringLiteral("Native runtime failed.");
        }
        if (errorOut != nullptr) {
            *errorOut = error;
        }
        out.error = error;
        out.modelLabel = modelVersion;
        out.modelPath = resolvedModelPath;
        out.modelLoaded = QStringLiteral("false");
        return false;
    }

    out.ok = true;
    out.mediaType = QStringLiteral("image");
    out.aiProbabilityPct = native.ai_probability;
    out.modelLabel = QString::fromStdString(native.model_version.empty() ? modelVersion.toStdString() : native.model_version);
    out.modelPath = resolvedModelPath;
    out.modelSha256 = QString::fromStdString(sha256File(resolvedModelPath));
    out.modelLoaded = QStringLiteral("true");
    out.debugFeatures = native.features;
    out.scores.aiModelRaw = native.raw_score;
    out.scores.spectral = scoreSlice(native.features, 0, kSpectralFeatures);
    out.scores.visual = scoreSlice(native.features, kSpectralFeatures, kVisualFeatures);
    out.scores.temporal = 0.0;
    out.decision = decisionFromRaw(native.raw_score);
    out.explanation = explanationFromRaw(native.raw_score, out.scores.spectral, out.scores.visual);
    return true;
}
} // namespace

QString OnDeviceNativeEngine::name() const {
    return QStringLiteral("on_device_native");
}

AnalysisResult OnDeviceNativeEngine::analyzeFile(const QString& filePath) {
    AnalysisResult out;

    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        out.error = QStringLiteral("File not found.");
        return out;
    }

    const QMimeDatabase db;
    const QString mimeType = db.mimeTypeForFile(info).name();
    if (!mimeType.startsWith(QStringLiteral("image/"))) {
        out.error = QStringLiteral("Only image files are supported in native on-device mode right now.");
        return out;
    }

    analyzeNativeImage(filePath, out);
    return out;
}

bool OnDeviceNativeEngine::extractFeaturesForDebug(const QString& filePath, std::vector<double>& outFeatures, QString& error) {
    outFeatures.clear();

    AnalysisResult out;
    if (!analyzeNativeImage(filePath, out, &error)) {
        return false;
    }

    outFeatures = out.debugFeatures;
    return true;
}
