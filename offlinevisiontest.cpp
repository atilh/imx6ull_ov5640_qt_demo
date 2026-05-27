#include "offlinevisiontest.h"

#include "realvisionpipeline.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>

namespace {

void printUsage()
{
    QTextStream out(stdout);
    out << "Usage:\n";
    out << "  ./imx6ull_ov5640_qt_demo --offline-test <input-image> <output-image>\n";
}

}

int runOfflineVisionTest(const QStringList &arguments)
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    if (arguments.size() != 4) {
        printUsage();
        return 1;
    }

    const QString inputPath = arguments.at(2);
    const QString outputPath = arguments.at(3);

    QImage inputImage(inputPath);
    if (inputImage.isNull()) {
        err << "Failed to load input image: " << inputPath << "\n";
        return 2;
    }

    RealVisionPipeline pipeline;
    const VisionResult result = pipeline.process(inputImage, 0);

    QFileInfo outputInfo(outputPath);
    QDir outputDir = outputInfo.dir();
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        err << "Failed to create output directory: " << outputDir.absolutePath() << "\n";
        return 3;
    }

    if (!result.frame.save(outputPath)) {
        err << "Failed to save output image: " << outputPath << "\n";
        return 4;
    }

    out << "Pipeline: " << pipeline.pipelineName() << "\n";
    out << "Input: " << inputPath << "\n";
    out << "Output: " << outputPath << "\n";
    out << "Faces: " << result.faceCount << "\n";
    out << "Message: " << result.message << "\n";

    for (int i = 0; i < result.detections.size(); ++i) {
        const FaceDetection &det = result.detections.at(i);
        out << "Detection[" << i << "]: "
            << "x=" << det.box.x()
            << ", y=" << det.box.y()
            << ", w=" << det.box.width()
            << ", h=" << det.box.height()
            << ", label=" << det.label
            << ", confidence=" << det.confidence
            << ", recognized=" << (det.recognized ? "true" : "false")
            << "\n";
    }

    return 0;
}
