/* ============================================================
* Copyright (C) 2018 by George Tsitsos <tsitsos@yahoo.com>
* Copyright (C) 2007-2012 by Kåre Särs <kare.sars@iki .fi>
* Copyright (C) 2014 by Gregor Mitsch: port to KDE5 frameworks
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License or (at your option) version 3 or any later version
* accepted by the membership of KDE e.V. (or its successor approved
*  by the membership of KDE e.V.), which shall act as a proxy
* defined in Section 14 of version 3 of the license.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License.
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*
* ============================================================ */

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>

#include <KAboutData>
#include <KLocalizedString>
#include <Kdelibs4ConfigMigrator>

#include "kEasySkan.h"
#include "version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Kdelibs4ConfigMigrator migrate(QStringLiteral("kEasySkan"));
    migrate.setConfigFiles(QStringList() << QStringLiteral("kEasySkanrc"));
    migrate.migrate();

    KLocalizedString::setApplicationDomain("kEasySkan");

    KAboutData aboutData(QStringLiteral("kEasySkan"), // componentName, k4: appName
                         i18n("kEasySkan"), // displayName, k4: programName
                         QLatin1String(kEasySkan_version), // version
                         i18n("Easy scanning for KDE based on skanlite"), // shortDescription
                         KAboutLicense::GPL, // licenseType
                         i18n("(C) 2018 George Tsitsos"), // copyrightStatement
                         QString(), // other Text
                         QString() // homePageAddress
                        );

    aboutData.addAuthor(i18n("George Tsitsos"),
                        i18n("developer"),
                        QStringLiteral("tsisos@yahoo.com"));

    aboutData.addAuthor(i18n("All skanlite contributors"),
                        i18n("contributor"));

    
        
    // Required for showing the translation list KXmlGui is not used
    aboutData.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"),
                            i18nc("EMAIL OF TRANSLATORS", "Your emails"));

    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("scanner")));

    QCoreApplication::setApplicationVersion(aboutData.version());
    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption deviceOption(QStringList() << QStringLiteral("d") << QStringLiteral("device"), i18n("Sane scanner device name. Use 'test' for test device."), i18n("device"));
    parser.addOption(deviceOption);
    parser.process(app); // the --author and --license is shown anyway but they work only with the following line
    aboutData.processCommandLine(&parser);

    const QString deviceName = parser.value(deviceOption);
    qDebug() << QString::fromLatin1("deviceOption value=%1").arg(deviceName);

    kEasySkan kEasySkanDialog(deviceName, 0);
    kEasySkanDialog.setAboutData(&aboutData);

    kEasySkanDialog.show();

    return app.exec();
}

