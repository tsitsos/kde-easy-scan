/* ============================================================
 * Copyright (C) 2018 by George Tsitsos <tsitsos@yahoo.com>
 * Copyright (C) 2007-2012 by Kåre Särs <kare.sars@iki .fi>
 * Copyright (C) 2009 by Arseniy Lartsev <receive-spam at yandex dot ru>
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

#include "kEasySkan.h"

#include "KSaneImageSaver.h"


#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDate>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QImageWriter>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPrinterInfo>
#include <QProcess>
#include <QProgressBar>
#include <QProgressDialog>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QStringList>
#include <QTemporaryFile>
#include <QTimer>
#include <QToolButton>
#include <QUrl>

#include <KAboutApplicationDialog>
#include <KLocalizedString>
#include <KIO/StatJob>
#include <KIO/Job>
#include <KJobWidgets>
#include <kio/global.h>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KHelpClient>
#include <KGuiItem>
#include <KStandardAction>
#include <KStandardGuiItem>
#include <KHelpMenu>
#include <KAboutData>
#include <KMessageBox>
#include <KMessageWidget>
#include <KRun>
#include <KOpenWithDialog>
#include <KNewPasswordDialog>



#include <errno.h>
#include <sstream>
#include <iomanip>
#include <string>

kEasySkan::kEasySkan(const QString &device, QWidget *parent)
: QDialog(parent)
, m_aboutData(nullptr)


{
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    QDialogButtonBox *dlgButtonBoxBottom = new QDialogButtonBox(this);
    dlgButtonBoxBottom->setStandardButtons(QDialogButtonBox::Help | QDialogButtonBox::Close);
    // was "User2:
    QPushButton *btnAbout = dlgButtonBoxBottom->addButton(i18n("About"), QDialogButtonBox::ButtonRole::ActionRole);
    // was "User1":
    QPushButton *btnSettings = dlgButtonBoxBottom->addButton(i18n("Settings"), QDialogButtonBox::ButtonRole::ActionRole);
    btnSettings->setIcon(settingsIcon);
    
    m_ksanew = new KSaneIface::KSaneWidget(this);
    connect(m_ksanew, &KSaneWidget::imageReady, this, &kEasySkan::imageReady);
    connect(m_ksanew, &KSaneWidget::availableDevices, this, &kEasySkan::availableDevices);
    connect(m_ksanew, &KSaneWidget::userMessage, this, &kEasySkan::alertUser);
    connect(m_ksanew, &KSaneWidget::buttonPressed, this, &kEasySkan::buttonPressed);
    
    mainLayout->addWidget(m_ksanew);
    mainLayout->addWidget(dlgButtonBoxBottom);
    
    m_ksanew->initGetDeviceList();
    
    // read the size here...
    KConfigGroup window(KSharedConfig::openConfig(), "Window");
    QSize rect = window.readEntry("Geometry", QSize(740, 400));
    resize(rect);
    
    connect(dlgButtonBoxBottom, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(this, &QDialog::finished, this, &kEasySkan::saveWindowSize);
    connect(this, &QDialog::finished, this, &kEasySkan::saveScannerOptions);
    connect(btnSettings, &QPushButton::clicked, this, &kEasySkan::showSettingsDialog);
    connect(btnAbout, &QPushButton::clicked, this, &kEasySkan::showAboutDialog);
    connect(dlgButtonBoxBottom, &QDialogButtonBox::helpRequested, this, &kEasySkan::showHelp);
    
    
    
    //
    // Create the settings dialog
    //
    {
        m_settingsDialog = new QDialog(this);
        
        QVBoxLayout *mainLayout = new QVBoxLayout(m_settingsDialog);
        
        QWidget *settingsWidget = new QWidget(m_settingsDialog);
        m_settingsUi.setupUi(settingsWidget);
        m_settingsUi.revertOptions->setIcon(editUndoIcon);
        
        mainLayout->addWidget(settingsWidget);
        
        QDialogButtonBox *dlgButtonBoxBottom = new QDialogButtonBox(this);
        QPushButton *doneButton = dlgButtonBoxBottom->addButton(i18n("Done"), QDialogButtonBox::ButtonRole::ActionRole);
        doneButton->setToolTip(i18n("Apply and save changes"));
        doneButton->setIcon(okApplyIcon);
        QPushButton *discardButton = dlgButtonBoxBottom->addButton(i18n("Discard"), QDialogButtonBox::ButtonRole::ActionRole);
        discardButton->setToolTip(i18n("Discard changes"));
        discardButton->setIcon(cancelIcon);
        
        connect(doneButton, &QPushButton::clicked, this ,&kEasySkan::saveSettings );
        connect(discardButton, &QPushButton::clicked, this ,&kEasySkan::readSettings );
        
        mainLayout->addWidget(dlgButtonBoxBottom);
        
        m_settingsDialog->setWindowTitle(i18n("kEasySkan Settings"));
        
        //         connect(m_settingsUi.getImgDirButton, &QPushButton::clicked, this, &kEasySkan::getImgDir);
        //         connect(m_settingsUi.getPdfDirButton, &QPushButton::clicked, this, &kEasySkan::getPdfDir);
        connect(m_settingsUi.revertOptions,&QPushButton::clicked, this, &kEasySkan::defaultScannerOptions);
        
        readSettings();
        
    }
    
    
    // open the scan device
    if (m_ksanew->openDevice(device) == false) {
        QString dev = m_ksanew->selectDevice(0);
        if (dev.isEmpty()) {
            // either no scanner was found or then cancel was pressed.
            exit(0);
        }
        if (m_ksanew->openDevice(dev) == false) {
            // could not open a scanner
            KMessageBox::sorry(0, i18n("Opening the selected scanner failed."));
            exit(1);
        }
        else {
            setWindowTitle(i18nc("@title:window %1 = scanner maker, %2 = scanner model", "%1 %2 - kEasySkan", m_ksanew->make(), m_ksanew->model()));
            m_deviceName = QString::fromLatin1("%1:%2").arg(m_ksanew->make()).arg(m_ksanew->model());
        }
    }
    else {
        setWindowTitle(i18nc("@title:window %1 = scanner device", "%1 - kEasySkan", device));
        m_deviceName = device;
    }
    
    
    // check for gimp and ghostscript
    {
        
        QString gimpSystemName = QStandardPaths::findExecutable(QStringLiteral("gimp"));
        QString gsSystemName = QStandardPaths::findExecutable(QStringLiteral("gs"));
        
        if (gimpSystemName.isEmpty()==true) { // gimp is not installed in standard path
            KMessageBox::sorry(0,i18n("Gimp executable was not found. Edit with Gimp will be disabled.\n\nNo gimp in:  ")\
            +QString::fromUtf8(qgetenv("PATH")));
            
            gimpExists=false;
        }
        else {
            gimpExists=true;
        }
        
        if (gsSystemName.isEmpty()==true) { // ghostscript is not installed in standard path
            KMessageBox::sorry(0,i18n("Ghostscript executable was not found. Most of the PDF operations will be disabled.\n\nNo gs in:  ")\
            +QString::fromUtf8(qgetenv("PATH")));
            gsExists=false;
            QStandardItemModel* model = qobject_cast<QStandardItemModel*>(m_settingsUi.saveMode->model());
            bool disabled= true;
            QStandardItem* saveModeItem= model->item(3);
            saveModeItem->setFlags(disabled? saveModeItem->flags() & ~Qt::ItemIsEnabled: saveModeItem->flags() | Qt::ItemIsEnabled);
            m_settingsUi.saveMode->setCurrentIndex(0);
            m_settingsUi.pdfEncrypt->setChecked(false);
            m_settingsUi.pdfEncrypt->setEnabled(false);
            m_settingsUi.pdfQuality->setCurrentText(QString());
            m_settingsUi.pdfQuality->setEnabled(false);
            saveSettings();
        }
        else {
            gsExists=true;
        }
        
    }
    
   
    // prepare the progress message dialog
    {
        msgDlg = new QDialog(this,Qt::SplashScreen);
        QGridLayout *lout = new QGridLayout(msgDlg);
        lbl->adjustSize();
        lout->addWidget(lbl);
        QProgressBar *prg = new QProgressBar(msgDlg);
        prg->setRange(0,0);
        lout->addWidget(prg);
    }
    
    
    
    // prepare the Show Image Dialog
    {
        
        m_showImgDialog = new QDialog(this);
        
        QVBoxLayout *mainLayout = new QVBoxLayout(m_showImgDialog);
        
        QDialogButtonBox *showImgButtonBox = new QDialogButtonBox(m_showImgDialog);
        
        // define and add the buttons to buttonbox 
        
        QToolButton *moreActionsButton = new QToolButton (m_showImgDialog); 
        QToolButton *pdfButton = new QToolButton (m_showImgDialog); 
        QPushButton *saveButton = showImgButtonBox->addButton(i18n("Save Image"), QDialogButtonBox::ButtonRole::ActionRole);
        QPushButton *printButton = showImgButtonBox->addButton(i18n("Print"), QDialogButtonBox::ButtonRole::ActionRole);
        showImgButtonBox->addButton(pdfButton, QDialogButtonBox::ActionRole);
        showImgButtonBox->addButton(moreActionsButton, QDialogButtonBox::ActionRole);
        QPushButton *discardButton = showImgButtonBox->addButton(i18n("Discard"), QDialogButtonBox::ButtonRole::ActionRole);
        
        //set button properties
        
        printButton->setToolTip(i18n("Send current image to printer or save to local pdf file."));
        printButton->setIcon(printIcon);
        saveButton->setToolTip(i18n("Save image to disk."));
        saveButton->setIcon(saveIcon);
        discardButton->setToolTip(i18n("Discard window"));
        discardButton->setIcon(cancelIcon);
        
        //define moreactions menu and its actions
        
        QMenu *menuMoreActions = new QMenu();
        QAction *MoreActionsAction1 = new QAction(i18n("View image in default viewer"), this);
        QAction *MoreActionsAction2 = new QAction(i18n("Edit image with gimp"), this);
        QAction *MoreActionsAction3 = new QAction(i18n("Copy image to Clipboard"),this);
        QAction *MoreActionsAction4 = new QAction(i18n("Settings"),this);
        QAction *MoreActionsAction5 = new QAction(i18n("Mail to"),this);
        QAction *MoreActionsAction6 = new QAction(i18n("Other Application"),this);
        
        QString gIcon = QStringLiteral("/usr/share/icons/breeze-dark/apps/48/gimp.svg");
        MoreActionsAction1->setIcon(defaultViewerIcon);
        MoreActionsAction2->setIcon(gimpIcon);
        MoreActionsAction3->setIcon(clipboardIcon);
        MoreActionsAction4->setIcon(settingsIcon);
        MoreActionsAction5->setIcon(mailIcon);
        MoreActionsAction6->setIcon(openWithIcon);
        MoreActionsAction6->setShortcuts(KStandardShortcut::open());
        
        if (gimpExists==false) {
            MoreActionsAction2->setEnabled(false);
        }
        
        //add moreActions to their menu
        
        menuMoreActions->addAction(MoreActionsAction1);
        menuMoreActions->addAction(MoreActionsAction2);
        menuMoreActions->addAction(MoreActionsAction3);
        menuMoreActions->addAction(MoreActionsAction4);
        menuMoreActions->addAction(MoreActionsAction5);
        menuMoreActions->addSeparator();
        menuMoreActions->addAction(MoreActionsAction6);
        
        //set properties of moreActionsButton
        
        moreActionsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        moreActionsButton->setPopupMode(QToolButton::MenuButtonPopup);
        moreActionsButton->setToolTip(i18n("More Actions"));
        moreActionsButton->setIcon(otherActionsIcon);
        moreActionsButton->setText(i18n("More Actions"));
        
        //  add menuMoreActions to moreActionsButton
        
        moreActionsButton->setMenu(menuMoreActions);
        
        //define pdfactions menu and its actions
        
        QMenu *menuPdf = new QMenu();
        QAction *PdfAction1 = new QAction(i18n("Create PDF"), this);
        QAction *PdfAction2 = new QAction(i18n("Append to PDF"), this);
        
        PdfAction1->setIcon(pdfCreateIcon);
        PdfAction2->setIcon(pdfAppendIcon);
        
        if (gsExists==false) {
            
            PdfAction2->setEnabled(false);
        }
        
        
        // add actions to pdfmenu
        menuPdf->addAction(PdfAction1);
        menuPdf->addAction(PdfAction2);
        
        //set properties of pdfButton
        
        pdfButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        pdfButton->setPopupMode(QToolButton::MenuButtonPopup);
        pdfButton->setToolTip(i18n("Create a new PDF file or append several documents into one PDF file"));
        pdfButton->setIcon(pdfButtonIcon);
        pdfButton->setText(i18n("PDF Actions"));
        
        //  add menuPdf to pdfButton
        
        pdfButton->setMenu(menuPdf);
        
        
        // set a divider to be used between scan image and showImgButtonBox
        
        QFrame *mDivider = new QFrame(m_showImgDialog);
        mDivider->setFrameShape(QFrame::HLine);
        mDivider->setLineWidth(6);
        
        
        // configue the window's layout   
        
        mainLayout->addWidget(&m_imageViewer);
        mainLayout->addWidget(mDivider);
        mainLayout->addWidget(showImgButtonBox);
        
        // connect buttons and actions to functions
        
        connect(MoreActionsAction1, &QAction::triggered, this, &kEasySkan::OpenWithDefault);
        connect(MoreActionsAction2, &QAction::triggered, this, &kEasySkan::OpenWithGimp);
        connect(MoreActionsAction3, &QAction::triggered, this, &kEasySkan::sendToClipboard);
        connect(MoreActionsAction4, &QAction::triggered, this, &kEasySkan::showSettingsDialog);
        connect(MoreActionsAction5, &QAction::triggered, this, &kEasySkan::mailTo);
        connect(MoreActionsAction6, &QAction::triggered, this, &kEasySkan::OpenWithOther);
        connect(PdfAction1, &QAction::triggered, this, &kEasySkan::CreatePdf);
        connect(PdfAction2, &QAction::triggered, this, &kEasySkan::AppendToPdf);
        connect(saveButton, &QPushButton::clicked, this, &kEasySkan::saveDocument);
        connect(printButton, &QPushButton::clicked, this, &kEasySkan::printImage);
        connect(discardButton, &QPushButton::clicked, m_showImgDialog,&QDialog::reject );
        
        
        m_showImgDialog->resize(640, 480);
        
    }
    
    
    // save the default sane options for later use
    m_ksanew->getOptVals(m_defaultScanOpts);
    
    // load saved options
    loadScannerOptions();
    
    m_ksanew->initGetDeviceList();
    
    
}

void kEasySkan::showHelp()
{
    QUrl helpFile = QUrl::fromLocalFile(pathToExecFile);
    QUrl relative(QStringLiteral("../share/kEasySkan/help/index.html"));
    QDesktopServices::openUrl(helpFile.resolved(relative));
    
}

void kEasySkan::setAboutData(KAboutData *aboutData)
{
    m_aboutData = aboutData;
}

void kEasySkan::closeEvent(QCloseEvent *event)
{
    saveWindowSize();
    saveScannerOptions();
    event->accept();
}

void kEasySkan::saveWindowSize()
{
    KConfigGroup window(KSharedConfig::openConfig(), "Window");
    window.writeEntry("Geometry", size());
    window.sync();
}



void kEasySkan::readSettings()
{
    
    m_settingsDialog->close(); // it doesn't hurt to close if already closed
    
    // enable the widgets to allow modifying
    m_settingsUi.setPreviewDPI->setChecked(true);
    
    // "Image saving"
    
    KConfigGroup imgSaving(KSharedConfig::openConfig(), "Image Saving");
    
    m_settingsUi.dateTimeAppend->setChecked(imgSaving.readEntry("Append Date and Time", true));
    m_settingsUi.numberAppend->setChecked(imgSaving.readEntry("Append Number", false));
    m_settingsUi.autoNumberAppend->setChecked(imgSaving.readEntry("Auto Append Number", false));
    m_settingsUi.saveMode->setCurrentIndex(imgSaving.readEntry("Save Mode", (int)standardMode));
    m_settingsUi.fileNumber->setValue(imgSaving.readEntry("File Number",1));
    
    // IMAGE
    
    m_settingsUi.saveImgDir->setText(imgSaving.readEntry("Image Save Location", QDir::homePath()));
    m_settingsUi.imgPrefix->setText(imgSaving.readEntry("Image Name Prefix", i18n("My Image")));
    m_settingsUi.imgFormat->setText(imgSaving.readEntry("Image Format","JPG"));
    m_settingsUi.imgQuality->setValue(imgSaving.readEntry("Image Quality",90));
    
    //PDF
    
    m_settingsUi.savePdfDir->setText(imgSaving.readEntry("PDF Save Location", QDir::homePath()));
    m_settingsUi.pdfPrefix->setText(imgSaving.readEntry("PDF Name Prefix", i18n("My PDF")));
    m_settingsUi.pdfQuality->setCurrentText(imgSaving.readEntry("PDF Quality",QStringLiteral("high")));
    m_settingsUi.pdfQuality->setEnabled(false);
    m_settingsUi.pdfEncrypt->setChecked(imgSaving.readEntry("PDF Encrypt",false));
    
    //Temp Files Location
    
    m_settingsUi.tmpDir->setText(imgSaving.readEntry("Temp Files Location",QDir::tempPath()));
    
    // define varialbles to be used from now on just for simplicity
    
    tmpDir=m_settingsUi.tmpDir->text();
    if ( tmpDir.data()[tmpDir.size()-1] != QStringLiteral("/") ) {
        tmpDir.append(QStringLiteral("/"));
        m_settingsUi.tmpDir->setText(tmpDir);
    }
    imgUrl=m_settingsUi.saveImgDir->url();
    
    QString imgUrlAsString = imgUrl.toString();
    if ( imgUrlAsString.data()[imgUrlAsString.size()-1] != QStringLiteral("/") ) {
        imgUrlAsString.append(QStringLiteral("/"));
        imgUrl=QUrl(imgUrlAsString);
        m_settingsUi.saveImgDir->setUrl(imgUrl);
    }
    
    if (imgUrl.isLocalFile()) {
        targetImgDirLocal=true;
        imgDir=imgUrl.path();
        if (!QFile::exists(imgDir)) {
            dirNotFound=true;
            if (KMessageBox::questionYesNo(0,i18n("The directory ")+imgDir+i18n(" doesn't exist. Do you wish to create it?"))==3) {
                QDir createDir;
                if (createDir.mkpath(imgDir)==false) {
                    KMessageBox::error(0,imgDir+i18n("  couldn't be created. Check your pemissions."));
                }
            }
            
        }
    } 
    else 
    {
        targetImgDirLocal=false;
        
    }
    imageQuality=(m_settingsUi.imgQuality->value());
    imageFormatAsString=m_settingsUi.imgFormat->text();
    imageFormat=imageFormatAsString.toUtf8(); // QByteArray
    imageNamePrefix=m_settingsUi.imgPrefix->text();
    fileNumber=(m_settingsUi.fileNumber->value());
    pdfUrl=m_settingsUi.savePdfDir->url();
    
    QString pdfUrlAsString = pdfUrl.toString();
    if ( pdfUrlAsString.data()[pdfUrlAsString.size()-1] != QStringLiteral("/") ) {
        pdfUrlAsString.append(QStringLiteral("/"));
        pdfUrl=QUrl(pdfUrlAsString);
        m_settingsUi.savePdfDir->setUrl(pdfUrl);
    }
    
    
    if (pdfUrl.isLocalFile()) {
        targetPdfDirLocal=true;
        pdfDir=pdfUrl.path();
        if (!QFile::exists(pdfDir)) {
            dirNotFound=true;
            if (KMessageBox::questionYesNo(0,i18n("The directory ")+pdfDir+i18n(" doesn't exist. Do you wish to create it?"))==3) {
                QDir createDir;
                if (createDir.mkpath(pdfDir)==false) {
                    KMessageBox::error(0,pdfDir+i18n("  couldn't be created. Check your pemissions."));
                }    
            }
            
        }
        
    } 
    else 
    {
        targetPdfDirLocal=false;
        
    }
    
    pdfNamePrefix=m_settingsUi.pdfPrefix->text();
    dirNotFound=false;
    
    
    // check if auto number append is checked and disables the other two
    
    if (m_settingsUi.autoNumberAppend->isChecked()==true) {
        m_settingsUi.numberAppend->setChecked(false);
        m_settingsUi.dateTimeAppend->setChecked(false);
    }
    
    if (m_settingsUi.dateTimeAppend->isChecked()==false && m_settingsUi.numberAppend->isChecked()==false && m_settingsUi.autoNumberAppend->isChecked()==false) {
        if (m_settingsUi.saveMode->currentIndex()==fastImg || m_settingsUi.saveMode->currentIndex()==fastPdf) {
            useIdentifier=false;
        }
    }
    else{
        useIdentifier=true;
    }
    
    //   "General"        
    
    KConfigGroup general(KSharedConfig::openConfig(), "General");
    
    m_settingsUi.previewDPI->setCurrentText(general.readEntry("PreviewDPI", "100"));
    
    m_settingsUi.setPreviewDPI->setChecked(general.readEntry("SetPreviewDPI", false));
    if (m_settingsUi.setPreviewDPI->isChecked()) {
        m_ksanew->setPreviewResolution(m_settingsUi.previewDPI->currentText().toFloat());
    }
    else {
        m_ksanew->setPreviewResolution(0.0);
    }
    m_settingsUi.u_disableSelections->setChecked(general.readEntry("DisableAutoSelection", false));
    m_ksanew->enableAutoSelect(!m_settingsUi.u_disableSelections->isChecked());
    
}


void kEasySkan::saveSettings()
{
    // Image saving
    
    m_settingsDialog->close(); // it doesn't hurt to close if already closed
    
    KConfigGroup imgSaving(KSharedConfig::openConfig(), "Image Saving");
    
    // 
    imgSaving.writeEntry("Save Mode", m_settingsUi.saveMode->currentIndex());
    imgSaving.writeEntry("Image Save Location", m_settingsUi.saveImgDir->text());
    imgSaving.writeEntry("Image Name Prefix", m_settingsUi.imgPrefix->text());
    imgSaving.writeEntry("Image Format",m_settingsUi.imgFormat->text());
    imgSaving.writeEntry("Image Quality",m_settingsUi.imgQuality->value());
    imgSaving.writeEntry("File Number",m_settingsUi.fileNumber->value());
    
    imgSaving.writeEntry("PDF Save Location",m_settingsUi.savePdfDir->text());
    imgSaving.writeEntry("PDF Name Prefix",m_settingsUi.pdfPrefix->text());
    imgSaving.writeEntry("PDF Quality",m_settingsUi.pdfQuality->currentText());
    imgSaving.writeEntry("PDF Encrypt",m_settingsUi.pdfEncrypt->isChecked());
    imgSaving.writeEntry("Append Date and Time", m_settingsUi.dateTimeAppend->isChecked());
    imgSaving.writeEntry("Append Number", m_settingsUi.numberAppend->isChecked());
    imgSaving.writeEntry("Auto Append Number", m_settingsUi.autoNumberAppend->isChecked());
    
    imgSaving.writeEntry("Temp Files Location",m_settingsUi.tmpDir->text());
    
    imgSaving.sync();
    
    //      General 
    
    KConfigGroup general(KSharedConfig::openConfig(), "General");
    
    general.writeEntry("PreviewDPI", m_settingsUi.previewDPI->currentText());
    general.writeEntry("SetPreviewDPI", m_settingsUi.setPreviewDPI->isChecked());
    general.writeEntry("DisableAutoSelection", m_settingsUi.u_disableSelections->isChecked());
    
    general.sync();
    
    readSettings(); // make changes, if any, effective at once.
    
}


void kEasySkan::showSettingsDialog()
{
    m_settingsDialog->exec();
}    



void kEasySkan::imageReady(QByteArray &data, int w, int h, int bpl, int f)
{
    // save the image data
    m_data = data;
    m_width = w;
    m_height = h;
    m_bytesPerLine = bpl;
    m_format = f;
    
    // copy the image data into mImage
    mImage = m_ksanew->toQImageSilent(data, w, h, bpl, (KSaneIface::KSaneWidget::ImageFormat)f);
    
    if (m_settingsUi.saveMode->currentIndex()==standardMode) {
        
        m_imageViewer.setQImage(&mImage);
        m_imageViewer.zoom2Fit();
        m_showImgDialog->exec();
    }
    
    else {
        saveDocument();
    }
}


void kEasySkan::saveDocument()
{
    QString suggestedFileName ;
    QUrl suggestedFileUrl;
    QDateTime dateTime = dateTime.currentDateTime();
    
    
    // Standard Mode
    if (m_settingsUi.saveMode->currentIndex()==standardMode) {
        
        
        if (m_settingsUi.dateTimeAppend->isChecked()==true) {
            suggestedFileName.append(QStringLiteral("-")+dateTime.toString(QStringLiteral("yyyy.MM.dd-hh.mm.ss")));
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
        }
        
        if (m_settingsUi.numberAppend->isChecked()==true) {
            suggestedFileName.append(QStringLiteral("-")+numberToString(fileNumber,4));
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
        }
        
        if (m_settingsUi.autoNumberAppend->isChecked()==true) {
            suggestedFileName.append(QStringLiteral("-"));
            QString strNm=numberToString(autoNumber(imgDir+imageNamePrefix+suggestedFileName),4);
            suggestedFileName.append(strNm);
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
        }
        
        suggestedFileUrl=imgUrl.resolved(QUrl(imageNamePrefix+suggestedFileName));
        //             
        //         KMessageBox::information(0,suggestedFileUrl.toString()+imgDir) ;
        
        QUrl flnm;
        QFileDialog *qfld = new QFileDialog() ;
        qfld->setAcceptMode(QFileDialog::AcceptSave);
//         qfld->setOption(QFileDialog::DontUseNativeDialog,true);
        qfld->setDirectoryUrl(imgUrl);
        qfld->selectUrl(suggestedFileUrl);
        if (qfld->exec()) {
            flnm=qfld->selectedUrls()[0];    
        }
        
        
        if (flnm.isEmpty()) {
            KMessageBox::error(0,i18n("No filename given!"));
            return;
        }
        
        QFileInfo getSuffix (flnm.fileName());
        QString  suffix = getSuffix.suffix(); 
        
        //         KMessageBox::information(0,flnm.toString());
        
        if ((suffix.toUtf8())!=imageFormat) {
            KMessageBox::information(0,i18n("It looks like you are trying to save in a different format than the one in settings. \nIf this fails, check your settings to see the supported formats."));
        }
        
        if (flnm.isLocalFile()) {
            ImageWriter(flnm.toLocalFile(),imageFormat,imageQuality);
            if (writeOk) {
                if (m_settingsUi.numberAppend->isChecked()==true) {
                    fileNumber+=1;
                    m_settingsUi.fileNumber->setValue(fileNumber);
                    saveSettings();
                }
            }
            
        }
        else {
            ImageWriter(tmpDir+flnm.fileName(),imageFormat,imageQuality);
            docUploader(flnm);
            if (uploadFileOk) {
                if (m_settingsUi.numberAppend->isChecked()==true) {
                    fileNumber+=1;
                    m_settingsUi.fileNumber->setValue(fileNumber);
                    saveSettings();
                }
            }
        }
        
    }
    
    //check for identifier
    
    if (useIdentifier==false) {
        KMessageBox::information(0,i18n("Fast saving modes need an identifier to work. Please select to append either Date and Time or Number."));
        showSettingsDialog();    
        if (useIdentifier==false) {return;}
    }
    
    // fast save to image    
    if (m_settingsUi.saveMode->currentIndex()==fastImg) {
       if (!targetImgDirLocal && m_settingsUi.autoNumberAppend->isChecked()) {
           KMessageBox::information (0,i18n("Using auto numbering on remote files is not supported.\nPlease change your settings"));
           showSettingsDialog();
           saveDocument();
        }
        
        // append date and time
        if (m_settingsUi.dateTimeAppend->isChecked()==true) {
            suggestedFileName.append(QStringLiteral("-")+dateTime.toString(QStringLiteral("yyyy.MM.dd-hh.mm.ss")));    
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
            
            if (!targetImgDirLocal) {
                suggestedFileName.prepend(tmpDir+imageNamePrefix);
                ImageWriter(suggestedFileName,imageFormat,imageQuality);
                QUrl q = QUrl::fromLocalFile(suggestedFileName);
                suggestedFileUrl=QUrl(q.fileName());
                docUploader( imgUrl.resolved(suggestedFileUrl) );
            }
            else {
                suggestedFileName.prepend(imgDir+imageNamePrefix);
                ImageWriter(suggestedFileName,imageFormat,imageQuality);
                if (writeOk==true) {
                    KMessageBox::information(0,suggestedFileName+i18n("  successfully saved."));
                    return;
                }
            }
            
        }
        
        // append auto numbering
        if (m_settingsUi.autoNumberAppend->isChecked()==true) {
            
            suggestedFileName.prepend(imgDir+imageNamePrefix);
            suggestedFileName.append(QStringLiteral("-"));
            QString strNm = numberToString(autoNumber(suggestedFileName),4);
            suggestedFileName.append(strNm);
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
            ImageWriter(suggestedFileName,imageFormat,imageQuality);
            if (writeOk==true) {
                KMessageBox::information(0,suggestedFileName+i18n("  successfully saved."));
                return;
            }
        }
        
        // append manual numbering 5
        if (m_settingsUi.numberAppend->isChecked()==true) {
            
            suggestedFileName.append(QStringLiteral("-")+numberToString(fileNumber,4));
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
            
            if (!targetImgDirLocal) {
                suggestedFileName.prepend(tmpDir+imageNamePrefix);
                ImageWriter(suggestedFileName,imageFormat,imageQuality);
                QUrl q = QUrl::fromLocalFile(suggestedFileName);
                suggestedFileUrl=QUrl(q.fileName());
                docUploader( imgUrl.resolved(suggestedFileUrl) );
                if (uploadFileOk) {
                    fileNumber+=1;
                    m_settingsUi.fileNumber->setValue(fileNumber);
                    saveSettings();   
                    return;
                }
            }
            else {
                int j;
                suggestedFileName.prepend(imgDir+imageNamePrefix);
                if (QFile::exists(suggestedFileName)==true) {
                    j = KMessageBox::warningContinueCancel(0,suggestedFileName+i18n(" already exists.\nOverwrite ?")); 
                }
                if (j==2) {return;}
                ImageWriter(suggestedFileName,imageFormat,imageQuality);
                
                if (writeOk==true) {
                    fileNumber+=1;
                    m_settingsUi.fileNumber->setValue(fileNumber);
                    saveSettings();    
                    KMessageBox::information(0,suggestedFileName+i18n("  successfully saved."));
                    return;                
                }
            }
            
        }
        
        
    }
    
    
    // fast save to pdf
    
    if (m_settingsUi.saveMode->currentIndex()==fastPdf) {
        
        if (!targetPdfDirLocal && m_settingsUi.autoNumberAppend->isChecked()) {
           KMessageBox::information (0,i18n("Using auto numbering on remote files is not supported.\nPlease change your settings"));
           showSettingsDialog();
           saveDocument();
        }
        
        if (m_settingsUi.pdfEncrypt->isChecked() && !pdfPasswdIsSet) {
            pdfPasswd=getPasswd();
            if (pdfPasswd.isEmpty()) {
                KMessageBox::information (0,i18n("No password was given! PDF encryption will be disabled."));
                m_settingsUi.pdfEncrypt->setChecked(false);
            } 
            pdfPasswdIsSet=true;
        }
        
        // append date and time
        if (m_settingsUi.dateTimeAppend->isChecked()==true) {
            suggestedFileName.append(QStringLiteral("-")+dateTime.toString(QStringLiteral("yyyy.MM.dd-hh.mm.ss")));    
            suggestedFileName.append(QStringLiteral(".pdf"));
            
            
            if (targetPdfDirLocal) {
                suggestedFileName.prepend(pdfDir+pdfNamePrefix);
                pdfWriter(suggestedFileName,true);
                if (pdfWriterSuccess==true) {
                    if (m_settingsUi.pdfEncrypt->isChecked()) {
                        gsPasswd(suggestedFileName);
                        if (!gsPasswdOk) return;
                    }
                    KMessageBox::information(0,suggestedFileName+i18n("  successfully saved."));
                    return;
                }
            } 
            else {
                suggestedFileName.prepend(tmpDir+pdfNamePrefix);
                pdfWriter(suggestedFileName,true);
                if (!pdfWriterSuccess) return;
                if (m_settingsUi.pdfEncrypt->isChecked()) {
                        gsPasswd(suggestedFileName);
                        if (!gsPasswdOk) return;
                    }
                QUrl q = QUrl::fromLocalFile(suggestedFileName);
                suggestedFileUrl=QUrl(q.fileName());
                docUploader( pdfUrl.resolved(suggestedFileUrl) );
                if (!uploadFileOk) return;
                
            }
        }
        
        // append auto numberting
        if (m_settingsUi.autoNumberAppend->isChecked()==true) {
            suggestedFileName.prepend(pdfDir+pdfNamePrefix);
            suggestedFileName.append(QStringLiteral("-"));
            QString strNm = numberToString(autoNumber(suggestedFileName),4);
            suggestedFileName.append(strNm);
            suggestedFileName.append(QStringLiteral(".pdf"));
            pdfWriter(suggestedFileName,true);
            if (pdfWriterSuccess==true) {
                if (m_settingsUi.pdfEncrypt->isChecked()) {
                        gsPasswd(suggestedFileName);
                        if (!gsPasswdOk) return;
                    }
                KMessageBox::information(0,suggestedFileName+i18n("  successfully saved."));
                return;
            }
        }
        
        
        // append manual numbering
        if (m_settingsUi.numberAppend->isChecked()==true) {
            
            suggestedFileName.append(QStringLiteral("-")+numberToString(fileNumber,4));
            suggestedFileName.append(QStringLiteral(".pdf"));
            
            if (!targetPdfDirLocal) {
                suggestedFileName.prepend(tmpDir+pdfNamePrefix);
                pdfWriter(suggestedFileName,true);
                if (!pdfWriterSuccess) return;
                if (m_settingsUi.pdfEncrypt->isChecked()) {
                        gsPasswd(suggestedFileName);
                        if (!gsPasswdOk) return;
                    }
                QUrl q = QUrl::fromLocalFile(suggestedFileName);
                suggestedFileUrl=QUrl(q.fileName());
                docUploader( pdfUrl.resolved(suggestedFileUrl) );
                if (uploadFileOk) {
                    fileNumber+=1;
                    m_settingsUi.fileNumber->setValue(fileNumber);
                    saveSettings();   
                    return;
                }
            }
            else {
                int j;
                suggestedFileName.prepend(pdfDir+pdfNamePrefix);
                if (QFile::exists(suggestedFileName)==true) {
                    j = KMessageBox::warningContinueCancel(0,suggestedFileName+i18n(" already exists.\nOverwrite ?")); 
                }
                if (j==2) {return;}
                pdfWriter(suggestedFileName,true);
                
                if (pdfWriterSuccess) {
                    if (m_settingsUi.pdfEncrypt->isChecked()) {
                        gsPasswd(suggestedFileName);
                        if (!gsPasswdOk) return;
                    }
                    fileNumber+=1;
                    m_settingsUi.fileNumber->setValue(fileNumber);
                    saveSettings();    
                    KMessageBox::information(0,suggestedFileName+i18n("  successfully saved."));
                    return;                
                }                     
            }
        }
    }
    
    // Fast save to single Pdf
    
    if (m_settingsUi.saveMode->currentIndex()==singlePdf) {
        if (firstPage==true) {
            SinglePdfFileName=QString();
            SinglePdfFileUrl=QUrl();
        }
        SaveToSinglePdf();
        if (firstPageCreated==true) {firstPage=false;}
    }
    
}




void kEasySkan::showAboutDialog()
{
    KAboutApplicationDialog(*m_aboutData).exec();
}

void writeScannerOptions(const QString &groupName, const QMap <QString, QString> &opts)
{
    KConfigGroup options(KSharedConfig::openConfig(), groupName);
    QMap<QString, QString>::const_iterator it = opts.constBegin();
    while (it != opts.constEnd()) {
        options.writeEntry(it.key(), it.value());
        ++it;
    }
    options.sync();
}

void readScannerOptions(const QString &groupName, QMap <QString, QString> &opts)
{
    KConfigGroup scannerOptions(KSharedConfig::openConfig(), groupName);
    opts = scannerOptions.entryMap();
}

void kEasySkan::saveScannerOptions()
{
    
    KConfigGroup options(KSharedConfig::openConfig(), QString::fromLatin1("Options For %1").arg(m_deviceName));
    QMap <QString, QString> opts;
    m_ksanew->getOptVals(opts);
    writeScannerOptions(QString::fromLatin1("Options For %1").arg(m_deviceName), opts);
}

void kEasySkan::defaultScannerOptions()
{
    if (!m_ksanew) {
        return;
    }
    
    m_ksanew->setOptVals(m_defaultScanOpts);
}

void kEasySkan::loadScannerOptions()
{
    
    QMap <QString, QString> opts;
    readScannerOptions(QString::fromLatin1("Options For %1").arg(m_deviceName), opts);
    m_ksanew->setOptVals(opts);
}

void kEasySkan::availableDevices(const QList<KSaneWidget::DeviceInfo> &deviceList)
{
    for (int i = 0; i < deviceList.size(); ++i) {
        qDebug() << deviceList.at(i).name;
    }
}

void kEasySkan::alertUser(int type, const QString &strStatus)
{
    switch (type) {
        case KSaneWidget::ErrorGeneral:
            KMessageBox::sorry(0, strStatus, QStringLiteral("kEasySkan Test"));
            break;
        default:
            KMessageBox::information(0, strStatus, QStringLiteral("kEasySkan Test"));
    }
}

void kEasySkan::buttonPressed(const QString &optionName, const QString &optionLabel, bool pressed)
{
    qDebug() << "Button" << optionName << optionLabel << ((pressed) ? "pressed" : "released");
}




void kEasySkan::printImage()
{
    
    QMessageBox *msgBox2 = new QMessageBox();
    msgBox2->setText(i18n("Preparing to print. Please wait..."));
    msgBox2->setInformativeText(i18n("(This dialog will autoclose)"));
    msgBox2->setIcon(QMessageBox::Information);
    msgBox2->setStandardButtons(QMessageBox::Ok);
    msgBox2->open();
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents,1000); //(workaround) : allow some time for the msgbox to render 
    
    QPrinter *printer = new QPrinter(QPrinter::HighResolution);
    
    QPrintDialog printDialog(printer, this);
    
    msgBox2->close();
    
    if (printDialog.exec()) {
        
        QPainter painter(printer);
        QRect rect = painter.viewport();
        QSize size = mImage.size();
        size.scale(rect.size(), Qt::KeepAspectRatio);
        painter.setViewport(rect.x(), rect.y(),size.width(), size.height());
        painter.setWindow(mImage.rect());
        painter.drawImage(0, 0, mImage);
        painter.end();
        
    }
    
}


void kEasySkan::CreatePdf()
{
    QString suggestedFileName,targetFileName ;
    QUrl suggestedFileUrl ;
    QDateTime dateTime = dateTime.currentDateTime();
    
    suggestedFileName.append(pdfNamePrefix);
    
    if (m_settingsUi.dateTimeAppend->isChecked()==true) {
        suggestedFileName.append(QStringLiteral("-")+dateTime.toString(QStringLiteral("yyyy.MM.dd-hh.mm.ss")));
        suggestedFileName.append(QStringLiteral(".pdf"));
    }
    
    if (m_settingsUi.numberAppend->isChecked()==true) {
        suggestedFileName.append(QStringLiteral("-")+numberToString(fileNumber,4));
        suggestedFileName.append(QStringLiteral(".pdf"));
    }
    
    if (m_settingsUi.autoNumberAppend->isChecked()==true) {
        suggestedFileName.append(QStringLiteral("-"));
        QString strNm=numberToString(autoNumber(pdfDir+suggestedFileName),4);
        suggestedFileName.append(strNm);
        suggestedFileName.append(QStringLiteral(".pdf"));
    }
    
    suggestedFileUrl=pdfUrl.resolved(QUrl(suggestedFileName));
    QUrl targetFileUrl;
        QFileDialog *qfld = new QFileDialog() ;
        qfld->setAcceptMode(QFileDialog::AcceptSave);
        qfld->setDirectoryUrl(imgUrl);
        qfld->selectUrl(suggestedFileUrl);
        if (qfld->exec()) {
            targetFileUrl=qfld->selectedUrls()[0];    
        }
 
    
    if (targetFileUrl.isEmpty()) {
        KMessageBox::error(0,i18n("No filename given!"));
        return;
    }
    
    
    if (m_settingsUi.pdfEncrypt->isChecked() && !pdfPasswdIsSet) {
            pdfPasswd=getPasswd();
            if (pdfPasswd.isEmpty()) {
                KMessageBox::information (0,i18n("No password was given! PDF encryption will be disabled."));
                m_settingsUi.pdfEncrypt->setChecked(false);
            } 
            pdfPasswdIsSet=true;
        }
    
    if (targetFileUrl.isLocalFile()) {
        targetFileName=targetFileUrl.toLocalFile();
        pdfWriter(targetFileName,true);
        if (pdfWriterSuccess==false) {
            return;
        }
        
        if (m_settingsUi.pdfEncrypt->isChecked()) {
            gsPasswd(targetFileName);
            if (!gsPasswdOk) return;
        }
        
        if (m_settingsUi.numberAppend->isChecked()==true) {
            fileNumber+=1;
            m_settingsUi.fileNumber->setValue(fileNumber);
            saveSettings();
        }
        
        bool openAfterFinish = false;
        QDialog *dialog = new QDialog(this, Qt::Dialog);
        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Yes);
        KMessageBox::createKMessageBox(dialog,
                                       buttons,
                                       QMessageBox::Information,
                                       targetFileName+i18n("  was successfully created."),
                                       QStringList(),
                                       i18n("Open file."),
                                       &openAfterFinish,
                                       KMessageBox::Notify);
        
        if (openAfterFinish==true) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(targetFileName));
        }
    }
    
    else {
        suggestedFileName=tmpDir+targetFileUrl.fileName();
        pdfWriter(suggestedFileName,true);
        if (pdfWriterSuccess==false) {
            return;
        }
        
        if (m_settingsUi.pdfEncrypt->isChecked()) {
            gsPasswd(suggestedFileName);
            if (!gsPasswdOk) return;
        }
        
        docUploader( targetFileUrl );
        if (uploadFileOk) {
            if (m_settingsUi.numberAppend->isChecked()==true) {
                fileNumber+=1;
                m_settingsUi.fileNumber->setValue(fileNumber);
                saveSettings();
            }
        }
    }
    pdfPasswdIsSet=false;
}


void kEasySkan::AppendToPdf() // works only with local files

{
    
    bool openAfterFinish = false;
    QDialog *dialog = new QDialog(this, Qt::Dialog);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Yes);
    KMessageBox::createKMessageBox(dialog,
                                   buttons,
                                   QMessageBox::Information,
                                   i18n("You are about to append this document to an existing local PDF file.\n The original file will be backed up."),
                                   QStringList(),
                                   i18n("Open file when done."),
                                   &openAfterFinish,
                                   KMessageBox::Notify);
    
    if (PdfExistingFileName.isEmpty()) {
        if (!pdfUrl.isLocalFile()) { 
            pdfDir=QDir::homePath()+QStringLiteral("/");
        }
        PdfExistingFileName = QFileDialog::getOpenFileName(this, i18n("Open PDF..."), pdfDir, i18n("PDF Files (*.pdf *.PDF *.Pdf)"));
    }
    else 
    {
        PdfExistingFileName = QFileDialog::getOpenFileName(this, i18n("Open PDF..."), PdfExistingFileName, i18n("PDF Files (*.pdf *.PDF *.Pdf)"));
    }
    
    if (PdfExistingFileName.isEmpty()) {
        KMessageBox::error(0,i18n("No filename given!"));
        return;
    }
    
    pdfWriter(tmpDir+QStringLiteral(".kEasySkan.pdf"),false); //writes tmp pdf
    
    if (pdfWriterSuccess==false) {
        
        return;
    }
    
    if (m_settingsUi.pdfEncrypt->isChecked() && !pdfPasswdIsSet) {
        pdfPasswd=getPasswd();
        if (pdfPasswd.isEmpty()) {
            KMessageBox::information (0,i18n("No password was given! PDF encryption will be disabled."));
            m_settingsUi.pdfEncrypt->setChecked(false);
        } 
        pdfPasswdIsSet=true;
    }
    
    gsMerge(PdfExistingFileName,tmpDir+QStringLiteral(".kEasySkan.pdf")); //merge existing pdf with tmp pdf
    
    if (gsMergeOk==false) {
        KMessageBox::error (0,PdfExistingFileName+i18n(" was not upadated!"));
        return;
    }
    else {
        if (m_settingsUi.pdfEncrypt->isChecked()) {
            gsPasswd(PdfExistingFileName);
            if (!gsPasswdOk) return;
        }
        KMessageBox::information (0,PdfExistingFileName+i18n("  has been successfully updated." ));
    }
    
    if (openAfterFinish==true) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(PdfExistingFileName));
    }
    
    gsPasswdOk=false;
}



void kEasySkan::SaveToSinglePdf()
{
    
    QDialog *dialogSingle = new QDialog(this, Qt::Dialog);
    QDialogButtonBox *buttonsSingle = new QDialogButtonBox(QDialogButtonBox::Yes);
    bool lastPage;
    
    if (firstPage==true) {
        SinglePdfFileUrl = QFileDialog::getSaveFileUrl(this, i18n("Save PDF..."), pdfUrl, i18n("PDF Files (*.pdf *.PDF *.Pdf)"),0,QFileDialog::DontConfirmOverwrite);
        
        if (QFile::exists(SinglePdfFileUrl.toLocalFile()) && SinglePdfFileUrl.isLocalFile()) {
            SinglePdfFileName=SinglePdfFileUrl.toLocalFile();
            int i = KMessageBox::warningContinueCancel(0,i18n("You've chosen an existing PDF file. Each new scan (from now on) will be appended to it\nOriginal file will be backed up."));
            if (i == 5) { // continue
                isExisting=true;
                firstPageCreated=true;
                QFile::copy(SinglePdfFileName, SinglePdfFileName + (QStringLiteral(".orig")));
                return;
            }
            else   { // cancel 
                firstPageCreated=false;
                return;
            }   
        } 
        if (SinglePdfFileUrl.isEmpty()==true) {
            KMessageBox::error(0,i18n("No filename given!"));
            firstPageCreated=false;
            return;
        }
        
        QString last4=(SinglePdfFileUrl.toString()).right(4);
        if ((last4.contains(QStringLiteral(".pdf"),Qt::CaseInsensitive))==false) {
            SinglePdfFileName=SinglePdfFileUrl.toString();
            SinglePdfFileName.append(QStringLiteral(".pdf"));
            SinglePdfFileUrl=QUrl(SinglePdfFileName);
        }
        
        if (SinglePdfFileUrl.isLocalFile()) {
            SinglePdfFileName=SinglePdfFileUrl.toLocalFile();
            pdfWriter(SinglePdfFileName,true); //writes the fisrt page 
            
            if (pdfWriterSuccess==true) {
                KMessageBox::information (0,SinglePdfFileName+i18n("  has been successfully created. Each new scan will be appended to it.")); 
                firstPageCreated=true;
                return;
            }     
            else {
                firstPageCreated=false;
                return;
            }
        }
        else { //remote file
            counter+=1;
            pdfWriter(tmpDir+SinglePdfFileUrl.fileName(),true);
            if (pdfWriterSuccess) {
                KMessageBox::information (0,i18n("1st page was successfully created. Each new scan will be appended to it.")); 
                firstPageCreated=true;
                return;
            }
            else {
                return;
            }
        }
    } // fisrt page created
    
    
    if (SinglePdfFileUrl.isLocalFile()) {
        pdfWriter(tmpDir+QStringLiteral(".kEasySkan.pdf"),false); //writes the next page into temp file
        gsMerge(SinglePdfFileName,tmpDir+QStringLiteral(".kEasySkan.pdf")); // append next pdf page 
    }
    else { //remote file
        SinglePdfFileName=SinglePdfFileUrl.fileName();
        counter+=1;
        pdfWriter(tmpDir+pdfNamePrefix+numberToString(counter,4),false);
        gsMerge(tmpDir+SinglePdfFileUrl.fileName(),tmpDir+pdfNamePrefix+numberToString(counter,4));
    }
    
    if (gsMergeOk==false) {
        KMessageBox::error (0,SinglePdfFileName+i18n(" was not upadated!"));
        return;
    }
    else {
        KMessageBox::createKMessageBox(dialogSingle,
                                       buttonsSingle,QMessageBox::Information,SinglePdfFileName+i18n("  has been successfully updated." ),
                                       QStringList(),
                                       i18n("This was the last page."),
                                       &lastPage,
                                       KMessageBox::Notify);
        if (lastPage==true) {
            if (m_settingsUi.pdfEncrypt->isChecked() && !pdfPasswdIsSet) {
            pdfPasswd=getPasswd();
            if (pdfPasswd.isEmpty()) {
                KMessageBox::information (0,i18n("No password was given! PDF encryption will be disabled."));
                m_settingsUi.pdfEncrypt->setChecked(false);
            } 
            pdfPasswdIsSet=true;
        }
        
        if (m_settingsUi.pdfEncrypt->isChecked()) {
            
            if (SinglePdfFileUrl.isLocalFile()) {
                gsPasswd(SinglePdfFileName);
                if (!gsPasswdOk) return;    
            }
            else {
                gsPasswd(tmpDir+SinglePdfFileUrl.fileName());
                if (!gsPasswdOk) return;    
            }
        }

            KMessageBox::information(0,SinglePdfFileName+i18n("  is now ready!"));
            firstPage=true; // so we can create a new PDF file
            firstPageCreated=false; // so we can create a new PDF file
            QFile::remove(SinglePdfFileName + QStringLiteral(".backup"));
        }
    }
    
    if (!SinglePdfFileUrl.isLocalFile() && lastPage) {
        
        docUploader(SinglePdfFileUrl);
    }
    QFile::remove(tmpDir+QStringLiteral(".kEasySkan.pdf"));
}


void kEasySkan::OpenWithGimp() // works only with local files
{
    
    KMessageBox::information(0,i18n("kEasySkan needs to save the image locally before you can edit it with Gimp."));
    QString suggestedFileName ;
    QDateTime dateTime = dateTime.currentDateTime();
    
    suggestedFileName=imgDir+imageNamePrefix; 
    if (!targetImgDirLocal) suggestedFileName=QDir::homePath()+QStringLiteral("/")+imageNamePrefix;
    
    if (m_settingsUi.dateTimeAppend->isChecked()==true) {
        suggestedFileName.append(QStringLiteral("-")+dateTime.toString(QStringLiteral("yyyy.MM.dd-hh.mm.ss")));
        suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
    }
    
    if (m_settingsUi.numberAppend->isChecked()==true) {
        suggestedFileName.append(QStringLiteral("-")+numberToString(fileNumber,4));
        suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
    }
    
    if (m_settingsUi.autoNumberAppend->isChecked()==true) {
        suggestedFileName.append(QStringLiteral("-"));
        QString strNm=numberToString(autoNumber(suggestedFileName),4);
        suggestedFileName.append(strNm);
        suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
    }
    
    QString gimpFileName = QFileDialog::getSaveFileName(this, i18n("Save Image File"),
                                                        suggestedFileName,
                                                        (i18n("Image Files (*.")+imageFormatAsString+QStringLiteral(")\""))
    ); 
    
    if (gimpFileName.isEmpty()) {
        KMessageBox::error(0,i18n("No filename given!"));
        return;
    }
    
    ImageWriter(gimpFileName,imageFormat,imageQuality);
    
    if (writeOk==false) {
        
        KMessageBox::sorry(0,i18n("Image couldn't be saved."));
        return;
    }
    if (m_settingsUi.numberAppend->isChecked()==true) {
        fileNumber+=1;
        m_settingsUi.fileNumber->setValue(fileNumber);
        saveSettings();
    }
    
    KMessageBox::information(0,(QString(gimpFileName)).append(i18n("  has been saved successfully. Gimp is now ready...") ));
    
    gimpFileName.prepend(QStringLiteral("\"")); // this is a workaround in case filename contains spaces
    gimpFileName.append(QStringLiteral("\"")); // same here...
    
    QProcess *RunGimp = new QProcess(); // program will stay open even if user closes kEasySkan
    gimpFileName.prepend(QStringLiteral("gimp "));
    RunGimp->start(gimpFileName);
    
}

void kEasySkan::OpenWithDefault()
{
    ImageWriter(tmpDir+QStringLiteral(".kEasySkan.")+imageFormatAsString,imageFormat,imageQuality);
    if (writeOk==false) {return;} 
    
    QDesktopServices::openUrl(QUrl::fromLocalFile(tmpDir+QStringLiteral(".kEasySkan.")+imageFormatAsString));
}



void kEasySkan::OpenWithOther()
{
    ImageWriter(tmpDir+QStringLiteral(".kEasySkan.")+imageFormatAsString,imageFormat,imageQuality);
    
    if (writeOk==false) {return;} 
    
    QList<QUrl> fileList;
    QUrl fileUrl;
    fileUrl.setPath(tmpDir+QStringLiteral(".kEasySkan.")+imageFormatAsString);
    fileList.append(fileUrl);
    KRun::displayOpenWithDialog(fileList, m_showImgDialog, false);
}



void kEasySkan::sendToClipboard()
{
    QApplication::clipboard() -> setImage(mImage, QClipboard::Clipboard);
    
    KMessageBox::information(0,i18n("Image copied to clipboard ! \nIt will remain available until you exit the main program"));
}


void kEasySkan::mailTo()
{
    QUrl mailUrl;
    QString mmailFname;
        
    QDialog *mailDialog = new QDialog(m_showImgDialog);
    mailDialog->resize(400, 300);
    mailDialog->setWindowTitle(QStringLiteral("mail to ..."));
    
    QGridLayout *mailLayout = new QGridLayout(mailDialog);
    
    QLabel *label = new QLabel(mailDialog);
    label->setText(i18n("Recipient:"));
    mailLayout->addWidget(label);
    QLineEdit *mailAddress = new QLineEdit(mailDialog);
    mailAddress->setText(mmailAddress);
    mailLayout->addWidget(mailAddress);
    
    QLabel *label_2 = new QLabel(mailDialog);
    label_2->setText(i18n("Subject:"));
    mailLayout->addWidget(label_2);
    QLineEdit *mailSubject = new QLineEdit(mailDialog);
    mailSubject->setText(mmailSubject);
    mailLayout->addWidget(mailSubject);
    
    QLabel *label_3 = new QLabel(mailDialog);
    label_3->setText(i18n("Message Text:"));
    mailLayout->addWidget(label_3);
    QPlainTextEdit *mailBody = new QPlainTextEdit(mailDialog);
    mailBody->setPlainText(mmailBody);
    mailLayout->addWidget(mailBody);
    
    QLabel *label_4 = new QLabel(mailDialog);
    label_4->setText(i18n("File description:"));
    mailLayout->addWidget(label_4);
    QLineEdit *mailFname = new QLineEdit(mailDialog);
    mailLayout->addWidget(mailFname);
    
    QLabel *label_5 = new QLabel(mailDialog);
    label_5->setText(i18n("E-Mail Client:"));
    mailLayout->addWidget(label_5);
    QComboBox *mailClient = new QComboBox(mailDialog);
    mailClient->addItem(QStringLiteral("kmail"));
    mailClient->addItem(QStringLiteral("thunderbird"));
    mailClient->addItem(QStringLiteral("evolution"));
    mailClient->addItem(QStringLiteral("seamonkey"));
    mailClient->setCurrentText(mmailClient);
    mailLayout->addWidget(mailClient);
    
    QCheckBox *mailPdf = new QCheckBox(mailDialog);
    mailPdf->setText(i18n("Send as PDF"));
    mailLayout->addWidget(mailPdf);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(mailDialog);
    buttonBox->setOrientation(Qt::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    mailLayout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::rejected, mailDialog, &QDialog::close);
    connect(buttonBox, &QDialogButtonBox::accepted, mailDialog, &QDialog::accept);
    
    if (mailDialog->exec()) {
        mmailAddress=mailAddress->text();
        mmailSubject=mailSubject->text();
        mmailBody=mailBody->toPlainText();
        mmailFname=mailFname->text();
        mmailClient=mailClient->currentText();
    }
    else {
        return;
    }
    
    if  ( (QStandardPaths::findExecutable(mmailClient)).isEmpty() ) {
        KMessageBox::error(mailDialog,mmailClient+i18n(" not found!"));
        return;
    }
    
    if (mmailFname.isEmpty()) {
        mmailFname=tmpDir+QStringLiteral(".kEasySkan.");
        
    }
    else {
        mmailFname=tmpDir+mmailFname+QStringLiteral(".");
    }
    
    if (mailPdf->isChecked()) {
        mmailFname.append(QStringLiteral("pdf"));
        pdfWriter(mmailFname,true);
        if (pdfWriterSuccess==false) {return;}
    }
    else {
        mmailFname.append(imageFormatAsString);
        ImageWriter(mmailFname,imageFormat,imageQuality);
        if (writeOk==false) {return;} 
    }
    
    QProcess *mailSend = new QProcess();
    QString mailFinalCommand;
    QString mailClientOptions;
    
    if (mmailClient==QStringLiteral("kmail")) {
        
        mailClientOptions.append(QStringLiteral(" -s \""));
        mailClientOptions.append(mmailSubject);
        mailClientOptions.append(QStringLiteral("\" --body \""));
        mailClientOptions.append(mmailBody);
        mailClientOptions.append(QStringLiteral("\" --attach \""));
        mailClientOptions.append(mmailFname);
        mailClientOptions.append(QStringLiteral("\" "));
        mailClientOptions.append(mmailAddress);
        mailFinalCommand=mmailClient+mailClientOptions;
    }
    
    if (mmailClient==QStringLiteral("evolution")) {
        mailClientOptions.append(QStringLiteral(" mailto:"));
        mailClientOptions.append(mmailAddress);
        mailClientOptions.append(QStringLiteral("?subject=\""));
        mailClientOptions.append(mmailSubject);
        mailClientOptions.append(QStringLiteral("\"\&body=\""));
        mailClientOptions.append(mmailBody);
        mailClientOptions.append(QStringLiteral("\"\&attach=\""));
        mailClientOptions.append(mmailFname);
        mailClientOptions.append(QStringLiteral("\"\""));
        mailFinalCommand=mmailClient+mailClientOptions;
    }
    
    if ( (mmailClient==QStringLiteral("thunderbird")) | (mmailClient==QStringLiteral("seamonkey")) ) {
        mailClientOptions.append(QStringLiteral(" -compose \"to="));
        mailClientOptions.append(mmailAddress);
        mailClientOptions.append(QStringLiteral(",subject=\""));
        mailClientOptions.append(mmailSubject);
        mailClientOptions.append(QStringLiteral("\",body=\""));
        mailClientOptions.append(mmailBody);
        mailClientOptions.append(QStringLiteral("\",attachment=\""));
        mailClientOptions.append(mmailFname);
        mailClientOptions.append(QStringLiteral("\"\""));
        mailFinalCommand=mmailClient+mailClientOptions;
    }
    
    mailSend->start(mailFinalCommand);
}


void kEasySkan::ImageWriter(const QString fName, const QByteArray fFormat, int fQuality)
{
    
    QImageWriter *writer = new QImageWriter();
    writer -> setFormat(fFormat);
    writer -> setFileName (fName);
    writer -> setQuality (fQuality); //does nothing if format doesn't support it
    writer -> write(mImage); // will overwrite any existing file with the same name
    writer -> error();
    if (writer->canWrite()==false) 
    {
        KMessageBox::error(0,i18n("Ooops! Something went wrong. \nPlease review your settings and try again.\nIf the problem persists, restart the application.\n\
        Possible reasons for this are:\nInvalid filename.\nUnsupported format\nFull disk.\nNo write permission.\nScanner didn't produce valid output.\
        \n\nError reported: ")+writer->errorString());
        writeOk=false;
        return;
    }
    writeOk=true;
    
}



void kEasySkan::pdfWriter(const QString fName, bool writeDocName)
{
    lbl->setText(i18n("Creating PDF file..."));
    
    QObject::connect (this,SIGNAL(processStarted()),msgDlg,SLOT(show()));
    QObject::connect (this,SIGNAL(processFinished()),msgDlg,SLOT(close()));
    
    Q_EMIT processStarted();
    msgDlg->repaint();
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents,1000); 
    
    QFileInfo fInfo(fName);
    QString path=fInfo.absolutePath();
    QString bName=fInfo.completeBaseName();
    
    pdfWriterSuccess=false;
    if (QFile::exists(fName)) {QFile::remove(fName);}
    
    QSize size = mImage.size();
    qreal x = qreal(1000) * qreal(size.width()) / qreal(mImage.dotsPerMeterX()) ;
    qreal y = qreal(1000) * qreal(size.height()) / qreal(mImage.dotsPerMeterY()) ;
    QPrinter *printToPdf = new QPrinter(QPrinter::HighResolution);
    QPageSize pSize = QPageSize(QSizeF(x,y),QPageSize::Millimeter,QString(),QPageSize::FuzzyMatch);
    
    printToPdf->setOutputFormat(QPrinter::PdfFormat);
    printToPdf->setPageSize(pSize);
    printToPdf->setOutputFileName(fName); 
    
    if (writeDocName==true) { printToPdf->setDocName(bName+QLatin1String(" created by kEasySkan!")); }
    QPainter painter(printToPdf);
    QRect rect = painter.viewport();
    
    size.scale(rect.size(), Qt::KeepAspectRatio);
    painter.setViewport(rect.x(), rect.y(),size.width(), size.height());
    painter.setWindow(mImage.rect());
    painter.drawImage(0, 0, mImage);
    
    Q_EMIT processFinished();
    
    if (QFile::exists(fName)) {
        
        pdfWriterSuccess=true ;
    }
    else {
        
        QString strError;
        if (QFile::exists(path)==false) {strError=QString(path+i18n(" : Directory not found or invalid file name"));}
        if (fInfo.isWritable()==false && QFile::exists(path)==true) {strError=QString(path+i18n(" : Directory not writable"));}
        if (fInfo.isWritable()==true && QFile::exists(path)==true) {strError=QString(i18n(" Scanner didn't produce a valid output."));}
        KMessageBox::error(0,i18n("PDF operation failed.\nPlease check your settings and try again.\nIf the proplem persists, restart kEasySkan.\nError : ")+strError);
        
        return;
    }    
}



bool   kEasySkan::gsMerge(const QString fName1, const QString fName2)
{ 
    lbl->setText(i18n("Merging PDF files..."));

    gsMergeOk=false;
    QString Command;
    QString CommandOptions;
    QString fNameBackup = fName1 + (QStringLiteral(".backup"))  ;
    
    if (QFile::exists(fNameBackup)) {QFile::remove(fNameBackup);}
    
    QFile::copy (fName1,fNameBackup);
    
    Command=QStringLiteral("gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -dPDFSETTINGS=/prepress -sOutputFile=");
    CommandOptions.append(QStringLiteral("\""));
    CommandOptions.append(fName1);
    CommandOptions.append(QStringLiteral("\""));
    CommandOptions.append(QStringLiteral(" "));
    CommandOptions.append(QStringLiteral("\""));
    CommandOptions.append(fNameBackup);
    CommandOptions.append(QStringLiteral("\" \""));
    CommandOptions.append(fName2);
    CommandOptions.append(QStringLiteral("\""));

//     KMessageBox::information(0,Command+CommandOptions);
    
    QProcess *gs = new QProcess();
    connect(gs,SIGNAL(started()),msgDlg,SLOT(exec()));
    connect(gs,SIGNAL(finished(int,QProcess::ExitStatus)),msgDlg,SLOT(close()));
    
    gs->start(Command + CommandOptions);
    gs->waitForFinished(500);
    int j = gs->exitCode();
    
    QString err = QString::fromStdString( ((gs->readAllStandardError())).toStdString() );
    QString out = QString::fromStdString( ((gs->readAllStandardOutput())).toStdString() );
    
    if (j==0) {
        QFile::remove(fName2);
        return gsMergeOk=true;
    }
    else {
        KMessageBox::error (0,i18n("PDF operation failed.\nGhostscript output:\n") +out+err);
        QFile::remove (fName1);
        QFile::rename (fNameBackup,fName1);
        return gsMergeOk=false;
    }
    
}


void kEasySkan::gsPasswd(const QString fName1)

{ 
    gsPasswdOk=false;
    lbl->setText(i18n("Encrypting PDF file..."));

    QString Command;
    QString CommandOptions;
    QString fNameBackup = fName1 + (QStringLiteral(".backup"))  ;
    
    if (QFile::exists(fNameBackup)) {QFile::remove(fNameBackup);}
    
    QFile::copy (fName1,fNameBackup);
 
    Command=QStringLiteral("gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -dPDFSETTINGS=/prepress -sOwnerPassword=");
    
    CommandOptions.append(QStringLiteral("\""));
    CommandOptions.append(pdfPasswd);
    CommandOptions.append(QStringLiteral("\" "));
    
    CommandOptions.append(QStringLiteral("-sUserPassword="));
    CommandOptions.append(QStringLiteral("\""));
    CommandOptions.append(pdfPasswd);
    CommandOptions.append(QStringLiteral("\" "));
    
    CommandOptions.append(QStringLiteral("-sOutputFile="));
    CommandOptions.append(QStringLiteral("\""));
    CommandOptions.append(fName1);
    CommandOptions.append(QStringLiteral("\" \""));
    CommandOptions.append(fNameBackup);
    CommandOptions.append(QStringLiteral("\""));

    QProcess *gs = new QProcess();
   
    connect(gs,SIGNAL(started()),msgDlg,SLOT(exec()));
    connect(gs,SIGNAL(finished(int,QProcess::ExitStatus)),msgDlg,SLOT(close()));
   
    gs->start(Command+CommandOptions);
    gs->waitForFinished(500);
    int j = gs->exitCode();
    
    QString err = QString::fromStdString( ((gs->readAllStandardError())).toStdString() );
    QString out = QString::fromStdString( ((gs->readAllStandardOutput())).toStdString() );
    
    if (j==0) {
        QFile::remove(fNameBackup);
        gsPasswdOk=true;
    }
    else {
        KMessageBox::error (0,i18n("PDF operation failed.\nGhostscript output:\n") +out+err);
        QFile::remove (fName1);
        QFile::rename (fNameBackup,fName1);
        gsPasswdOk=false;
    }
    
}


    


int kEasySkan::autoNumber (const QString fNamePrefix)
{
    int aNumber;
    
    QFileInfo fInfo (fNamePrefix);
    bool ok;
    QString  bName = fInfo.completeBaseName();
    QDir *mdir = new QDir();
    QStringList fFilters;
    QStringList matchingFiles;
    
    mdir->setPath(fInfo.absolutePath());
    mdir->setNameFilters(fFilters); // use empty filter
    matchingFiles=mdir->QDir::entryList(); // to list all files
    
    matchingFiles=matchingFiles.filter(bName); // selects only the filenames witch contain the same flnm prefix
    
    if (matchingFiles.isEmpty()==true) { // returns 1 if no matching filenames found
        aNumber=1;
        return aNumber;
    }
    
    for (int i=0 ; i<= matchingFiles.size() -1 ; i=i+1) {
        QFileInfo mF(matchingFiles[i]);
        matchingFiles[i]=mF.completeBaseName(); //remove suffixes
        matchingFiles[i].remove(0,bName.size()); //remove prefix "-" included
        int j=matchingFiles[i].toInt(&ok,10); // check that only number remains
        if (ok==false || j<0) {matchingFiles[i]=QString();}
    }
    
    matchingFiles.removeDuplicates();
    
    QList<int> list;
    foreach (QString s , matchingFiles){
        list << s.toInt();
    }
    qSort(list.begin(),list.end(),qGreater<int>()); 
    
    aNumber=list[0]+1  ; //fist entry -> integer +1 is what we were looking for
    return aNumber;
}  


QString kEasySkan::numberToString (int i, int length)
{
    std::stringstream stdStream;
    stdStream << std::setw(length) << std::setfill('0') << i;
    return QString::fromStdString(stdStream.str());
    
}

void kEasySkan::docUploader (const QUrl fName)
{
    QObject::connect (this,SIGNAL(processStarted()),msgDlg,SLOT(show()));
    QObject::connect (this,SIGNAL(processFinished()),msgDlg,SLOT(close()));
    
    lbl->setText(i18n("Uploading file to remote host..."));
    
    Q_EMIT processStarted();
    
    msgDlg->repaint();
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents,1000); 
    
    uploadFileOk=false;
    QString localName = fName.fileName();
    QFile tmpFile(tmpDir+localName);
    tmpFile.open(QIODevice::ReadOnly);
    auto uploadJob = KIO::storedPut(&tmpFile, fName, -1);
    KJobWidgets::setWindow(uploadJob, QApplication::activeWindow());
    uploadFileOk = uploadJob->exec();
    int j = uploadJob->error();
    tmpFile.close();
    
    if (uploadFileOk==false) {
        KMessageBox::sorry(0, i18n("Failed to upload image. Error code: ")+QString::number(j));
    }
    else
    {
        KMessageBox::information(0,localName+i18n("  successfully uploaded to remote host."));
        tmpFile.remove();
    }
    Q_EMIT processFinished ();
}

QString kEasySkan::getPasswd()
{
    QString result;
    KNewPasswordDialog *dlg = new KNewPasswordDialog();
    dlg->setPrompt( i18n( "Enter password for PDF encryption" ) );
    if (dlg->exec()) {
        result=dlg->password();
    }
    else {
        result=QString();
    }
    return result;
}
