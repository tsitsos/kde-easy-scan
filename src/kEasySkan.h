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
    void showSettingsDialog();
    void getImgDir();
    void getPdfDir();
    void imageReady(QByteArray &, int, int, int, int);
    void saveDocument();
    void showAboutDialog();
    void saveWindowSize();

    void saveScannerOptions();
    void defaultScannerOptions();

    void availableDevices(const QList<KSaneWidget::DeviceInfo> &deviceList);

    void alertUser(int type, const QString &strStatus);
    void buttonPressed(const QString &optionName, const QString &optionLabel, bool pressed);

    void showHelp();

    
    void sendToClipboard();
    void printImage();
    void OpenWithGimp();
    void OpenWithDefault();
    void OpenWithOther();
    void ImageWriter(const QString fName,const QByteArray fFormat, int fQuality);
    void AppendToPdf();
    void PdfWriter(const QString fName);
    void CreatePdf();
    QString numberToString (int i, int length);
    int autoNumber (const QString fName);
    void SaveToSinglePdf();
    bool gsMerge(const QString fName);
    
    
protected:
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

private:
    KAboutData              *m_aboutData;
    KSaneWidget             *m_ksanew = nullptr;
    Ui::kEasySkanSettings     m_settingsUi;
    QDialog                 *m_settingsDialog = nullptr;
    QDialog                 *m_showImgDialog = nullptr;
    // having this variable here is not so nice; ShowImgageDialog should be separate class
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
    
    

    QPrinter         *printer ;
    QImage            mImage;
    bool              writeOk;
    bool              PdfWriterSuccess;
    QString           tmpDir;
    QByteArray        imageFormat;
    QString           imageFormatAsString;
    int               imageQuality;
    int               fileNumber;
    QString           fileNumberAsString;
    QString           imgDir;
    QString           pdfDir;
    QString           pdfNamePrefix;
    QString           imageNamePrefix;
    bool              gimpExists;
    bool              gsExists;
    bool              useIdentifier=true;
    bool              dirNotFound=false;
    bool              firstPage=true;
    bool              firstPageCreated=false;
    QString           SinglePdfFileName ;
    QString           PdfExistingFileName;
    bool              gsMergeOk;
    bool              isExisting=false;
};

#endif

