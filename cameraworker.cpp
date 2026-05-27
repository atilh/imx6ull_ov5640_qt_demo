#include "cameraworker.h"
#include "mockvisionpipeline.h"
#include "realvisionpipeline.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QThread>

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

CameraWorker::CameraWorker(QObject *parent)
    : QObject(parent),
      m_videoFd(-1),
      m_running(false),
      m_width(0),
      m_height(0),
      m_bufferCount(0),
      m_totalFrameCount(0),
      m_recentFrameCount(0),
      m_frameInFlight(false),
      m_lastFaceCount(-1),
      m_pipeline(createPipeline())
{
    std::memset(m_buffers, 0, sizeof(m_buffers));
}

CameraWorker::~CameraWorker()
{
    stopPreview();
    delete m_pipeline;
    m_pipeline = nullptr;
}

void CameraWorker::startPreview(const QString &devicePath, int width, int height)
{
    if (m_running) {
        emit statusChanged(QStringLiteral("Preview is already running."));
        return;
    }

    if (!openDevice(devicePath, width, height)) {
        closeDevice();
        return;
    }

    if (!initMmap()) {
        closeDevice();
        return;
    }

    if (!startStreaming()) {
        closeDevice();
        return;
    }

    resetStats();
    m_running = true;
    emit statusChanged(QStringLiteral("Preview started: %1 %2x%3 RGB565 [%4]")
                       .arg(devicePath).arg(width).arg(height)
                       .arg(m_pipeline ? m_pipeline->pipelineName() : QStringLiteral("none")));

    while (m_running) {
        if (!readOneFrame()) {
            break;
        }
        publishStats();
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }

    stopStreaming();
    closeDevice();
    m_running = false;
    emit previewStopped();
    emit statusChanged(QStringLiteral("Preview stopped."));
}

void CameraWorker::stopPreview()
{
    m_running = false;
}

void CameraWorker::saveCurrentFrame(const QString &filePath)
{
    if (m_currentFrame.isNull()) {
        emit errorOccurred(QStringLiteral("No frame is available to save."));
        return;
    }

    if (!m_currentFrame.save(filePath, "JPG", 95)) {
        emit errorOccurred(QStringLiteral("Failed to save image: %1").arg(filePath));
        return;
    }

    emit statusChanged(QStringLiteral("Snapshot saved: %1").arg(filePath));
}

void CameraWorker::saveEnrollmentSample(const QString &dirPath)
{
    if (m_currentFrame.isNull()) {
        emit errorOccurred(QStringLiteral("No frame is available for enrollment."));
        return;
    }

    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        emit errorOccurred(QStringLiteral("Failed to create sample directory: %1").arg(dirPath));
        return;
    }

    const QString fileName = QStringLiteral("face_sample_%1.jpg")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    const QString filePath = dir.filePath(fileName);

    if (!m_currentFrame.save(filePath, "JPG", 95)) {
        emit errorOccurred(QStringLiteral("Failed to save enrollment sample: %1").arg(filePath));
        return;
    }

    emit statusChanged(QStringLiteral("Enrollment sample saved: %1").arg(filePath));
}

void CameraWorker::notifyFrameDisplayed()
{
    m_frameInFlight = false;
}

VisionPipeline *CameraWorker::createPipeline() const
{
    const QString configPath = QStringLiteral("/home/imx6ull_vision_mode.conf");
    QFile file(configPath);

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString mode = QString::fromUtf8(file.readAll()).trimmed().toLower();
        if (mode == QStringLiteral("real")) {
            return new RealVisionPipeline;
        }
    }

    return new MockVisionPipeline;
}

