#include "mainwindow.h"

#include <QDateTime>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

const char *kWindowStyle =
        "QMainWindow {"
        "  background-color: #0b1220;"
        "}"
        "QWidget {"
        "  color: #d7e3f4;"
        "  font-size: 12px;"
        "}"
        "QFrame#panel {"
        "  background-color: #131d2d;"
        "  border: 1px solid #24334d;"
        "  border-radius: 8px;"
        "}"
        "QFrame#previewPanel {"
        "  background-color: #101827;"
        "  border: 1px solid #2c3f5d;"
        "  border-radius: 8px;"
        "}"
        "QLabel#titleLabel {"
        "  font-size: 18px;"
        "  font-weight: 700;"
        "  color: #f7fbff;"
        "}"
        "QLabel#clockLabel {"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  color: #9fb3c8;"
        "}"
        "QLabel#bannerLabel {"
        "  color: #7f92ab;"
        "  font-size: 10px;"
        "}"
        "QLabel#previewLabel {"
        "  background-color: #020617;"
        "  border-radius: 6px;"
        "  color: #74839c;"
        "  font-size: 13px;"
        "}"
        "QLabel#sectionLabel {"
        "  font-size: 10px;"
        "  font-weight: 700;"
        "  color: #6f849f;"
        "}"
        "QLabel#valueLabel {"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #f4f8fc;"
        "}"
        "QPushButton {"
        "  min-height: 30px;"
        "  padding: 0 10px;"
        "  border-radius: 6px;"
        "  background-color: #1d4ed8;"
        "  color: white;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  border: none;"
        "}"
        "QPushButton:hover {"
        "  background-color: #2563eb;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #1e40af;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #314158;"
        "  color: #8fa0b8;"
        "}"
        "QPushButton#stopButton {"
        "  background-color: #b91c1c;"
        "}"
        "QPushButton#stopButton:hover {"
        "  background-color: #dc2626;"
        "}"
        "QPushButton#saveButton {"
        "  background-color: #0f766e;"
        "}"
        "QPushButton#saveButton:hover {"
        "  background-color: #0d9488;"
        "}"
        "QPushButton#enrollButton {"
        "  background-color: #6d28d9;"
        "}"
        "QPushButton#enrollButton:hover {"
        "  background-color: #7c3aed;"
        "}"
        "QListWidget {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "  color: #d2deed;"
        "}"
        "QListWidget::item {"
        "  padding: 6px 4px;"
        "  border-bottom: 1px solid #223149;"
        "}";

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_titleLabel(nullptr),
      m_clockLabel(nullptr),
      m_previewLabel(nullptr),
      m_bannerLabel(nullptr),
      m_statusValueLabel(nullptr),
      m_deviceValueLabel(nullptr),
      m_resolutionValueLabel(nullptr),
      m_frameValueLabel(nullptr),
      m_fpsValueLabel(nullptr),
      m_faceValueLabel(nullptr),
      m_modeValueLabel(nullptr),
      m_eventList(nullptr),
      m_startButton(nullptr),
      m_stopButton(nullptr),
      m_saveButton(nullptr),
      m_enrollButton(nullptr),
      m_clockTimer(new QTimer(this)),
      m_cameraWorker(new CameraWorker),
      m_previewServer(new PreviewServer(this)),
      m_previewRunning(false)
{
    buildUi();
    setStyleSheet(QString::fromLatin1(kWindowStyle));
    setWindowTitle(QStringLiteral("IMX6ULL Smart Monitor"));
    resize(800, 480);

    m_cameraWorker->moveToThread(&m_cameraThread);

    connect(&m_cameraThread, &QThread::finished, m_cameraWorker, &QObject::deleteLater);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    connect(m_enrollButton, &QPushButton::clicked, this, &MainWindow::onEnrollClicked);
    connect(this, &MainWindow::destroyed, m_cameraWorker, &CameraWorker::stopPreview);

    connect(m_cameraWorker, &CameraWorker::frameReady, this, &MainWindow::updateFrame);
    connect(m_cameraWorker, &CameraWorker::statusChanged, this, &MainWindow::updateStatus);
    connect(m_cameraWorker, &CameraWorker::frameStatsChanged, this, &MainWindow::updateFrameStats);
    connect(m_cameraWorker, &CameraWorker::detectionStateChanged, this, &MainWindow::updateDetectionState);
    connect(m_cameraWorker, &CameraWorker::errorOccurred, this, &MainWindow::showError);
    connect(m_cameraWorker, &CameraWorker::previewStopped, this, &MainWindow::onPreviewStopped);
    connect(m_previewServer, &PreviewServer::infoMessage, this, &MainWindow::updateStatus);

    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    m_clockTimer->start(1000);
    updateClock();

    m_cameraThread.start();
    appendEvent(QStringLiteral("System ready. Mock face pipeline armed."));
}

MainWindow::~MainWindow()
{
    m_cameraWorker->stopPreview();
    m_cameraThread.quit();
    m_cameraThread.wait();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    refreshPreview();
}

