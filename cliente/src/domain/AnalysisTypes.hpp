#pragma once

#include <QString>

struct AnalysisScores {
    double spectral = 0.0;
    double visual = 0.0;
    double temporal = 0.0;
    double aiModelRaw = 0.0;
};

enum class DecisionLevel {
    RealLikely,
    Inconclusive,
    AILikely,
    AIHigh,
};

struct AnalysisResult {
    bool ok = false;
    QString mediaType;
    double aiProbabilityPct = 0.0;
    DecisionLevel decision = DecisionLevel::Inconclusive;
    QString explanation;
    AnalysisScores scores;
    QString error;
};

inline QString decisionToString(DecisionLevel d) {
    switch (d) {
    case DecisionLevel::RealLikely:
        return QStringLiteral("RealLikely");
    case DecisionLevel::Inconclusive:
        return QStringLiteral("Inconclusive");
    case DecisionLevel::AILikely:
        return QStringLiteral("AILikely");
    case DecisionLevel::AIHigh:
        return QStringLiteral("AIHigh");
    }
    return QStringLiteral("Inconclusive");
}
