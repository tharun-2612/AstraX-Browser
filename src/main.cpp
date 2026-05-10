#include "ui/BrowserWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QIcon>

#ifndef ASTRA_VERSION
#define ASTRA_VERSION "dev"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/branding/AstraX.ico")));

    QCoreApplication::setApplicationName(QStringLiteral("AstraX"));
    QCoreApplication::setApplicationVersion(QStringLiteral(ASTRA_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("AstraX Labs"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("astrax.local"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("AstraX - a C++ Qt WebEngine browser"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption privateModeOption(
        QStringList{QStringLiteral("p"), QStringLiteral("private")},
        QStringLiteral("Start without persistent cookies, cache, or history writes."));
    parser.addOption(privateModeOption);
    parser.process(app);

    astra::BrowserWindow window(parser.isSet(privateModeOption));
    window.resize(1280, 820);
    window.show();

    return QApplication::exec();
}
