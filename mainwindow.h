#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "cameraworker.h"
#include "previewserver.h"

#include <QImage>
#include <QMainWindow>
#include <QThread>

class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onSaveClicked();
    void onEnrollClicked();
    void updateFrame(const QImage &image);
    void updateStatus(const QString &message);
    void updateFrameStats(int frameCount, int fpsTimes10);
    void updateDetectionState(int faceCount, const QString &message);
    void showError(const QString &message);
    void onPreviewStopped();
    void updateClock();

private:
    void buildUi();
    QLabel *createValueLabel(const QString &text = QString());
    QString buildCapturePath() const;
    void appendEvent(const QString &message);
    void refreshPreview();
    void setPreviewState(bool running);

    QLabel *m_titleLabel;
    QLabel *m_clockLabel;
    QLabel *m_previewLabel;
    QLabel *m_bannerLabel;
    QLabel *m_statusValueLabel;
    QLabel *m_deviceValueLabel;
    QLabel *m_resolutionValueLabel;
    QLabel *m_frameValueLabel;
    QLabel *m_fpsValueLabel;
    QLabel *m_faceValueLabel;
    QLabel *m_modeValueLabel;
    QListWidget *m_eventList;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QPushButton *m_saveButton;
    QPushButton *m_enrollButton;
    QTimer *m_clockTimer;

    QThread m_cameraThread;
    CameraWorker *m_cameraWorker;
    PreviewServer *m_previewServer;
    QImage m_lastFrame;
    bool m_previewRunning;
};

#endif