void MainWindow::onStartClicked()
{
    if (m_previewRunning) {
        return;
    }

    setPreviewState(true);
    m_deviceValueLabel->setText(QStringLiteral("/dev/video1"));
    m_resolutionValueLabel->setText(QStringLiteral("800 x 480"));
    m_modeValueLabel->setText(QStringLiteral("Live preview"));
    appendEvent(QStringLiteral("Preview start requested."));

    QMetaObject::invokeMethod(m_cameraWorker, "startPreview", Qt::QueuedConnection,
                              Q_ARG(QString, QStringLiteral("/dev/video1")),
                              Q_ARG(int, 800),
                              Q_ARG(int, 480));
}

void MainWindow::onStopClicked()
{
    appendEvent(QStringLiteral("Preview stop requested."));
    m_cameraWorker->stopPreview();
}

void MainWindow::onSaveClicked()
{
    if (m_lastFrame.isNull()) {
        QMessageBox::warning(this,
                             QStringLiteral("No frame"),
                             QStringLiteral("No frame is available yet."));
        return;
    }

    const QString filePath = buildCapturePath();
    appendEvent(QStringLiteral("Snapshot queued: %1").arg(filePath));

    QMetaObject::invokeMethod(m_cameraWorker, "saveCurrentFrame", Qt::QueuedConnection,
                              Q_ARG(QString, filePath));
}

void MainWindow::onEnrollClicked()
{
    if (m_lastFrame.isNull()) {
        QMessageBox::warning(this,
                             QStringLiteral("No frame"),
                             QStringLiteral("No frame is available for enrollment."));
        return;
    }

    const QString dirPath = QStringLiteral("/home/face_samples");
    appendEvent(QStringLiteral("Enrollment sample queued: %1").arg(dirPath));

    QMetaObject::invokeMethod(m_cameraWorker, "saveEnrollmentSample", Qt::QueuedConnection,
                              Q_ARG(QString, dirPath));
}

void MainWindow::updateFrame(const QImage &image)
{
    m_lastFrame = image;
    refreshPreview();
    m_previewServer->updateFrame(image);
    QMetaObject::invokeMethod(m_cameraWorker, "notifyFrameDisplayed", Qt::QueuedConnection);

    if (m_faceValueLabel->text() == QStringLiteral("Offline")) {
        m_faceValueLabel->setText(QStringLiteral("0 faces"));
    }
}

void MainWindow::updateStatus(const QString &message)
{
    m_statusValueLabel->setText(message);
    appendEvent(message);
}

void MainWindow::updateFrameStats(int frameCount, int fpsTimes10)
{
    m_frameValueLabel->setText(QString::number(frameCount));
    m_fpsValueLabel->setText(QStringLiteral("%1.%2 fps")
                             .arg(fpsTimes10 / 10)
                             .arg(fpsTimes10 % 10));
}

void MainWindow::updateDetectionState(int faceCount, const QString &message)
{
    if (faceCount <= 0) {
        m_faceValueLabel->setText(QStringLiteral("0 faces"));
        appendEvent(QStringLiteral("Detection update: %1").arg(message));
        return;
    }

    m_faceValueLabel->setText(QStringLiteral("%1 faces").arg(faceCount));
    appendEvent(QStringLiteral("Detection update: %1").arg(message));
}

void MainWindow::showError(const QString &message)
{
    m_statusValueLabel->setText(message);
    appendEvent(QStringLiteral("Error: %1").arg(message));
    QMessageBox::critical(this, QStringLiteral("Camera Error"), message);
    onPreviewStopped();
}

void MainWindow::onPreviewStopped()
{
    setPreviewState(false);
    m_modeValueLabel->setText(QStringLiteral("Idle"));
    if (m_lastFrame.isNull()) {
        m_faceValueLabel->setText(QStringLiteral("Offline"));
    }
}

void MainWindow::updateClock()
{
    m_clockLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
}