bool CameraWorker::openDevice(const QString &devicePath, int width, int height)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;

    m_videoFd = ::open(devicePath.toLocal8Bit().constData(), O_RDWR);
    if (m_videoFd < 0) {
        emit errorOccurred(QStringLiteral("Failed to open camera: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    if (xioctl(m_videoFd, VIDIOC_QUERYCAP, &cap) < 0) {
        emit errorOccurred(QStringLiteral("VIDIOC_QUERYCAP failed: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING)) {
        emit errorOccurred(QStringLiteral("Camera does not support V4L2 capture/streaming."));
        return false;
    }

    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = static_cast<quint32>(width);
    fmt.fmt.pix.height = static_cast<quint32>(height);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(m_videoFd, VIDIOC_S_FMT, &fmt) < 0) {
        emit errorOccurred(QStringLiteral("VIDIOC_S_FMT failed: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
        emit errorOccurred(QStringLiteral("Camera did not switch to RGB565 mode."));
        return false;
    }

    m_width = static_cast<int>(fmt.fmt.pix.width);
    m_height = static_cast<int>(fmt.fmt.pix.height);
    return true;
}

bool CameraWorker::initMmap()
{
    struct v4l2_requestbuffers req;

    std::memset(&req, 0, sizeof(req));
    req.count = kBufferCount;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(m_videoFd, VIDIOC_REQBUFS, &req) < 0) {
        emit errorOccurred(QStringLiteral("VIDIOC_REQBUFS failed: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    if (req.count < 2) {
        emit errorOccurred(QStringLiteral("Not enough V4L2 buffers available."));
        return false;
    }

    m_bufferCount = req.count > kBufferCount ? kBufferCount : req.count;

    for (unsigned int i = 0; i < m_bufferCount; ++i) {
        struct v4l2_buffer buf;

        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(m_videoFd, VIDIOC_QUERYBUF, &buf) < 0) {
            emit errorOccurred(QStringLiteral("VIDIOC_QUERYBUF failed: %1")
                               .arg(QString::fromLocal8Bit(std::strerror(errno))));
            return false;
        }

        m_buffers[i].length = buf.length;
        m_buffers[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, m_videoFd, buf.m.offset);
        if (m_buffers[i].start == MAP_FAILED) {
            m_buffers[i].start = nullptr;
            emit errorOccurred(QStringLiteral("mmap failed: %1")
                               .arg(QString::fromLocal8Bit(std::strerror(errno))));
            return false;
        }
    }

    return true;
}

bool CameraWorker::startStreaming()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (unsigned int i = 0; i < m_bufferCount; ++i) {
        struct v4l2_buffer buf;

        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(m_videoFd, VIDIOC_QBUF, &buf) < 0) {
            emit errorOccurred(QStringLiteral("VIDIOC_QBUF failed: %1")
                               .arg(QString::fromLocal8Bit(std::strerror(errno))));
            return false;
        }
    }

    if (xioctl(m_videoFd, VIDIOC_STREAMON, &type) < 0) {
        emit errorOccurred(QStringLiteral("VIDIOC_STREAMON failed: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    return true;
}

void CameraWorker::stopStreaming()
{
    if (m_videoFd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(m_videoFd, VIDIOC_STREAMOFF, &type);
    }
}

void CameraWorker::closeDevice()
{
    for (unsigned int i = 0; i < m_bufferCount; ++i) {
        if (m_buffers[i].start) {
            munmap(m_buffers[i].start, m_buffers[i].length);
            m_buffers[i].start = nullptr;
            m_buffers[i].length = 0;
        }
    }
    m_bufferCount = 0;

    if (m_videoFd >= 0) {
        ::close(m_videoFd);
        m_videoFd = -1;
    }
}

bool CameraWorker::readOneFrame()
{
    struct v4l2_buffer buf;

    std::memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(m_videoFd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            return true;
        }
        emit errorOccurred(QStringLiteral("VIDIOC_DQBUF failed: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    QImage image(static_cast<const uchar *>(m_buffers[buf.index].start),
                 m_width, m_height, QImage::Format_RGB16);

    // Copy only once from the mmap buffer so the frame stays valid after requeue.
    m_currentFrame = image.copy();
    m_currentFrame = runVisionPipeline(m_currentFrame);
    ++m_totalFrameCount;
    ++m_recentFrameCount;

    if (!m_frameInFlight &&
        (!m_previewEmitTimer.isValid() || m_previewEmitTimer.elapsed() >= 33)) {
        m_frameInFlight = true;
        emit frameReady(m_currentFrame);
        m_previewEmitTimer.restart();
    }

    if (xioctl(m_videoFd, VIDIOC_QBUF, &buf) < 0) {
        emit errorOccurred(QStringLiteral("VIDIOC_QBUF requeue failed: %1")
                           .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    return true;
}

QImage CameraWorker::runVisionPipeline(const QImage &frame)
{
    if (!m_pipeline) {
        return frame;
    }

    const VisionResult result = m_pipeline->process(frame, m_totalFrameCount);
    if (result.faceCount != m_lastFaceCount) {
        m_lastFaceCount = result.faceCount;
        emit detectionStateChanged(result.faceCount, result.message);
    }

    return result.frame;
}

void CameraWorker::resetStats()
{
    m_totalFrameCount = 0;
    m_recentFrameCount = 0;
    m_frameInFlight = false;
    m_lastFaceCount = -1;
    m_fpsTimer.restart();
    m_previewEmitTimer.invalidate();
    emit frameStatsChanged(0, 0);
}

void CameraWorker::publishStats()
{
    const qint64 elapsedMs = m_fpsTimer.elapsed();
    if (elapsedMs < 500) {
        return;
    }

    const int fpsTimes10 = static_cast<int>((m_recentFrameCount * 10000LL) / elapsedMs);
    emit frameStatsChanged(m_totalFrameCount, fpsTimes10);
    m_recentFrameCount = 0;
    m_fpsTimer.restart();
}

int CameraWorker::xioctl(int fd, int request, void *arg)
{
    int ret;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}
