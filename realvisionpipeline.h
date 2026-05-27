#ifndef REALVISIONPIPELINE_H
#define REALVISIONPIPELINE_H

#include "facedetectorbackend.h"
#include "visionpipeline.h"

class RealVisionPipeline : public VisionPipeline
{
public:
    RealVisionPipeline();
    ~RealVisionPipeline();

    VisionResult process(const QImage &frame, int frameIndex) override;
    QString pipelineName() const override;

private:
    QImage prepareInputFrame(const QImage &frame) const;
    QVector<FaceDetection> inferDetections(const QImage &preparedFrame, int frameIndex) const;
    QString buildStatusMessage(const QVector<FaceDetection> &detections) const;
    void drawDetections(QImage *frame, const QVector<FaceDetection> &detections) const;

    FaceDetectorBackend *m_backend;
};

#endif