void MainWindow::buildUi()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *rootLayout = new QVBoxLayout(central);
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QHBoxLayout *contentLayout = new QHBoxLayout;

    m_titleLabel = new QLabel(QStringLiteral("Smart Monitor"), this);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));

    m_clockLabel = new QLabel(this);
    m_clockLabel->setObjectName(QStringLiteral("clockLabel"));
    m_clockLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addWidget(m_clockLabel, 0);

    QFrame *previewPanel = new QFrame(this);
    previewPanel->setObjectName(QStringLiteral("previewPanel"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewPanel);
    QHBoxLayout *buttonLayout = new QHBoxLayout;

    m_bannerLabel = new QLabel(QStringLiteral("CH01  |  LIVE VIEW  |  FACE READY"), this);
    m_bannerLabel->setObjectName(QStringLiteral("bannerLabel"));

    m_previewLabel = new QLabel(QStringLiteral("Press START to open the camera stream."), this);
    m_previewLabel->setObjectName(QStringLiteral("previewLabel"));
    m_previewLabel->setMinimumSize(420, 220);
    m_previewLabel->setAlignment(Qt::AlignCenter);

    m_startButton = new QPushButton(QStringLiteral("Start"), this);
    m_stopButton = new QPushButton(QStringLiteral("Stop"), this);
    m_saveButton = new QPushButton(QStringLiteral("Snapshot"), this);
    m_enrollButton = new QPushButton(QStringLiteral("Enroll Face"), this);

    m_stopButton->setObjectName(QStringLiteral("stopButton"));
    m_saveButton->setObjectName(QStringLiteral("saveButton"));
    m_enrollButton->setObjectName(QStringLiteral("enrollButton"));

    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_enrollButton);
    buttonLayout->addStretch(1);

    previewLayout->addWidget(m_bannerLabel);
    previewLayout->addWidget(m_previewLabel, 1);
    previewLayout->addLayout(buttonLayout);

    QFrame *sidePanel = new QFrame(this);
    sidePanel->setObjectName(QStringLiteral("panel"));
    sidePanel->setMinimumWidth(220);
    sidePanel->setMaximumWidth(250);
    QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);

    QLabel *statusSectionLabel = new QLabel(QStringLiteral("SYSTEM STATUS"), this);
    statusSectionLabel->setObjectName(QStringLiteral("sectionLabel"));
    sideLayout->addWidget(statusSectionLabel);

    sideLayout->addWidget(new QLabel(QStringLiteral("Status"), this));
    m_statusValueLabel = createValueLabel(QStringLiteral("Idle"));
    sideLayout->addWidget(m_statusValueLabel);

    sideLayout->addWidget(new QLabel(QStringLiteral("Device"), this));
    m_deviceValueLabel = createValueLabel(QStringLiteral("/dev/video1"));
    sideLayout->addWidget(m_deviceValueLabel);

    sideLayout->addWidget(new QLabel(QStringLiteral("Resolution"), this));
    m_resolutionValueLabel = createValueLabel(QStringLiteral("800 x 480"));
    sideLayout->addWidget(m_resolutionValueLabel);

    sideLayout->addWidget(new QLabel(QStringLiteral("Frames"), this));
    m_frameValueLabel = createValueLabel(QStringLiteral("0"));
    sideLayout->addWidget(m_frameValueLabel);

    sideLayout->addWidget(new QLabel(QStringLiteral("Frame Rate"), this));
    m_fpsValueLabel = createValueLabel(QStringLiteral("0.0 fps"));
    sideLayout->addWidget(m_fpsValueLabel);

    sideLayout->addWidget(new QLabel(QStringLiteral("Faces"), this));
    m_faceValueLabel = createValueLabel(QStringLiteral("Offline"));
    sideLayout->addWidget(m_faceValueLabel);

    sideLayout->addWidget(new QLabel(QStringLiteral("Mode"), this));
    m_modeValueLabel = createValueLabel(QStringLiteral("Idle"));
    sideLayout->addWidget(m_modeValueLabel);

    QLabel *eventSectionLabel = new QLabel(QStringLiteral("EVENT LOG"), this);
    eventSectionLabel->setObjectName(QStringLiteral("sectionLabel"));
    sideLayout->addSpacing(4);
    sideLayout->addWidget(eventSectionLabel);

    m_eventList = new QListWidget(this);
    sideLayout->addWidget(m_eventList, 1);

    contentLayout->addWidget(previewPanel, 5);
    contentLayout->addWidget(sidePanel, 1);

    rootLayout->addLayout(headerLayout);
    rootLayout->addLayout(contentLayout, 1);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    previewLayout->setContentsMargins(8, 8, 8, 8);
    previewLayout->setSpacing(6);
    sideLayout->setContentsMargins(8, 8, 8, 8);
    sideLayout->setSpacing(3);
    headerLayout->setSpacing(6);
    contentLayout->setSpacing(6);

    setCentralWidget(central);
    setPreviewState(false);
}

QLabel *MainWindow::createValueLabel(const QString &text)
{
    QLabel *label = new QLabel(text, this);
    label->setObjectName(QStringLiteral("valueLabel"));
    label->setWordWrap(true);
    return label;
}

QString MainWindow::buildCapturePath() const
{
    const QString dirPath = QStringLiteral("/home");
    const QString fileName = QStringLiteral("monitor_capture_%1.jpg")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    return QDir(dirPath).filePath(fileName);
}

void MainWindow::appendEvent(const QString &message)
{
    const QString line = QStringLiteral("[%1] %2")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
            .arg(message);
    m_eventList->insertItem(0, line);

    while (m_eventList->count() > 12) {
        delete m_eventList->takeItem(m_eventList->count() - 1);
    }
}

void MainWindow::refreshPreview()
{
    if (m_lastFrame.isNull()) {
        m_previewLabel->setPixmap(QPixmap());
        if (!m_previewRunning) {
            m_previewLabel->setText(QStringLiteral("Press START to open the camera stream."));
        }
        return;
    }

    const QPixmap pixmap = QPixmap::fromImage(m_lastFrame).scaled(
                m_previewLabel->size(),
                Qt::KeepAspectRatio,
                Qt::FastTransformation);
    m_previewLabel->setPixmap(pixmap);
}

void MainWindow::setPreviewState(bool running)
{
    m_previewRunning = running;
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_saveButton->setEnabled(running || !m_lastFrame.isNull());
    m_enrollButton->setEnabled(true);
}
