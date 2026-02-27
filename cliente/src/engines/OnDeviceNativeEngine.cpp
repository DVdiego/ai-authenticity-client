#include "engines/OnDeviceNativeEngine.hpp"

#include <onnxruntime/onnxruntime_cxx_api.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <QFileInfo>
#include <QMimeDatabase>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

namespace {
constexpr int kNormSize = 512;
constexpr double kEps = 1e-6;
constexpr double kLowConfThreshold = 0.40;
constexpr double kHighConfThreshold = 0.60;
constexpr double kModelMaxConfidence = 0.81;

constexpr double kSpectralRanges[5][2] = {
    {0.0, 30.0}, {0.0, 30.0}, {0.0, 2.0}, {5.0, 15.0}, {0.0, 10.0}
};
constexpr double kVisualRanges[7][2] = {
    {0.0, 50.0}, {0.0, 70.0}, {0.0, 2500.0}, {0.0, 0.5}, {0.0, 60.0}, {0.0, 100.0}, {0.0, 1000.0}
};

std::string modelPath() {
    const QByteArray env = qgetenv("AI_AUTH_IMAGE_ONNX");
    if (!env.isEmpty()) {
        return env.toStdString();
    }
    return "/Users/macmini/tfg_ia_video/desarrollo/backend/models/image_net.onnx";
}

double clamp01(double v) {
    return std::max(0.0, std::min(1.0, v));
}

double normRange(double v, double lo, double hi) {
    return clamp01((v - lo) / (hi - lo + kEps));
}

cv::Mat squareCropResize(const cv::Mat& img, int size = kNormSize) {
    const int h = img.rows;
    const int w = img.cols;
    const int side = std::min(h, w);
    const int y0 = std::max(0, (h - side) / 2);
    const int x0 = std::max(0, (w - side) / 2);
    cv::Mat crop = img(cv::Rect(x0, y0, side, side));

    cv::Mat out;
    const int interp = side > size ? cv::INTER_AREA : cv::INTER_CUBIC;
    cv::resize(crop, out, cv::Size(size, size), 0.0, 0.0, interp);
    return out;
}

double meanOfMat(const cv::Mat& m) {
    return cv::mean(m)[0];
}

double computeElaScore(const cv::Mat& imgBgr, int quality = 90) {
    try {
        std::vector<uchar> enc;
        const std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
        if (!cv::imencode(".jpg", imgBgr, enc, params)) {
            return 0.0;
        }
        cv::Mat dec = cv::imdecode(enc, cv::IMREAD_COLOR);
        if (dec.empty()) {
            return 0.0;
        }
        cv::Mat diff;
        cv::absdiff(imgBgr, dec, diff);
        const cv::Scalar m = cv::mean(diff);
        return (m[0] + m[1] + m[2]) / 3.0;
    } catch (...) {
        return 0.0;
    }
}

std::vector<double> spectralAnalysisCore(const cv::Mat& frameBgr) {
    cv::Mat gray;
    cv::cvtColor(frameBgr, gray, cv::COLOR_BGR2GRAY);

    const int h = gray.rows;
    const int w = gray.cols;

    std::array<cv::Mat, 4> patches = {
        gray(cv::Rect(0, 0, w / 2, h / 2)),
        gray(cv::Rect(w / 2, 0, w - (w / 2), h / 2)),
        gray(cv::Rect(0, h / 2, w / 2, h - (h / 2))),
        gray(cv::Rect(w / 2, h / 2, w - (w / 2), h - (h / 2))),
    };

    std::vector<double> patchEnergies;
    patchEnergies.reserve(4);

    for (const cv::Mat& p : patches) {
        cv::Mat p32;
        p.convertTo(p32, CV_32F);

        cv::Mat dftOut;
        cv::dft(p32, dftOut, cv::DFT_COMPLEX_OUTPUT);

        std::vector<cv::Mat> c(2);
        cv::split(dftOut, c);
        cv::Mat mag;
        cv::magnitude(c[0], c[1], mag);
        mag += 1e-8f;
        cv::log(mag, mag);
        mag *= 20.0f;

        patchEnergies.push_back(meanOfMat(mag));
    }

    cv::Mat gray32;
    gray.convertTo(gray32, CV_32F);
    cv::Mat dftGlobal;
    cv::dft(gray32, dftGlobal, cv::DFT_COMPLEX_OUTPUT);

    std::vector<cv::Mat> cg(2);
    cv::split(dftGlobal, cg);
    cv::Mat magGlobal;
    cv::magnitude(cg[0], cg[1], magGlobal);
    magGlobal += 1e-8f;
    cv::log(magGlobal, magGlobal);
    magGlobal *= 20.0f;

    double minVal = 0.0;
    double maxVal = 0.0;
    cv::minMaxLoc(magGlobal, &minVal, &maxVal);
    if (std::abs(maxVal - minVal) < 1e-9) {
        maxVal = minVal + 1.0;
    }

    cv::Mat hist;
    const int histSize[] = {256};
    const float range[] = {static_cast<float>(minVal), static_cast<float>(maxVal)};
    const float* ranges[] = {range};
    const int channels[] = {0};
    cv::calcHist(&magGlobal, 1, channels, cv::Mat(), hist, 1, histSize, ranges, true, false);

    const double total = magGlobal.total();
    const double binWidth = (maxVal - minVal) / 256.0;
    double entropy = 0.0;
    for (int i = 0; i < 256; ++i) {
        const double count = hist.at<float>(i);
        const double density = count / (total * (binWidth + 1e-12));
        entropy += -density * std::log2(density + 1e-12);
    }

    cv::Scalar meanS, stddevS;
    cv::meanStdDev(magGlobal, meanS, stddevS);
    const double meanGlobal = meanS[0];
    const double varGlobal = stddevS[0] * stddevS[0];

    return {
        meanGlobal,
        *std::max_element(patchEnergies.begin(), patchEnergies.end()),
        varGlobal / 100.0,
        entropy,
        maxVal / (meanGlobal + 1e-6),
    };
}

std::vector<double> visualAnalysisCore(const cv::Mat& frameBgr) {
    cv::Mat gray;
    cv::cvtColor(frameBgr, gray, cv::COLOR_BGR2GRAY);

    const double elaGlobal = computeElaScore(frameBgr, 90);

    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_64F);
    cv::Scalar lapMean, lapStd;
    cv::meanStdDev(lap, lapMean, lapStd);
    const double lapVar = lapStd[0] * lapStd[0];

