#include <catch2/catch_session.hpp>

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

int main(int argc, char* argv[])
{
    QTemporaryDir settingsDirectory;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(
        QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    QCoreApplication application{argc, argv};
    QCoreApplication::setOrganizationName("YaapTests");
    QCoreApplication::setApplicationName("YaapTests");
    return Catch::Session().run(argc, argv);
}
