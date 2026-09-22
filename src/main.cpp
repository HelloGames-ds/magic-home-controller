#include "MainWindow.h"
#include "SettingsStore.h"
#include "config.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QTranslator>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif

namespace {

#ifdef Q_OS_WIN
void setWindowsApplicationId()
{
    SetCurrentProcessExplicitAppUserModelID(L"magichome.controller.v4");
}
#endif

} // namespace

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    setWindowsApplicationId();
#endif

    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        QString::fromLatin1(elkbledom::config::ApplicationName));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));
    QCoreApplication::setOrganizationName(
        QString::fromLatin1(elkbledom::config::OrganizationName));

    application.setQuitOnLastWindowClosed(false);
    application.setStyleSheet(elkbledom::config::StyleSheet);

    QTranslator appTranslator;
    QTranslator qtTranslator;
    {
        elkbledom::SettingsStore store;
        const QString language = store.load().language;
        if (language == QStringLiteral("ru")) {
            QString appRu = QCoreApplication::applicationDirPath()
                                + QStringLiteral("/translations/magic_home_controller_ru.qm");
            if (appTranslator.load(appRu) || appTranslator.load(QStringLiteral(":/i18n/magic_home_controller_ru.qm"))) {
                application.installTranslator(&appTranslator);
            }
            QString qtBaseRu = QCoreApplication::applicationDirPath()
                                   + QStringLiteral("/translations/qtbase_ru.qm");
            if (!qtTranslator.load(qtBaseRu)) {
                qtBaseRu = QLibraryInfo::path(QLibraryInfo::TranslationsPath)
                               + QStringLiteral("/qtbase_ru.qm");
                qtTranslator.load(qtBaseRu);
            }
            if (!qtTranslator.isEmpty()) {
                application.installTranslator(&qtTranslator);
            }
        }
    }

    elkbledom::MainWindow window;
    const bool startMinimized = application.arguments().contains(QStringLiteral("--minimized"));
    if (!startMinimized) {
        window.show();
    }

    return application.exec();
}