    const int h = frameBgr.rows;
    const int w = frameBgr.cols;
    const cv::Mat p1 = frameBgr(cv::Rect(0, 0, w / 2, h / 2));
    const double elaPatchMax = std::max(elaGlobal, computeElaScore(p1, 90));

    cv::Mat edges;
    cv::Canny(gray, edges, 100, 200);
    const double edgeDensity = cv::sum(edges)[0] / (static_cast<double>(gray.total()) + 1e-6);

    cv::Mat blur;
    cv::GaussianBlur(gray, blur, cv::Size(5, 5), 0);
    cv::Mat absd;
    cv::absdiff(gray, blur, absd);
    const double noiseEst = meanOfMat(absd);

    std::vector<cv::Mat> bgr;
    cv::split(frameBgr, bgr);
    cv::Scalar mb, sb, mg, sg, mr, sr;
    cv::meanStdDev(bgr[0], mb, sb);
    cv::meanStdDev(bgr[1], mg, sg);
    cv::meanStdDev(bgr[2], mr, sr);
    const double chromaStd = (sb[0] + sg[0] + sr[0]) / 3.0;

    cv::Mat sobel;
    cv::Sobel(gray, sobel, CV_64F, 1, 1, 5);
    cv::Scalar ms, ss;
    cv::meanStdDev(sobel, ms, ss);
    const double textureSmoothness = ss[0];

    return {
        elaGlobal,
        elaPatchMax,
        lapVar,
        edgeDensity,
        noiseEst,
        chromaStd,
        textureSmoothness,
    };
}

std::vector<double> extractImageFeatures(const cv::Mat& inputBgr) {
    const cv::Mat img = squareCropResize(inputBgr, kNormSize);

    const std::vector<double> sRaw = spectralAnalysisCore(img);
    const std::vector<double> vRaw = visualAnalysisCore(img);

    std::vector<double> features;
    features.reserve(12);

    for (size_t i = 0; i < sRaw.size(); ++i) {
        features.push_back(normRange(sRaw[i], kSpectralRanges[i][0], kSpectralRanges[i][1]));
    }
    for (size_t i = 0; i < vRaw.size(); ++i) {
        features.push_back(normRange(vRaw[i], kVisualRanges[i][0], kVisualRanges[i][1]));
    }
    return features;
}

DecisionLevel decide(double rawScore) {
    if (rawScore < kLowConfThreshold) {
        return DecisionLevel::RealLikely;
    }
    if (rawScore < kHighConfThreshold) {
        return DecisionLevel::Inconclusive;
    }
    if (rawScore >= kModelMaxConfidence) {
        return DecisionLevel::AIHigh;
    }
    return DecisionLevel::AILikely;
}

