#ifndef SIMPLEFACEDETECTORBACKEND_H
#define SIMPLEFACEDETECTORBACKEND_H

#include "facedetectorbackend.h"

class SimpleFaceDetectorBackend : public FaceDetectorBackend
{
public:
    QVector<FaceDetection> detect(const QImage &frame) const override;
    QString backendName() const override;
};

#endif
