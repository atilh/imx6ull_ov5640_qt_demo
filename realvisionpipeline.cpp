#include "realvisionpipeline.h"

#include "simplefacedetectorbackend.h"

#include <QPainter>
#include <QPen>

RealVisionPipeline::RealVisionPipeline()
    : m_backend(new SimpleFaceDetectorBackend)
{
}

RealVisionPipeline::~RealVisionPipeline()
{
    delete m_backend;
    m_backend = nullptr;
}

VisionResult RealVisionPipeline::process(const QImage &frame, int frameIndex)
{
    VisionResult result;
    result.frame = frame;
    result.faceCount = 0;
    result.message = QStringLiteral("Real detector idle");

    if (result.frame.isNull()) {
        result.message = QStringLiteral("Input frame is empty");
        return result;
    }

    const QImage preparedFrame = prepareInputFrame(result.frame);
    result.detections = inferDetections(preparedFrame, frameIndex);
    result.faceCount = result.detections.size();
    result.message = buildStatusMessage(result.detections);
    drawDetections(&result.frame, result.detections);
    return result;
}

QString RealVisionPipeline::pipelineName() const
{
    return QStringLiteral("real");
}

QImage RealVisionPipeline::prepareInputFrame(const QImage &frame) const
{
    return frame.convertToFormat(QImage::Format_RGB888);
}

QVector<FaceDetection> RealVisionPipeline::inferDetections(const QImage &preparedFrame, int frameIndex) const
{
    Q_UNUSED(frameIndex);

    if (!m_backend) {
        return QVector<FaceDetection>();
    }

    return m_backend->detect(preparedFrame);
}

QString RealVisionPipeline::buildStatusMessage(const QVector<FaceDetection> &detections) const
{
    if (!m_backend) {
        return QStringLiteral("Real detector unavailable");
    }

    if (detections.isEmpty()) {
        return QStringLiteral("%1: no face detected").arg(m_backend->backendName());
    }

    return QStringLiteral("%1: %2 face candidate(s)")
            .arg(m_backend->backendName())
            .arg(detections.size());
}

void RealVisionPipeline::drawDetections(QImage *frame, const QVector<FaceDetection> &detections) const
{
    if (!frame || frame->isNull()) {
        return;
    }

    QPainter painter(frame);
    painter.setRenderHint(QPainter::Antialiasing, false);

    for (int i = 0; i < detections.size(); ++i) {
        const FaceDetection &det = detections.at(i);
        const QColor color = det.recognized ? QColor(0, 255, 160) : QColor(255, 80, 80);
        painter.setPen(QPen(color, 3));
        painter.drawRect(det.box);
        painter.drawText(det.box.adjusted(0, -8, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                         QStringLiteral("%1 %2%")
                         .arg(det.label.isEmpty() ? QStringLiteral("Unknown") : det.label)
                         .arg(static_cast<int>(det.confidence * 100.0f)));
    }

    painter.setPen(QPen(QColor(255, 255, 255), 1));
    painter.drawText(12, frame->height() - 14,
                     QStringLiteral("Real backend: %1")
                     .arg(m_backend ? m_backend->backendName() : QStringLiteral("none")));
    painter.end();
}
