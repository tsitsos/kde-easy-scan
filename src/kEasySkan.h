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

#ifndef kEasySkan_h
#define kEasySkan_h

#include <QDir>
#include <QDialog>
#include <QUrl>
#include <QStandardPaths>
#include <QIcon>

#include <KSaneWidget>

#include "ui_settings.h"
#include "ImageViewer.h"
#include <KMessageWidget>
#include <QDialogButtonBox>
#include <QFrame>
#include <QPixmap>
#include <QToolButton>
#include <QPrintDialog>
#include <QPrinter>

class KAboutData;

using namespace KSaneIface;

class kEasySkan : public QDialog
{
    Q_OBJECT

public:
    explicit kEasySkan(const QString &device, QWidget *parent);
    void setAboutData(KAboutData *aboutData);

private:
    // Order of items in save mode combo-box
    enum saveMode {
        standardMode = 0,
        fastImg = 1,
        fastPdf = 2,
        singlePdf = 3
    };

    void readSettings();
    void saveSettings();
    void loadScannerOptions();

private Q_SLOTS:
    
    QString getPasswd();
    QString numberToString (int i, int length);
    bool gsMerge(const QString fName1, const QString fName2);
    int  autoNumber (const QString fName);
    void AppendToPdf();
    void CreatePdf();
    void ImageWriter(const QString fName,const QByteArray fFormat, int fQuality);
    void OpenWithDefault();
    void OpenWithGimp();
    void OpenWithOther();
    void SaveToSinglePdf();
    void alertUser(int type, const QString &strStatus);
    void availableDevices(const QList<KSaneWidget::DeviceInfo> &deviceList);
    void buttonPressed(const QString &optionName, const QString &optionLabel, bool pressed);
    void defaultScannerOptions();
    void docUploader(const QUrl fName);
    void gsPasswd(const QString fName);
    void imageReady(QByteArray &, int, int, int, int);
    void mailTo();
    void pdfWriter(const QString fName, bool writeDocName);
    void printImage();
    void saveDocument();
    void saveScannerOptions();
    void saveWindowSize();
    void sendToClipboard();
    void showAboutDialog();
    void showHelp();
    void showSettingsDialog();
    
Q_SIGNALS: 
    void processFinished();
    void processStarted();
    
protected:
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

private:
    KAboutData              *m_aboutData;
    KSaneWidget             *m_ksanew = nullptr;
    Ui::kEasySkanSettings    m_settingsUi;
    QDialog                 *m_settingsDialog = nullptr;
    QDialog                 *m_showImgDialog = nullptr;
    QDialog                  *msgDlg = nullptr;
    QLabel                   *lbl = new QLabel(msgDlg);
    QString                  m_deviceName;
    QMap<QString, QString>   m_defaultScanOpts;
    QByteArray               m_data;
    int                      m_width;
    int                      m_height;
    int                      m_bytesPerLine;
    int                      m_format;
    ImageViewer              m_imageViewer;
    QStringList              m_filterList;
    QStringList              m_filter16BitList;
    QStringList              m_typeList;

    QByteArray        imageFormat;
    QImage            mImage;
    QPrinter         *printer ;
    QString           PdfExistingFileName;
    QString           SinglePdfFileName ;
    QString           fileNumberAsString;
    QString           imageFormatAsString;
    QString           imageNamePrefix;
    QString           imgDir;
    QString           pdfDir;
    QString           pdfNamePrefix;
    QString           pdfPasswd;
    QString           tmpDir;
    QString           mmailAddress;
    QString           mmailBody;
    QString           mmailClient;
    QString           mmailSubject;
    QUrl              SinglePdfFileUrl;
    QUrl              imgUrl;
    QUrl              pdfUrl;
    bool              dirNotFound=false;
    bool              firstPage=true;
    bool              firstPageCreated=false;
    bool              gimpExists;
    bool              gsExists;
    bool              gsMergeOk;
    bool              gsPasswdOk;
    bool              isExisting=false;
    bool              pdfPasswdIsSet=false;
    bool              pdfWriterSuccess;
    bool              targetImgDirLocal=true;
    bool              targetPdfDirLocal=true;
    bool              uploadFileOk;
    bool              useIdentifier=true;
    bool              writeOk;
    int               counter=0;
    int               fileNumber;
    int               imageQuality;

QString  pathToExecFile = QStandardPaths::findExecutable(QStringLiteral("kEasySkan"));
QUrl     iconDir = QUrl::fromLocalFile(pathToExecFile);
QString  iconPath = (iconDir.resolved(QUrl((QStringLiteral("../share/kEasySkan/icons/"))))).path();
    
QIcon    settingsIcon = QIcon::fromTheme(QStringLiteral("configure"),QIcon( iconPath+QStringLiteral("/settings.svg")));
QIcon    saveIcon = QIcon::fromTheme(QStringLiteral("document-save"),QIcon( iconPath+QStringLiteral("/save.svg")));
QIcon    okApplyIcon = QIcon::fromTheme(QStringLiteral("dialog-ok"),QIcon( iconPath+QStringLiteral("/okApply.svg"))); 
QIcon    cancelIcon = QIcon::fromTheme(QStringLiteral("dialog-cancel"),QIcon( iconPath+QStringLiteral("/cancel.svg")));
QIcon    printIcon =QIcon::fromTheme(QStringLiteral("document-print"),QIcon( iconPath+QStringLiteral("/print.svg")));
QIcon    defaultViewerIcon =QIcon::fromTheme(QStringLiteral("graphics-viewer-document"),QIcon( iconPath+QStringLiteral("/defaultviewer.svg"))); 
QIcon    gimpIcon =QIcon( iconPath+QStringLiteral("/gimp.svg")); 
QIcon    clipboardIcon =QIcon::fromTheme(QStringLiteral("document-swap"),QIcon( iconPath+QStringLiteral("/clipboard.svg"))); 
QIcon    mailIcon =QIcon::fromTheme(QStringLiteral("mail-message"),QIcon( iconPath+QStringLiteral("/mail.svg"))); 
QIcon    openWithIcon =QIcon::fromTheme(QStringLiteral("document-open"),QIcon( iconPath+QStringLiteral("/openwith.svg"))); 
QIcon    pdfCreateIcon =QIcon( iconPath+QStringLiteral("/pdfcreate.svg")); 
QIcon    pdfAppendIcon =QIcon( iconPath+QStringLiteral("/pdfappend.svg")); 
QIcon    pdfButtonIcon =QIcon( iconPath+QStringLiteral("/pdfbutton.svg")); 
// QIcon    pdfEditColorsIcon =QIcon( iconPath+QStringLiteral("/pdfeditcolors.svg")); 
QIcon    editUndoIcon  =QIcon::fromTheme(QStringLiteral("edit-undo"),QIcon( iconPath+QStringLiteral("/edit-undo.svg")));
QIcon    otherActionsIcon  =QIcon::fromTheme(QStringLiteral("document-export"),QIcon( iconPath+QStringLiteral("/otheractionsbutton.svg")));
};

#endif

