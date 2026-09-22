#pragma once

#include <QDialog>
#include <QtGlobal>

class QCheckBox;
class QCloseEvent;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;

namespace elkbledom {

class LogWindow final : public QDialog
{
    Q_OBJECT

public:
    explicit LogWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void onPause(bool checked);
    void onClear();
    void onSave();
    void updateFileLabel();
    void refresh(bool preserveScroll = true);

    QLabel* info_ = nullptr;
    QCheckBox* autoscroll_ = nullptr;
    QPushButton* pauseButton_ = nullptr;
    QPlainTextEdit* text_ = nullptr;
    QLabel* fileLabel_ = nullptr;
    QTimer* timer_ = nullptr;
    quint64 lastSequence_ = 0;
    bool paused_ = false;
};

} // namespace elkbledom
