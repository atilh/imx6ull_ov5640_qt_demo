#include "mockvisionpipeline.h"

#include <QPainter>
#include <QPen>

VisionResult MockVisionPipeline::process(const QImage &frame, int frameIndex)
{
    VisionResult result;
    result.frame = frame;
    result.faceCount = 0;
    result.message = QStringLiteral("No face detected");

    if (result.frame.isNull()) {
        return result;
    }

    const int phase = (frameIndex / 45) % 4;
    QPainter painter(&result.frame);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const int boxW = qMax(70, result.frame.width() / 6);
    const int boxH = qMax(84, result.frame.height() / 4);

    if (phase == 1 || phase == 2 || phase == 3) {
        FaceDetection det;
        det.box = QRect(result.frame.width() / 4,
                        result.frame.height() / 5,
                        boxW,
                        boxH);
        det.label = QStringLiteral("Visitor A");
        det.confidence = 0.91f;
        det.recognized = true;
        result.detections.append(det);
        result.message = QStringLiteral("Detected 1 face");
    }

    if (phase == 3) {
        FaceDetection det;
        det.box = QRect(result.frame.width() / 2,
                        result.frame.height() / 4,
                        boxW,
                        boxH);
        det.label = QStringLiteral("Visitor B");
        det.confidence = 0.84f;
        det.recognized = false;
        result.detections.append(det);
        result.message = QStringLiteral("Detected 2 faces");
    }

    result.faceCount = result.detections.size();

    for (int i = 0; i < result.detections.size(); ++i) {
        const FaceDetection &det = result.detections.at(i);
        const QColor color = det.recognized ? QColor(0, 255, 160) : QColor(255, 196, 0);
        painter.setPen(QPen(color, 3));
        painter.drawRect(det.box);
        painter.drawText(det.box.adjusted(0, -8, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                         QStringLiteral("%1 %2%")
                         .arg(det.label)
                         .arg(static_cast<int>(det.confidence * 100.0f)));
    }

    painter.setPen(QPen(QColor(255, 255, 255), 1));
    painter.drawText(12, result.frame.height() - 14,
                     QStringLiteral("Mock vision pipeline active"));
    painter.end();

    return result;
}

QString MockVisionPipeline::pipelineName() const
{
    return QStringLiteral("mock");
}
