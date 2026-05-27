#ifndef FACEDETECTORBACKEND_H
#define FACEDETECTORBACKEND_H

#include "visionpipeline.h"

class FaceDetectorBackend
{
public:
    virtual ~FaceDetectorBackend() {}
    virtual QVector<FaceDetection> detect(const QImage &frame) const = 0;
    virtual QString backendName() const = 0;
};

#endif
