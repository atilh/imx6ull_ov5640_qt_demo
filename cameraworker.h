#ifndef CAMERAWORKER_H
#define CAMERAWORKER_H

#include "visionpipeline.h"

#include <QImage>
#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <linux/videodev2.h>

#include "mjpegserver.h"

class CameraWorker : public QObject
{
    Q_OBJECT

public:
    explicit CameraWorker(QObject *parent = nullptr);
    ~CameraWorker();

public slots:
    void startPreview(const QString &devicePath = "/dev/video1",
                      int width = 800, int height = 480);
    void stopPreview();
    void saveCurrentFrame(const QString &filePath);
    void saveEnrollmentSample(const QString &dirPath);
    void notifyFrameDisplayed();

signals:
    void frameReady(const QImage &image);
    void statusChanged(const QString &message);
    void frameStatsChanged(int frameCount, int fpsTimes10);
    void detectionStateChanged(int faceCount, const QString &message);
    void errorOccurred(const QString &message);
    void previewStopped();

private:
    VisionPipeline *createPipeline() const;
    struct VideoBuffer {
        void *start;
        size_t length;
    };

    static const unsigned int kBufferCount = 4;

    bool openDevice(const QString &devicePath, int width, int height);
    bool initMmap();
    bool startStreaming();
    void stopStreaming();
    void closeDevice();
    bool readOneFrame();
    QImage runVisionPipeline(const QImage &frame);
    void resetStats();
    void publishStats();
    static int xioctl(int fd, int request, void *arg);

    int m_videoFd;
    bool m_running;
    int m_width;
    int m_height;
    VideoBuffer m_buffers[kBufferCount];
    unsigned int m_bufferCount;
    QImage m_currentFrame;
    QElapsedTimer m_fpsTimer;
    QElapsedTimer m_previewEmitTimer;
    int m_totalFrameCount;
    int m_recentFrameCount;
    bool m_frameInFlight;
    int m_lastFaceCount;
    VisionPipeline *m_pipeline;
};

#endif
