#include "browserwindow.h"

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QUrl>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("CachySurf"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("io.cachyos.community"));
    QCoreApplication::setApplicationName(QStringLiteral("Cachy Surf"));
    QCoreApplication::setApplicationVersion(QStringLiteral(CACHYSURF_VERSION));
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QByteArray chromiumFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    if (!chromiumFlags.contains("--force-dark-mode"))
        chromiumFlags.append(" --force-dark-mode --enable-features=WebContentsForceDark");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", chromiumFlags.trimmed());

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Unofficial CachyOS-styled web browser"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption privateOption(
        {QStringLiteral("p"), QStringLiteral("private")},
        QStringLiteral("Open an off-the-record private window."));
    parser.addOption(privateOption);
    parser.addPositionalArgument(QStringLiteral("url"), QStringLiteral("URL to open."));
    parser.process(app);

    BrowserWindow window(parser.isSet(privateOption));
    window.show();

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        window.createTab(QUrl::fromUserInput(positional.first()), true);

    return app.exec();
}
