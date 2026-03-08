#include "engines/ApiEngine.hpp"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {
QString normalizedBaseUrl() {
    QString base = qEnvironmentVariable("AI_AUTH_API_BASE_URL", QStringLiteral("http://127.0.0.1:8000"));
    while (base.endsWith('/')) {
        base.chop(1);
    }
    return base;
}
} // namespace

QString ApiEngine::name() const {
    return QStringLiteral("api");
}

QUrl ApiEngine::endpointUrl() {
    return QUrl(normalizedBaseUrl() + QStringLiteral("/analyze/image"));
}

DecisionLevel ApiEngine::decisionFromString(const QString& value) {
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

AnalysisResult ApiEngine::analyzeFile(const QString& filePath) {
    AnalysisResult out;

    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        out.error = QStringLiteral("File not found.");
        return out;
    }
    const QMimeDatabase db;
    const QString mimeType = db.mimeTypeForFile(info).name();
    if (!mimeType.startsWith(QStringLiteral("image/"))) {
        out.error = QStringLiteral("Only image files are supported in API mode right now.");
        return out;
    }

    auto* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        out.error = QStringLiteral("Cannot open file for upload.");
        file->deleteLater();
        return out;
    }

    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(info.fileName()));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkRequest request(endpointUrl());
    QNetworkReply* reply = network_.post(request, multiPart);
    multiPart->setParent(reply);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, [&]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timeout.start(30000);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        out.error = QStringLiteral("API request failed: %1").arg(reply->errorString());
        reply->deleteLater();
        return out;
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = QStringLiteral("Invalid JSON response from backend.");
        return out;
    }

    const QJsonObject root = doc.object();
    if (root.contains("detail")) {
        out.error = root.value("detail").toString(QStringLiteral("Backend error."));
        return out;
    }

    const QJsonObject metadata = root.value("metadata").toObject();
    const QJsonObject scores = root.value("scores").toObject();

    out.ok = true;
    out.aiProbabilityPct = root.value("ai_probability").toDouble(0.0);
    out.explanation = root.value("explanation").toString(QStringLiteral("-"));
    out.mediaType = metadata.value("media_type").toString(QStringLiteral("unknown"));
    out.decision = decisionFromString(metadata.value("decision").toString());
    out.modelLabel = metadata.value("model_filename").toString(metadata.value("model_used").toString(QStringLiteral("backend")));
    out.modelPath = metadata.value("model_path").toString(QStringLiteral("N/A"));
    out.modelSha256 = metadata.value("model_sha256").toString(QStringLiteral("N/A"));
    out.modelLoaded = metadata.value("model_loaded").toString(QStringLiteral("unknown"));
    const QString modelError = metadata.value("model_error").toString();
    if (out.modelLoaded != QStringLiteral("true")) {
        out.error = modelError.isEmpty()
                        ? QStringLiteral("Backend model was not loaded.")
                        : QStringLiteral("Backend model was not loaded: %1").arg(modelError);
        out.ok = false;
        return out;
    }
    out.scores.spectral = scores.value("spectral").toDouble(0.0);
    out.scores.visual = scores.value("visual").toDouble(0.0);
    out.scores.temporal = scores.value("temporal").toDouble(0.0);
    out.scores.aiModelRaw = scores.value("ai_model").toDouble(0.0);

    return out;
}
