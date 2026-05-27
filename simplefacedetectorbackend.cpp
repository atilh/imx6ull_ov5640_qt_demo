#include "simplefacedetectorbackend.h"

#include <QQueue>

namespace {

static inline bool isSkinLike(int r, int g, int b)
{
    const int maxc = qMax(r, qMax(g, b));
    const int minc = qMin(r, qMin(g, b));

    return r > 90 &&
           g > 45 &&
           b > 20 &&
           (maxc - minc) > 15 &&
           qAbs(r - g) > 15 &&
           r > g &&
           r > b;
}

}

QVector<FaceDetection> SimpleFaceDetectorBackend::detect(const QImage &frame) const
{
    QVector<FaceDetection> detections;
    if (frame.isNull()) {
        return detections;
    }

    const QImage rgbFrame = frame.convertToFormat(QImage::Format_RGB888);
    const int srcW = rgbFrame.width();
    const int srcH = rgbFrame.height();
    if (srcW < 32 || srcH < 32) {
        return detections;
    }

    const int step = 4;
    const int w = srcW / step;
    const int h = srcH / step;
    if (w <= 0 || h <= 0) {
        return detections;
    }

    QVector<uchar> mask(w * h, 0);
    QVector<uchar> visited(w * h, 0);

    for (int y = 0; y < h; ++y) {
        const uchar *line = rgbFrame.constScanLine(y * step);
        for (int x = 0; x < w; ++x) {
            const int px = x * step * 3;
            const int r = line[px];
            const int g = line[px + 1];
            const int b = line[px + 2];
            if (isSkinLike(r, g, b)) {
                mask[y * w + x] = 1;
            }
        }
    }

    QQueue<int> queue;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int start = y * w + x;
            if (!mask[start] || visited[start]) {
                continue;
            }

            int minX = x;
            int maxX = x;
            int minY = y;
            int maxY = y;
            int area = 0;

            visited[start] = 1;
            queue.enqueue(start);

            while (!queue.isEmpty()) {
                const int idx = queue.dequeue();
                const int cx = idx % w;
                const int cy = idx / w;
                ++area;

                minX = qMin(minX, cx);
                maxX = qMax(maxX, cx);
                minY = qMin(minY, cy);
                maxY = qMax(maxY, cy);

                const int nx[4] = {cx - 1, cx + 1, cx, cx};
                const int ny[4] = {cy, cy, cy - 1, cy + 1};
                for (int k = 0; k < 4; ++k) {
                    if (nx[k] < 0 || nx[k] >= w || ny[k] < 0 || ny[k] >= h) {
                        continue;
                    }
                    const int ni = ny[k] * w + nx[k];
                    if (!mask[ni] || visited[ni]) {
                        continue;
                    }
                    visited[ni] = 1;
                    queue.enqueue(ni);
                }
            }

            const int boxW = maxX - minX + 1;
            const int boxH = maxY - minY + 1;
            if (area < 80 || boxW < 12 || boxH < 14) {
                continue;
            }

            const float ratio = static_cast<float>(boxH) / static_cast<float>(boxW);
            if (ratio < 0.85f || ratio > 1.8f) {
                continue;
            }

            const QRect box(minX * step,
                            minY * step,
                            boxW * step,
                            boxH * step);

            FaceDetection det;
            det.box = box;
            det.label = QStringLiteral("FaceCandidate");
            det.confidence = qMin(0.95f, 0.45f + static_cast<float>(area) / 500.0f);
            det.recognized = false;
            detections.append(det);

            if (detections.size() >= 3) {
                return detections;
            }
        }
    }

    return detections;
}

QString SimpleFaceDetectorBackend::backendName() const
{
    return QStringLiteral("simple-skin-detector");
}
