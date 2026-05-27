#ifndef PREVIEWSERVER_H
#define PREVIEWSERVER_H

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>

class PreviewServer : public QObject
{
    Q_OBJECT

public:
    explicit PreviewServer(QObject *parent = nullptr);

    void updateFrame(const QImage &frame);
    bool hasFrame() const;
    QByteArray latestJpeg() const;
    quint64 frameVersion() const;
    bool isRecording() const;
    int recordedFrameCount() const;

signals:
    void infoMessage(const QString &message);

private slots:
    void onNewConnection();

private:
    struct StreamClient {
        QPointer<QTcpSocket> socket;
        quint64 lastVersion;
    };

    void handleHttpRequest(QTcpSocket *socket, const QByteArray &requestData);
    QByteArray buildIndexPage() const;
    QByteArray buildStatusJson(const QString &message = QString()) const;
    QByteArray latestJpegPart() const;
    bool saveSnapshot(QString *savedPath);
    bool startRecording(QString *message);
    bool stopRecording(QString *message);
    void maybeRecordFrame();
    void sendFrameToStreamClients();

    QTcpServer m_server;
    QImage m_latestFrame;
    QByteArray m_latestJpeg;
    quint64 m_frameVersion;
    bool m_recording;
    QString m_recordDirPath;
    int m_recordedFrameCount;
    qint64 m_lastRecordedMs;
    qint64 m_lastStreamEncodeMs;
    QList<StreamClient> m_streamClients;
};

#endif
