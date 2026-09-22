#include "LogWindow.h"

#include "AppLog.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace elkbledom {

LogWindow::LogWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Application log"));
    setMinimumSize(820, 560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto* top = new QHBoxLayout;
    info_ = new QLabel(tr("Log is empty"), this);
    info_->setProperty("role", QStringLiteral("status"));
    top->addWidget(info_);
    top->addStretch(1);

    autoscroll_ = new QCheckBox(tr("Auto-scroll"), this);
    autoscroll_->setChecked(true);
    top->addWidget(autoscroll_);

    pauseButton_ = new QPushButton(tr("⏸  Pause"), this);
    pauseButton_->setCheckable(true);
    connect(pauseButton_, &QPushButton::toggled, this, &LogWindow::onPause);
    top->addWidget(pauseButton_);

    auto* clearButton = new QPushButton(tr("Clear"), this);
    connect(clearButton, &QPushButton::clicked, this, &LogWindow::onClear);
    top->addWidget(clearButton);

    auto* saveButton = new QPushButton(tr("💾  Save to file"), this);
    connect(saveButton, &QPushButton::clicked, this, &LogWindow::onSave);
    top->addWidget(saveButton);

    auto* closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    top->addWidget(closeButton);
    root->addLayout(top);

    text_ = new QPlainTextEdit(this);
    text_->setReadOnly(true);
    text_->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont font(QStringLiteral("Consolas"));
    font.setPointSize(9);
    text_->setFont(font);
    root->addWidget(text_, 1);

    fileLabel_ = new QLabel(this);
    fileLabel_->setProperty("role", QStringLiteral("status"));
    root->addWidget(fileLabel_);

    refresh(false);

    timer_ = new QTimer(this);
    timer_->setInterval(400);
    connect(timer_, &QTimer::timeout, this, [this] { refresh(); });
    timer_->start();
    updateFileLabel();
}

void LogWindow::onPause(const bool checked)
{
    paused_ = checked;
    if (!paused_) {
        refresh(false);
    }
}

void LogWindow::onClear()
{
    AppLog::clear();
    const AppLog::Snapshot current = AppLog::snapshot();
    lastSequence_ = current.sequence;
    text_->clear();
    info_->setText(tr("Log is empty"));
}

void LogWindow::onSave()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save log"),
        QStringLiteral("elk_bledom.log"),
        tr("Log files (*.log *.txt);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this,
                             tr("Error"),
                             tr("Failed to save: %1").arg(file.errorString()));
        return;
    }

    const QByteArray contents = AppLog::buffer().join(QLatin1Char('\n')).toUtf8();
    const qint64 written = file.write(contents);
    if (written != contents.size() || !file.flush()) {
        const QString error = file.errorString();
        file.close();
        QMessageBox::warning(this,
                             tr("Error"),
                             tr("Failed to save: %1").arg(error));
        return;
    }
    file.close();

    QMessageBox::information(this,
                             tr("Saved"),
                             tr("Log saved:\n%1").arg(path));
}

void LogWindow::updateFileLabel()
{
    const QString path = AppLog::filePath();
    fileLabel_->setText(path.isEmpty()
                            ? tr("File: (unavailable)")
                            : tr("File: %1").arg(path));
}

void LogWindow::refresh(const bool preserveScroll)
{
    if (paused_) {
        return;
    }

    const AppLog::Snapshot current = AppLog::snapshot();
    info_->setText(tr("Lines: %1").arg(current.lines.size()));

    // Size remains 2000 after the ring fills; sequence still changes on every
    // append (and clear), so the window continues to update correctly.
    if (current.sequence == lastSequence_) {
        return;
    }
    lastSequence_ = current.sequence;

    QScrollBar* scrollBar = text_->verticalScrollBar();
    const bool atBottom = !preserveScroll
        || scrollBar->value() >= scrollBar->maximum() - 2;
    const int oldValue = scrollBar->value();

    text_->setPlainText(current.lines.join(QLatin1Char('\n')));

    if (autoscroll_->isChecked() && atBottom) {
        text_->moveCursor(QTextCursor::End);
    } else if (preserveScroll) {
        scrollBar->setValue(qMin(oldValue, scrollBar->maximum()));
    }
}

void LogWindow::closeEvent(QCloseEvent* event)
{
    QDialog::closeEvent(event);
}

} // namespace elkbledom
