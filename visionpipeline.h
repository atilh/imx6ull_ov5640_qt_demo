#ifndef VISIONPIPELINE_H
#define VISIONPIPELINE_H

#include <QImage>
#include <QRect>
#include <QVector>
#include <QString>

struct FaceDetection
{
    QRect box;
    QString label;
    float confidence;
    bool recognized;
};

struct VisionResult
{
    QImage frame;
    int faceCount;
    QString message;
    QVector<FaceDetection> detections;
};

class VisionPipeline
{
public:
    virtual ~VisionPipeline() {}
    virtual VisionResult process(const QImage &frame, int frameIndex) = 0;
    virtual QString pipelineName() const = 0;
};

#endif
