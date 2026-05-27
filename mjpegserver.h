#ifndef MJPEGSERVER_H
#define MJPEGSERVER_H

#include <QHash>
#include <QObject>

class QImage;
class QTcpServer;
class QTcpSocket;

class MjpegServer : public QObject
{
    Q_OBJECT

public:
    explicit MjpegServer(QObject *parent = nullptr);
    ~MjpegServer();

    bool start(quint16 port = 8080);
    void publishFrame(const QImage &frame);
    quint16 port() const;

signals:
    void statusMessage(const QString &message);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();

private:
    struct ClientState {
        bool streaming;
        bool requestHandled;
    };

    void removeClient(QTcpSocket *socket);
    void sendIndexPage(QTcpSocket *socket);
    void sendStreamHeader(QTcpSocket *socket);
    void sendSnapshot(QTcpSocket *socket);
    void broadcastJpegFrame(const QByteArray &jpegBytes);

    QTcpServer *m_server;
    QHash<QTcpSocket *, ClientState> m_clients;
    QByteArray m_lastJpeg;
    quint16 m_port;
};

#endif