std::string explain(double rawScore, const std::vector<double>& features) {
    const double spectral = std::accumulate(features.begin(), features.begin() + 5, 0.0) / 5.0;
    const double visual = std::accumulate(features.begin() + 5, features.end(), 0.0) / 7.0;

    if (rawScore < kLowConfThreshold) {
        return "The image shows high structural consistency. Noise patterns are typical of physical sensors.";
    }
    if (rawScore < kHighConfThreshold) {
        std::string reasons;
        if (spectral > 0.60) {
            reasons += "compression / frequency noise";
        }
        if (visual < 0.45) {
            if (!reasons.empty()) {
                reasons += ", ";
            }
            reasons += "over-smooth textures";
        }
        if (!reasons.empty()) {
            reasons = " (" + reasons + ")";
        }
        return "Analysis is inconclusive" + reasons +
               ". We detected subtle artifacts that are typical of post-processing."
               " This can happen when a real photo has been edited with enhancement or retouching tools"
               " (including AI-assisted edits that modify small regions like eyes, skin, or facial hair),"
               " or when the file has been re-exported with strong compression, resizing, filtering, denoise, or sharpening."
               " Similar micro-patterns can also appear in fully AI-generated media, so this score alone is not definitive."
               " For a clearer result, compare with the original capture or a minimally edited export.";
    }
    if (rawScore < kModelMaxConfidence) {
        return "Suspicious image: Detected frequency and texture patterns commonly found in AI-generated media.";
    }
    return "High probability of AI generation. The image exhibits strong mathematical signatures typical of Generative Models.";
}

float runOnnx(const std::vector<double>& features) {
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "native_image_engine");
    static Ort::SessionOptions options;
    static bool initialized = false;
    static std::unique_ptr<Ort::Session> session;
    static std::string inputName;
    static std::string outputName;

    if (!initialized) {
        options.SetIntraOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session = std::make_unique<Ort::Session>(env, modelPath().c_str(), options);

        Ort::AllocatorWithDefaultOptions allocator;
        auto in = session->GetInputNameAllocated(0, allocator);
        auto out = session->GetOutputNameAllocated(0, allocator);
        inputName = in ? in.get() : "features";
        outputName = out ? out.get() : "probability";

        initialized = true;
    }

    std::vector<float> x(features.begin(), features.end());
    std::array<int64_t, 2> shape = {1, static_cast<int64_t>(x.size())};
    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(mem, x.data(), x.size(), shape.data(), shape.size());

    const char* inNames[] = {inputName.c_str()};
    const char* outNames[] = {outputName.c_str()};
    auto outputs = session->Run(Ort::RunOptions{nullptr}, inNames, &inputTensor, 1, outNames, 1);

    const float* outData = outputs[0].GetTensorData<float>();
    return outData ? std::clamp(outData[0], 0.0f, 1.0f) : 0.0f;
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

    const cv::Mat img = cv::imread(filePath.toStdString(), cv::IMREAD_COLOR);
    if (img.empty()) {
        out.error = QStringLiteral("Unable to load image with OpenCV.");
        return out;
    }

    std::vector<double> features;
    try {
        features = extractImageFeatures(img);
    } catch (const std::exception& ex) {
        out.error = QStringLiteral("Native feature extraction error: %1").arg(ex.what());
        return out;
    }

    if (features.size() != 12) {
        out.error = QStringLiteral("Native feature extraction failed.");
        return out;
    }

    float raw = 0.0f;
    try {
        raw = runOnnx(features);
    } catch (const std::exception& ex) {
        out.error = QStringLiteral("ONNX inference error: %1").arg(ex.what());
        return out;
    }

    out.ok = true;
    out.mediaType = QStringLiteral("image");
    out.scores.aiModelRaw = raw;
    out.aiProbabilityPct = std::min(static_cast<double>(raw) * 100.0, 99.0);
    out.decision = decide(raw);
    out.scores.spectral = std::accumulate(features.begin(), features.begin() + 5, 0.0) / 5.0;
    out.scores.visual = std::accumulate(features.begin() + 5, features.end(), 0.0) / 7.0;
    out.scores.temporal = 0.0;
    out.explanation = QString::fromStdString(explain(raw, features));

    return out;
}

bool OnDeviceNativeEngine::extractFeaturesForDebug(const QString& filePath, std::vector<double>& outFeatures, QString& error) {
    outFeatures.clear();

    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        error = QStringLiteral("File not found.");
        return false;
    }

    const QMimeDatabase db;
    const QString mimeType = db.mimeTypeForFile(info).name();
    if (!mimeType.startsWith(QStringLiteral("image/"))) {
        error = QStringLiteral("Only image files are supported.");
        return false;
    }

    const cv::Mat img = cv::imread(filePath.toStdString(), cv::IMREAD_COLOR);
    if (img.empty()) {
        error = QStringLiteral("Unable to load image with OpenCV.");
        return false;
    }

    try {
        outFeatures = extractImageFeatures(img);
    } catch (const std::exception& ex) {
        error = QStringLiteral("Native feature extraction error: %1").arg(ex.what());
        return false;
    }

    if (outFeatures.size() != 12) {
        error = QStringLiteral("Native feature extraction returned invalid size.");
        return false;
    }
    return true;
}
