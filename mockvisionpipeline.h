#ifndef MOCKVISIONPIPELINE_H
#define MOCKVISIONPIPELINE_H

#include "visionpipeline.h"

class MockVisionPipeline : public VisionPipeline
{
public:
    VisionResult process(const QImage &frame, int frameIndex) override;
    QString pipelineName() const override;
};

#endif
