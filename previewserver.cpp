#include "previewserver.h"

#include <QAbstractSocket>
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QTcpSocket>

namespace {

const quint16 kPreviewPort = 8080;
const int kRecordIntervalMs = 200;
const int kStreamIntervalMs = 80;
const int kStreamMaxWidth = 480;
const int kStreamJpegQuality = 55;

QString jsonEscape(const QString &text)
{
    QString escaped = text;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    return escaped;
}

}

PreviewServer::PreviewServer(QObject *parent)
    : QObject(parent),
      m_frameVersion(0),
      m_recording(false),
      m_recordedFrameCount(0),
      m_lastRecordedMs(0),
      m_lastStreamEncodeMs(0)
{
    connect(&m_server, &QTcpServer::newConnection, this, &PreviewServer::onNewConnection);
    if (m_server.listen(QHostAddress::Any, kPreviewPort)) {
        emit infoMessage(QStringLiteral("Preview web server listening on port %1").arg(kPreviewPort));
    } else {
        emit infoMessage(QStringLiteral("Preview web server failed to start"));
    }
}

void PreviewServer::updateFrame(const QImage &frame)
{
    m_latestFrame = frame;

    maybeRecordFrame();

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_lastStreamEncodeMs != 0 && (nowMs - m_lastStreamEncodeMs) < kStreamIntervalMs) {
        return;
    }

    QImage streamFrame = m_latestFrame;
    if (streamFrame.width() > kStreamMaxWidth) {
        streamFrame = streamFrame.scaledToWidth(kStreamMaxWidth, Qt::FastTransformation);
    }

    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    streamFrame.save(&buffer, "JPG", kStreamJpegQuality);
    m_latestJpeg = jpeg;
    ++m_frameVersion;
    m_lastStreamEncodeMs = nowMs;

    sendFrameToStreamClients();
}

bool PreviewServer::hasFrame() const
{
    return !m_latestFrame.isNull() && !m_latestJpeg.isEmpty();
}

QByteArray PreviewServer::latestJpeg() const
{
    return m_latestJpeg;
}

quint64 PreviewServer::frameVersion() const
{
    return m_frameVersion;
}

bool PreviewServer::isRecording() const
{
    return m_recording;
}

int PreviewServer::recordedFrameCount() const
{
    return m_recordedFrameCount;
}

void PreviewServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            handleHttpRequest(socket, socket->readAll());
        });
    }
}

void PreviewServer::handleHttpRequest(QTcpSocket *socket, const QByteArray &requestData)
{
    const QList<QByteArray> lines = requestData.split('\n');
    if (lines.isEmpty()) {
        socket->disconnectFromHost();
        return;
    }

    const QByteArray requestLine = lines.first().trimmed();
    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2) {
        socket->disconnectFromHost();
        return;
    }

    const QByteArray method = parts.at(0);
    const QByteArray path = parts.at(1);

    if (method == "GET" && path == "/") {
        const QByteArray html = buildIndexPage();
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
                      QByteArray::number(html.size()) + "\r\nConnection: close\r\n\r\n");
        socket->write(html);
        socket->disconnectFromHost();
        return;
    }

    if (method == "GET" && path == "/frame.jpg") {
        if (!hasFrame()) {
            const QByteArray body = "No frame";
            socket->write("HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain\r\nContent-Length: " +
                          QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
            socket->write(body);
            socket->disconnectFromHost();
            return;
        }

        socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nCache-Control: no-store\r\nContent-Length: " +
                      QByteArray::number(m_latestJpeg.size()) + "\r\nConnection: close\r\n\r\n");
        socket->write(m_latestJpeg);
        socket->disconnectFromHost();
        return;
    }

    if (method == "GET" && path == "/stream.mjpg") {
        socket->write("HTTP/1.1 200 OK\r\n");
        socket->write("Cache-Control: no-store\r\n");
        socket->write("Pragma: no-cache\r\n");
        socket->write("Connection: close\r\n");
        socket->write("Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");

        StreamClient client;
        client.socket = socket;
        client.lastVersion = 0;
        m_streamClients.append(client);
        sendFrameToStreamClients();
        return;
    }

    if (method == "GET" && path == "/status") {
        const QByteArray body = buildStatusJson();
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nCache-Control: no-store\r\nContent-Length: " +
                      QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
        socket->write(body);
        socket->disconnectFromHost();
        return;
    }

    if (method == "POST" && path == "/snapshot") {
        QString savedPath;
        const bool ok = saveSnapshot(&savedPath);
        const QByteArray body = buildStatusJson(ok ? QStringLiteral("Snapshot saved: %1").arg(savedPath)
                                                   : QStringLiteral("Snapshot failed"));
        socket->write(ok ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 500 Internal Server Error\r\n");
        socket->write("Content-Type: application/json\r\nContent-Length: " +
                      QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
        socket->write(body);
        socket->disconnectFromHost();
        return;
    }

    if (method == "POST" && path == "/record/start") {
        QString message;
        const bool ok = startRecording(&message);
        const QByteArray body = buildStatusJson(message);
        socket->write(ok ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 409 Conflict\r\n");
        socket->write("Content-Type: application/json\r\nContent-Length: " +
                      QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
        socket->write(body);
        socket->disconnectFromHost();
        return;
    }

    if (method == "POST" && path == "/record/stop") {
        QString message;
        const bool ok = stopRecording(&message);
        const QByteArray body = buildStatusJson(message);
        socket->write(ok ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 409 Conflict\r\n");
        socket->write("Content-Type: application/json\r\nContent-Length: " +
                      QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
        socket->write(body);
        socket->disconnectFromHost();
        return;
    }

    const QByteArray body = "Not found";
    socket->write("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: " +
                  QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
    socket->write(body);
    socket->disconnectFromHost();
}

QByteArray PreviewServer::buildIndexPage() const
{
    return QByteArray(
                "<!doctype html><html><head><meta charset=\"utf-8\">"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                "<title>IMX6ULL Preview</title>"
                "<style>"
                "body{margin:0;font-family:Arial,sans-serif;background:#09111d;color:#e6eef7;}"
                ".wrap{max-width:980px;margin:0 auto;padding:20px;}"
                "h1{font-size:24px;margin:0 0 16px;}"
                ".panel{background:#101a2a;border:1px solid #25354d;border-radius:12px;padding:16px;}"
                "img{width:100%;background:#020617;border-radius:10px;display:block;min-height:240px;object-fit:contain;}"
                ".toolbar{display:flex;gap:10px;flex-wrap:wrap;margin:14px 0;}"
                "button{background:#1d4ed8;color:white;border:none;border-radius:8px;padding:10px 14px;font-size:14px;cursor:pointer;}"
                "button.stop{background:#b91c1c;}"
                ".meta{font-size:14px;color:#9fb3c8;line-height:1.8;}"
                "</style></head><body><div class=\"wrap\">"
                "<h1>IMX6ULL Remote Preview</h1>"
                "<div class=\"panel\">"
                "<img id=\"preview\" src=\"/stream.mjpg\" alt=\"preview\">"
                "<div class=\"toolbar\">"
                "<button onclick=\"postAction('/snapshot')\">Snapshot</button>"
                "<button onclick=\"postAction('/record/start')\">Start Record</button>"
                "<button class=\"stop\" onclick=\"postAction('/record/stop')\">Stop Record</button>"
                "</div>"
                "<div class=\"meta\" id=\"status\">Loading...</div>"
                "</div></div>"
                "<script>"
                "const status=document.getElementById('status');"
                "function refreshStatus(){fetch('/status').then(r=>r.json()).then(data=>{"
                "status.textContent='Status: '+data.message+' | Record: '+(data.recording?'ON':'OFF')+' | Frames: '+data.recordedFrames;"
                "}).catch(()=>{status.textContent='Status fetch failed';});}"
                "function postAction(path){fetch(path,{method:'POST'}).then(r=>r.json()).then(data=>{refreshStatus(); alert(data.message);}).catch(()=>alert('Request failed'));}"
                "setInterval(refreshStatus,1000);"
                "refreshStatus();"
                "</script></body></html>");
}

QByteArray PreviewServer::buildStatusJson(const QString &message) const
{
    const QString msg = message.isEmpty()
            ? (m_recording ? QStringLiteral("Recording") : QStringLiteral("Idle"))
            : message;

    const QString json = QStringLiteral(
                "{\"message\":\"%1\",\"recording\":%2,\"recordedFrames\":%3,\"hasFrame\":%4}")
            .arg(jsonEscape(msg))
            .arg(m_recording ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(m_recordedFrameCount)
            .arg(hasFrame() ? QStringLiteral("true") : QStringLiteral("false"));
    return json.toUtf8();
}

QByteArray PreviewServer::latestJpegPart() const
{
    QByteArray part;
    part += "--frame\r\n";
    part += "Content-Type: image/jpeg\r\n";
    part += "Content-Length: " + QByteArray::number(m_latestJpeg.size()) + "\r\n\r\n";
    part += m_latestJpeg;
    part += "\r\n";
    return part;
}

bool PreviewServer::saveSnapshot(QString *savedPath)
{
    if (m_latestFrame.isNull()) {
        return false;
    }

    QDir dir(QStringLiteral("/home/web_snapshots"));
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    const QString filePath = dir.filePath(QStringLiteral("web_capture_%1.jpg")
                                          .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")));
    if (!m_latestFrame.save(filePath, "JPG", 90)) {
        return false;
    }

    if (savedPath) {
        *savedPath = filePath;
    }
    emit infoMessage(QStringLiteral("Web snapshot saved: %1").arg(filePath));
    return true;
}

bool PreviewServer::startRecording(QString *message)
{
    if (m_recording) {
        if (message) {
            *message = QStringLiteral("Recording already in progress");
        }
        return false;
    }

    QDir root(QStringLiteral("/home/web_recordings"));
    if (!root.exists() && !root.mkpath(QStringLiteral("."))) {
        if (message) {
            *message = QStringLiteral("Failed to create recording root");
        }
        return false;
    }

    m_recordDirPath = root.filePath(QStringLiteral("record_%1")
                                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")));
    QDir dir(m_recordDirPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (message) {
            *message = QStringLiteral("Failed to create recording directory");
        }
        return false;
    }

    m_recordedFrameCount = 0;
    m_lastRecordedMs = 0;
    m_recording = true;
    if (message) {
        *message = QStringLiteral("Recording started: %1").arg(m_recordDirPath);
    }
    emit infoMessage(*message);
    return true;
}

bool PreviewServer::stopRecording(QString *message)
{
    if (!m_recording) {
        if (message) {
            *message = QStringLiteral("Recording is not active");
        }
        return false;
    }

    m_recording = false;
    if (message) {
        *message = QStringLiteral("Recording stopped: %1 (%2 frames)")
                .arg(m_recordDirPath)
                .arg(m_recordedFrameCount);
    }
    emit infoMessage(*message);
    return true;
}

void PreviewServer::maybeRecordFrame()
{
    if (!m_recording || m_latestFrame.isNull()) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_lastRecordedMs != 0 && (nowMs - m_lastRecordedMs) < kRecordIntervalMs) {
        return;
    }

    QDir dir(m_recordDirPath);
    const QString filePath = dir.filePath(QStringLiteral("frame_%1.jpg")
                                          .arg(m_recordedFrameCount, 5, 10, QLatin1Char('0')));
    if (m_latestFrame.save(filePath, "JPG", 85)) {
        ++m_recordedFrameCount;
        m_lastRecordedMs = nowMs;
    }
}

void PreviewServer::sendFrameToStreamClients()
{
    if (m_latestJpeg.isEmpty()) {
        return;
    }

    const QByteArray part = latestJpegPart();
    for (int i = m_streamClients.size() - 1; i >= 0; --i) {
        StreamClient &client = m_streamClients[i];
        if (!client.socket || client.socket->state() != QAbstractSocket::ConnectedState) {
            m_streamClients.removeAt(i);
            continue;
        }
        if (client.lastVersion == m_frameVersion) {
            continue;
        }
        client.socket->write(part);
        client.socket->flush();
        client.lastVersion = m_frameVersion;
    }
}
