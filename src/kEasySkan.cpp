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
#include <KDELibs4Support/kio/netaccess.h>
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
    QPushButton *btnAbout = dlgButtonBoxBottom->addButton(i18n("About"), QDialogButtonBox::ButtonRole::ActionRole);
    QPushButton *btnSettings = dlgButtonBoxBottom->addButton(i18n("Settings"), QDialogButtonBox::ButtonRole::ActionRole);
    btnSettings->setIcon(settingsIcon);
    
    m_ksanew = new KSaneIface::KSaneWidget(this);
    connect(m_ksanew, &KSaneWidget::imageReady, this, &kEasySkan::imageReady);
    connect(m_ksanew, &KSaneWidget::availableDevices, this, &kEasySkan::availableDevices);
    connect(m_ksanew, &KSaneWidget::userMessage, this, &kEasySkan::alertUser);
    connect(m_ksanew, &KSaneWidget::buttonPressed, this, &kEasySkan::buttonPressed);
    
    lbl2->setMaximumHeight(20);
    lbl2->setText(i18n("kEasySkan is ready."));
    lbl2->show();
    
    QFrame *mDivider = new QFrame(this);
    mDivider->setFrameShape(QFrame::HLine);
    mDivider->setLineWidth(6);
    
    mainLayout->addWidget(m_ksanew);
    
    mainLayout->addWidget(dlgButtonBoxBottom);
    mainLayout->addWidget(mDivider);
    mainLayout->addWidget(lbl2);
    
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
        
        connect(m_settingsUi.revertOptions,&QPushButton::clicked, this, &kEasySkan::defaultScannerOptions);
        
        m_settingsDialog->setMinimumSize(550,680);
        
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
        
        lbl->show();
        lbl->setMaximumHeight(20);
        lbl->setText(i18n("kEasySkan is ready."));
        
        mainLayout->addWidget(mDivider);
        mainLayout->addWidget(lbl);
        
        // connect buttons and actions to functions
        
        connect(MoreActionsAction1, &QAction::triggered, this, &kEasySkan::openWithDefault);
        connect(MoreActionsAction2, &QAction::triggered, this, &kEasySkan::openWithGimp);
        connect(MoreActionsAction3, &QAction::triggered, this, &kEasySkan::sendToClipboard);
        connect(MoreActionsAction4, &QAction::triggered, this, &kEasySkan::showSettingsDialog);
        connect(MoreActionsAction5, &QAction::triggered, this, &kEasySkan::mailTo);
        connect(MoreActionsAction6, &QAction::triggered, this, &kEasySkan::openWithOther);
        connect(PdfAction1, &QAction::triggered, this, &kEasySkan::createPdf);
        connect(PdfAction2, &QAction::triggered, this, &kEasySkan::appendToPdf);
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
    
    m_settingsUi.noneAppend->setChecked(imgSaving.readEntry("No Identifier", false));
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
    m_settingsUi.pdfQuality->setCurrentText(imgSaving.readEntry("PDF Quality",QStringLiteral("medium")));
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
        imgDir=imgUrl.path();
        if (!QFile::exists(imgDir)) {
            if (KMessageBox::questionYesNo(0,i18n("The directory ")+imgDir+i18n(" doesn't exist. Do you wish to create it?"))==3) {
                QDir createDir;
                if (createDir.mkpath(imgDir)==false) {
                    KMessageBox::error(0,imgDir+i18n("  couldn't be created. Check your pemissions."));
                }
            }
        }
    }
    else {
         if (!KIO::NetAccess::exists (pdfUrl,KIO::NetAccess::DestinationSide,nullptr)) {
             KMessageBox::error(0,i18n("Remote host for saving images is unreachable."));
             remoteAccess=false;
         }
         else {
             remoteAccess=true;
        }
    }
    
    //todo create remote dir
    
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
        pdfDir=pdfUrl.path();
        if (!QFile::exists(pdfDir)) {
            if (KMessageBox::questionYesNo(0,i18n("The directory ")+pdfDir+i18n(" doesn't exist. Do you wish to create it?"))==3) {
                QDir createDir;
                if (createDir.mkpath(pdfDir)==false) {
                    KMessageBox::error(0,pdfDir+i18n("  couldn't be created. Check your pemissions."));
                }    
            }
        }
    } 
    else {
         if (!KIO::NetAccess::exists (pdfUrl,KIO::NetAccess::DestinationSide,nullptr)) {
             KMessageBox::error(0,i18n("Remote host for saving PDF files is unreachable."));
             remoteAccess=false;
         }
         else {
             remoteAccess=true;
        }
    }
    //todo create remote dir
    
    pdfNamePrefix=m_settingsUi.pdfPrefix->text();
    
    if (m_settingsUi.pdfQuality->currentIndex()==0) pdfQuality=QStringLiteral("/screen");
    if (m_settingsUi.pdfQuality->currentIndex()==1) pdfQuality=QStringLiteral("/ebook");
    if (m_settingsUi.pdfQuality->currentIndex()==2) pdfQuality=QStringLiteral("");
    if (m_settingsUi.pdfQuality->currentIndex()==3) pdfQuality=QStringLiteral("/printer");
    if (m_settingsUi.pdfQuality->currentIndex()==4) pdfQuality=QStringLiteral("/prepress");
    
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
    imgSaving.writeEntry("No Identifier", m_settingsUi.noneAppend->isChecked());
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
    QString suggestedFileName,targetFileName ;
    QUrl suggestedFileUrl;
    QDateTime dateTime = dateTime.currentDateTime();
    QUrl targetFileUrl;
    
    // Standard Mode
    if (m_settingsUi.saveMode->currentIndex()==standardMode) {
        
        suggestedFileName.append(imgUrl.toString()+imageNamePrefix);
        
        if (m_settingsUi.noneAppend->isChecked()) {
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
        }
        
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
            QString strNm=numberToString(autoNumber(QUrl(suggestedFileName)),4);
            if (aNumber==-1) return;
            suggestedFileName.append(strNm);
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
        }
        
        suggestedFileUrl=QUrl(suggestedFileName);
        
        QUrl flnm;
        QFileDialog *qfld = new QFileDialog() ;
        qfld->setAcceptMode(QFileDialog::AcceptSave);
        qfld->setOptions(QFileDialog::DontConfirmOverwrite);
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
        
        if ((suffix.toUtf8())!=imageFormat) {
            KMessageBox::information(0,i18n("It looks like you are trying to save in a different format than the one in settings. \nIf this fails, check your settings to see the supported formats."));
        }
        
        imageWriter(flnm,imageFormat,imageQuality);
        if (writeOk) {
            if (m_settingsUi.numberAppend->isChecked()==true) {
                fileNumber+=1;
                m_settingsUi.fileNumber->setValue(fileNumber);
                saveSettings();
            }
        }
        return;
    }
    
    //check for identifier
    
    if (m_settingsUi.noneAppend->isChecked()) {
        KMessageBox::information(0,i18n("Fast saving modes need an identifier to work. Please select to append either Date and Time or Number."));
        showSettingsDialog();    
    }
    
    // fast save to image    
    if (m_settingsUi.saveMode->currentIndex()==fastImg) {
        if (!imgUrl.isLocalFile() && !remoteAccess) {
            KMessageBox::sorry(0,i18n("Remote host is unreachable.\nCheck your settings."));
            return;
        }
        suggestedFileName.clear();
        
        // append date and time
        if (m_settingsUi.dateTimeAppend->isChecked()==true) {
            suggestedFileName.append(imgUrl.toString()+imageNamePrefix);
            suggestedFileName.append(QStringLiteral("-")+dateTime.toString(QStringLiteral("yyyy.MM.dd-hh.mm.ss")));    
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
            suggestedFileUrl=QUrl(suggestedFileName);
            imageWriter(suggestedFileUrl,imageFormat,imageQuality);
            if (writeOk==true && suggestedFileUrl.isLocalFile()) {
                KMessageBox::information(0,suggestedFileName+i18n("  successfully saved."));
                return;
            }
        }
        
        // append auto numbering
        if (m_settingsUi.autoNumberAppend->isChecked()==true) {
            suggestedFileName.append(imgUrl.toString()+imageNamePrefix);
            suggestedFileName.append(QStringLiteral("-"));
            QString strNm = numberToString(autoNumber(QUrl(suggestedFileName)),4);
            if (aNumber==-1) return;
            suggestedFileName.append(strNm);
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
            suggestedFileUrl=QUrl(suggestedFileName);
            imageWriter(suggestedFileUrl,imageFormat,imageQuality);
            if (writeOk==true  && suggestedFileUrl.isLocalFile()) {
                KMessageBox::information(0,suggestedFileName+i18n("  successfully saved."));
                return;
            }
        }
        
        // append manual numbering 5
        if (m_settingsUi.numberAppend->isChecked()==true) {
            
            suggestedFileName.append(imgUrl.toString()+imageNamePrefix);
            suggestedFileName.append(QStringLiteral("-")+numberToString(fileNumber,4));
            suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
            suggestedFileUrl=QUrl(suggestedFileName);
            imageWriter(suggestedFileUrl,imageFormat,imageQuality);
            if (writeOk==true)  {
                if (suggestedFileUrl.isLocalFile()) KMessageBox::information(0,suggestedFileName+i18n("  successfully saved."));
                fileNumber+=1;
                m_settingsUi.fileNumber->setValue(fileNumber);
                saveSettings();   
                return;
            }
        }
    }
    
    
    // fast save to pdf
    
    if (m_settingsUi.saveMode->currentIndex()==fastPdf) {
        if (!pdfUrl.isLocalFile() && !remoteAccess) {
            KMessageBox::sorry(0,i18n("Remote host is unreachable.\nCheck your settings."));
            return;
        }
        
        suggestedFileName.clear();
        
        if (m_settingsUi.pdfEncrypt->isChecked() && !pdfPasswdIsSet) {
            pdfPasswd=getPasswd();
            if (pdfPasswd.isEmpty()) {
                KMessageBox::information (0,i18n("No password was given! PDF encryption will be disabled."));
                m_settingsUi.pdfEncrypt->setChecked(false);
            } 
            pdfPasswdIsSet=true;
        }
        
        suggestedFileName.append(pdfUrl.toString()+pdfNamePrefix);
        
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
            QString strNm=numberToString(autoNumber(QUrl(suggestedFileName)),4);
            if (aNumber==-1) return;
            suggestedFileName.append(strNm);
            suggestedFileName.append(QStringLiteral(".pdf"));
        }
        
        targetFileUrl=QUrl(suggestedFileName);
        
        if (targetFileUrl.isLocalFile()) {
            targetFileName=pdfUrl.path()+targetFileUrl.fileName();
        }
        else {
            targetFileName=tmpDir+targetFileUrl.fileName();
        }
        
        pdfWriter(targetFileName,targetFileUrl.fileName(),true);
        
        if (!pdfWriterSuccess)  return;
        
        if (m_settingsUi.pdfQuality->currentIndex()<4) {
            gsQuality(targetFileName);
            if (!gsQualityOk) return;
        }
        
        if (m_settingsUi.pdfEncrypt->isChecked() ) {
            gsEncrypt(targetFileName);
            if (!gsEncryptOk) return;
        }
        
        if (!targetFileUrl.isLocalFile()) {
            docUploader(targetFileName,targetFileUrl);
            if (!uploadFileOk) return;
            QFile::remove(targetFileName);
        }
        else {
            KMessageBox::information(0,targetFileName+i18n("  successfully saved."));
        }
        
        if (m_settingsUi.numberAppend->isChecked()==true) {
            fileNumber+=1;
            m_settingsUi.fileNumber->setValue(fileNumber);
            saveSettings();
        }
        
        lbl2->setText(i18n("kEasySkan is ready."));
    }
    
    // Fast save to single Pdf
    
    if (m_settingsUi.saveMode->currentIndex()==singlePdf) {
        if (firstPage==true) {
            singlePdfFileName=QString();
            singlePdfFileUrl=QUrl();
        }
        saveToSinglePdf();
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
    lbl->setText(i18n("Preparing to print. Please wait..."));
    lbl2->setText(i18n("Preparing to print. Please wait..."));
    qApp->processEvents();
    
    QPrinter *printer = new QPrinter(QPrinter::HighResolution);
    
    QPrintDialog printDialog(printer, this);
    
    lbl->clear();
    lbl2->clear();
    
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
    lbl->setText(i18n("kEasySkan is ready."));
}


void kEasySkan::createPdf()
{
    QString suggestedFileName,targetFileName ;
    QUrl suggestedFileUrl ;
    QDateTime dateTime = dateTime.currentDateTime();
    
    
    suggestedFileName.append(pdfUrl.toString()+pdfNamePrefix);
    
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
        QString strNm=numberToString(autoNumber(QUrl(suggestedFileName)),4);
        if (aNumber==-1) return;
        suggestedFileName.append(strNm);
        suggestedFileName.append(QStringLiteral(".pdf"));
    }
    
    suggestedFileUrl=QUrl(suggestedFileName);
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
    
    QString last4=(targetFileUrl.toString()).right(4);
    if ((last4.contains(QStringLiteral(".pdf"),Qt::CaseInsensitive))==false) {
        targetFileName=targetFileUrl.toString();
        targetFileName.append(QStringLiteral(".pdf"));
        targetFileUrl=QUrl(targetFileName);
    }
    
    if (m_settingsUi.pdfEncrypt->isChecked() && !pdfPasswdIsSet) {
        pdfPasswd=getPasswd();
        if (pdfPasswd.isEmpty()) return; 
        pdfPasswdIsSet=true;
    }
    
    if (targetFileUrl.isLocalFile()) {
        targetFileName=targetFileUrl.toLocalFile();
    }
    else {
        targetFileName=tmpDir+targetFileUrl.fileName();
    }
    pdfWriter(targetFileName,targetFileUrl.fileName(),false);
    if (!pdfWriterSuccess) return;
    
    if (m_settingsUi.pdfQuality->currentIndex()<4) {
        gsQuality(targetFileName);
        if (!gsQualityOk) return;
    }
    
    if (m_settingsUi.pdfEncrypt->isChecked()) {
        gsEncrypt(targetFileName);
        if (!gsEncryptOk) return;
    }
    
    if (!targetFileUrl.isLocalFile()) {
        docUploader(targetFileName,targetFileUrl);
        if (uploadFileOk) QFile::remove(targetFileName);
        else return;
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
                                   targetFileUrl.toString()+i18n("  was successfully created."),
                                   QStringList(),
                                   i18n("Open file."),
                                   &openAfterFinish,
                                   KMessageBox::Notify);
    
    if (openAfterFinish==true) {
        QDesktopServices::openUrl(targetFileUrl);
    }
    
    pdfPasswdIsSet=false;
    lbl->setText(i18n("kEasySkan is ready."));
}


void kEasySkan::appendToPdf()

{
    QUrl PdfExistingFileUrl;
    QString passwd;
    bool pdfEncryptChanged=false;
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
    
    if (PdfExistingFileUrl.isEmpty()) {
        PdfExistingFileUrl = QFileDialog::getOpenFileUrl(this, i18n("Open PDF..."), pdfUrl, i18n("PDF Files (*.pdf *.PDF *.Pdf)"));
    }
    else 
    {
        QFileDialog *qfld = new QFileDialog() ;
        qfld->setAcceptMode(QFileDialog::AcceptOpen);
        qfld->setOptions(QFileDialog::DontConfirmOverwrite);
        qfld->setDirectoryUrl(PdfExistingFileUrl.resolved(QUrl(QStringLiteral("./"))));
        qfld->selectUrl(PdfExistingFileUrl);
        if (qfld->exec()) {
            PdfExistingFileUrl=qfld->selectedUrls()[0];    
        }
    }
    
    if (PdfExistingFileUrl.isEmpty()) {
        KMessageBox::error(0,i18n("No filename given!"));
        return;
    }
    
    if (PdfExistingFileUrl.isLocalFile()) {
        PdfExistingFileName=PdfExistingFileUrl.toLocalFile();
    }
    else {
        PdfExistingFileName=tmpDir+PdfExistingFileUrl.fileName();
        KIO::NetAccess::download(PdfExistingFileUrl, PdfExistingFileName,nullptr);
        
    }
    
    QFile::copy(PdfExistingFileName,PdfExistingFileName+QStringLiteral(".orig"));
    
    gsCheckForPasswd(PdfExistingFileName,QString()); // check if selected file is protected
    if (gsIsProtected) {
        bool ok;
        passwd = QInputDialog::getText(this, i18n("Enter password"),
                                       PdfExistingFileName + i18n(" seems to be password protected.\nEnter password to continue. The resulting file will be encrypted using the existing password.")
                                       , QLineEdit::Password,
                                       QString(), &ok);
        if (!ok) return;
        gsCheckForPasswd(PdfExistingFileName,passwd); //check if passwd from user input is right
        if (gsPasswdIsCorrect) {
            pdfPasswd=passwd;
            pdfPasswdIsSet=true;
            if (!m_settingsUi.pdfEncrypt->isChecked()) {
                m_settingsUi.pdfEncrypt->setChecked(true);
                pdfEncryptChanged=true;
            }
        }
        else {
            KMessageBox::error(0,i18n("Wrong password!"));
            QFile::remove (PdfExistingFileName+QStringLiteral(".orig"));
            return;
        }
    }
    else {
        passwd=QString();
    }
    
    if (m_settingsUi.pdfEncrypt->isChecked() && !pdfPasswdIsSet) {
        pdfPasswd=getPasswd();
        if (pdfPasswd.isEmpty()) {
            KMessageBox::information (0,i18n("No password was given! PDF encryption will be disabled."));
            m_settingsUi.pdfEncrypt->setChecked(false);
        } 
        pdfPasswdIsSet=true;
    }
    
    pdfWriter(tmpDir+QStringLiteral(".kEasySkan.pdf"),PdfExistingFileUrl.fileName(),false); //writes tmp pdf
    
    if (pdfWriterSuccess==false) {
        QFile::remove (PdfExistingFileName+QStringLiteral(".orig"));
        return;
    }
    
    gsMerge(PdfExistingFileName,tmpDir+QStringLiteral(".kEasySkan.pdf"),passwd); //merge existing pdf with tmp pdf
    
    if (gsMergeOk==false) {
        KMessageBox::error (0,PdfExistingFileName+i18n(" was not upadated!"));
        QFile::remove(PdfExistingFileName);
        QFile::rename (PdfExistingFileName+QStringLiteral(".orig"),PdfExistingFileName);
        return;
    }
    else {
        if (m_settingsUi.pdfEncrypt->isChecked()) {
            gsEncrypt(PdfExistingFileName);
            if (!gsEncryptOk) return;
        }
        
        if (!PdfExistingFileUrl.isLocalFile()) {
            docUploader(PdfExistingFileName,PdfExistingFileUrl);
            if (!uploadFileOk) return;
            QString bu=PdfExistingFileUrl.toString()+QStringLiteral(".orig");
            QUrl backUpUrl=QUrl(bu);
            backUpUrl.resolved(QUrl(QStringLiteral("./")+PdfExistingFileUrl.fileName()+QStringLiteral(".orig")));
            docUploader(PdfExistingFileName+QStringLiteral(".orig"),backUpUrl);
            QFile::remove(PdfExistingFileName);
            QFile::remove(PdfExistingFileName+QStringLiteral(".orig"));
        }
        else 
        {
            KMessageBox::information (0,PdfExistingFileUrl.toString()+i18n("  has been successfully updated." ));
        }
        
        if (pdfEncryptChanged) m_settingsUi.pdfEncrypt->setChecked(false);
        saveSettings();
        pdfPasswdIsSet=false;
    }
    
    if (openAfterFinish==true) {
        QDesktopServices::openUrl(PdfExistingFileUrl);
    }
    lbl->setText(i18n("kEasySkan is ready."));
}



void kEasySkan::saveToSinglePdf()
{
    bool pdfEncryptChanged=false;
    QString passwd;
    QDialog *dialogSingle = new QDialog(this, Qt::Dialog);
    QDialogButtonBox *buttonsSingle = new QDialogButtonBox(QDialogButtonBox::Yes);
    bool lastPage;
    
    if (firstPage==true) {
        singlePdfFileExists=false;
        QFileDialog *qfld = new QFileDialog() ;
        qfld->setAcceptMode(QFileDialog::AcceptSave);
        qfld->setOptions(QFileDialog::DontConfirmOverwrite);
        qfld->setDirectoryUrl(pdfUrl);
        qfld->selectUrl(singlePdfFileUrl);
        if (qfld->exec()) {
            singlePdfFileUrl=qfld->selectedUrls()[0];    
        }
        
        if (QFile::exists(singlePdfFileUrl.toLocalFile()) && singlePdfFileUrl.isLocalFile()) {
            singlePdfFileName=singlePdfFileUrl.toLocalFile();
            singlePdfFileExists=true;
            
        }
        if (!singlePdfFileUrl.isLocalFile()) {
            if (KIO::NetAccess::exists (singlePdfFileUrl,KIO::NetAccess::DestinationSide,nullptr)) {
                singlePdfFileName=tmpDir+singlePdfFileUrl.fileName();
                singlePdfFileExists=true;
                KIO::NetAccess::download(singlePdfFileUrl, singlePdfFileName,nullptr);
            }
        }
        
        if (singlePdfFileExists)
        {
            QFile::copy(singlePdfFileName, singlePdfFileName + QStringLiteral(".orig"));
            int i = KMessageBox::warningContinueCancel(0,i18n("You've chosen an existing PDF file. Each new scan (from now on) will be appended to it\nOriginal file will be backed up."));
            if (i == 5) { // continue
                
                firstPageCreated=true;
                return;
            }
            else   { // cancel 
                firstPageCreated=false;
                return;
            }   
        }
        
        if (singlePdfFileUrl.isEmpty()==true) {
            KMessageBox::error(0,i18n("No filename given!"));
            firstPageCreated=false;
            return;
        }
        
        QString last4=(singlePdfFileUrl.toString()).right(4);
        if ((last4.contains(QStringLiteral(".pdf"),Qt::CaseInsensitive))==false) {
            singlePdfFileName=singlePdfFileUrl.toString();
            singlePdfFileName.append(QStringLiteral(".pdf"));
            singlePdfFileUrl=QUrl(singlePdfFileName);
        }
        
        if (singlePdfFileUrl.isLocalFile()) {
            singlePdfFileName=(singlePdfFileUrl.toString()).remove(0,7);
        }
        else {
            singlePdfFileName=tmpDir+singlePdfFileUrl.fileName();
        }
        
        pdfWriter(singlePdfFileName,singlePdfFileUrl.fileName(),true); //writes the fisrt page locally
        
        if (pdfWriterSuccess) {
            KMessageBox::information (0,i18n("1st page was successfully created. Each new scan will be appended to it.")); 
            firstPageCreated=true;
        }
        return;
    } // fisrt page created
    
    gsCheckForPasswd(singlePdfFileName,QString()); // check if selected file is protected
    if (gsIsProtected) {
        bool ok;
        passwd = QInputDialog::getText(this, i18n("Enter password"),
                                       singlePdfFileUrl.toString() + i18n(" seems to be password protected.\nEnter password to continue. The resulting file will be encrypted using the existing password.")
                                       , QLineEdit::Password,
                                       QString(), &ok);
        if (!ok) return;
        gsCheckForPasswd(singlePdfFileName,passwd); //check if passwd from user input is right
        if (gsPasswdIsCorrect) {
            pdfPasswd=passwd;
            pdfPasswdIsSet=true;
            if (!m_settingsUi.pdfEncrypt->isChecked()) {
                m_settingsUi.pdfEncrypt->setChecked(true);
                pdfEncryptChanged=true;
            }
        }
        else {
            KMessageBox::error(0,i18n("Wrong password!"));
            //             QFile::remove (singlePdfFileName+QStringLiteral(".orig"));
            return;
        }
    }
    else {
        passwd=QString();
    }
    
    pdfWriter(tmpDir+QStringLiteral(".kEasySkan.pdf"),singlePdfFileUrl.fileName(),false); //writes the next page into temp file
    gsMerge(singlePdfFileName,tmpDir+QStringLiteral(".kEasySkan.pdf"),passwd); // append next pdf page 
    
    
    if (gsMergeOk==false) {
        KMessageBox::error (0,singlePdfFileUrl.toString()+i18n(" was not upadated!"));
        return;
    }
    else {
        KMessageBox::createKMessageBox(dialogSingle,
                                       buttonsSingle,QMessageBox::Information,singlePdfFileUrl.toString()+i18n("  has been successfully updated." ),
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
                gsEncrypt(singlePdfFileName);
                if (!gsEncryptOk) return;    
            }
            
            KMessageBox::information(0,singlePdfFileUrl.fileName()+i18n("  is now ready!"));
            firstPage=true; // so we can create a new PDF file
            firstPageCreated=false; // so we can create a new PDF file
            pdfPasswdIsSet=false; // resets the password for new project file
            QFile::remove(singlePdfFileName + QStringLiteral(".backup"));
        }
    }
    
    if (pdfEncryptChanged) m_settingsUi.pdfEncrypt->setChecked(false);
    
    if (!singlePdfFileUrl.isLocalFile() && lastPage) {
        
        docUploader(singlePdfFileName,singlePdfFileUrl);
        if (uploadFileOk) QFile::remove(singlePdfFileName);
        if (singlePdfFileExists) {
            QUrl singlePdfFileUrlBu=QUrl(singlePdfFileUrl.toString()+QStringLiteral(".orig"));
            docUploader(singlePdfFileName+QStringLiteral(".orig"),singlePdfFileUrlBu);
            if (uploadFileOk) QFile::remove(singlePdfFileName+QStringLiteral(".orig"));
        }
        
    }
    QFile::remove(tmpDir+QStringLiteral(".kEasySkan.pdf"));
    lbl2->setText(i18n("kEasySkan is ready."));
}



void kEasySkan::openWithGimp() // works only with local files
{
    lbl->setText(i18n("Preparing for Gimp..."));
    KMessageBox::information(0,i18n("kEasySkan needs to save the image locally before you can edit it with Gimp."));
    QString suggestedFileName ;
    QDateTime dateTime = dateTime.currentDateTime();
    
    suggestedFileName=imgDir+imageNamePrefix; 
    if (!imgUrl.isLocalFile()) suggestedFileName=QDir::homePath()+QStringLiteral("/")+imageNamePrefix;
    
    if (m_settingsUi.noneAppend->isChecked()) {
        suggestedFileName.append(QStringLiteral(".")+imageFormatAsString);
    }
    
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
        QString strNm=numberToString(autoNumber(QUrl::fromLocalFile(suggestedFileName)),4);
        if (aNumber==-1) return;
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
    
    imageWriter(QUrl::fromLocalFile(gimpFileName),imageFormat,imageQuality);
    
    if (writeOk==false) {
        
        KMessageBox::sorry(0,i18n("Image couldn't be saved."));
        return;
    }
    if (m_settingsUi.numberAppend->isChecked()==true) {
        fileNumber+=1;
        m_settingsUi.fileNumber->setValue(fileNumber);
        saveSettings();
    }
    
    gimpFileName.prepend(QStringLiteral("\"")); // this is a workaround in case filename contains spaces
    gimpFileName.append(QStringLiteral("\"")); // same here...
    
    QProcess *RunGimp = new QProcess(); // program will stay open even if user closes kEasySkan
    gimpFileName.prepend(QStringLiteral("gimp "));
    RunGimp->start(gimpFileName);
    lbl->setText(i18n("kEasySkan is ready."));
    
}

void kEasySkan::openWithDefault()
{
    imageWriter(QUrl((tmpDir+QStringLiteral(".kEasySkan.")+imageFormatAsString)),imageFormat,imageQuality);
    if (writeOk==false) {return;} 
    QDesktopServices::openUrl(QUrl::fromLocalFile(tmpDir+QStringLiteral(".kEasySkan.")+imageFormatAsString));
}



void kEasySkan::openWithOther()
{
    imageWriter(QUrl(tmpDir+QStringLiteral(".kEasySkan.")+imageFormatAsString),imageFormat,imageQuality);    
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
    lbl->setText(i18n("Composing e-mail..."));
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
    label_4->setText(i18n("Filename:"));
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
        lbl->setText(i18n("kEasySkan is ready."));
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
        pdfWriter(mmailFname,mailFname->text(),false);
        if (pdfWriterSuccess==false) {return;}
    }
    else {
        mmailFname.append(imageFormatAsString);
        imageWriter(QUrl::fromLocalFile(mmailFname),imageFormat,imageQuality);
        if (writeOk==false) {return;} 
    }
    
    QProcess *mailSend = new QProcess();
    QString mailFinalCommand;
    QString mailClientOptions;
    
    if (mmailClient==QStringLiteral("kmail")) {
        mailFinalCommand=QStringLiteral("kmail -s MAILSUBJECT --body MAILBODY --attach MAILATTACH MAILADDRESS");
    }
    
    if (mmailClient==QStringLiteral("evolution")) {
        
        mailFinalCommand=QStringLiteral("evolution mailto:MAILADDRESS?subject=MAILSUBJECT&body=MAILBODY&attach=MAILATTACH");
    }
    
    if ( (mmailClient==QStringLiteral("thunderbird")) | (mmailClient==QStringLiteral("seamonkey")) ) {
        mailClientOptions=QStringLiteral(" -compose \"to=MAILADDRESS ,subject=MAILSUBJECT,body=MAILBODY,attachment=MAILATTACH\"");
        mailFinalCommand=mmailClient+mailClientOptions;
    }
    
    mailFinalCommand.replace(QStringLiteral("MAILADDRESS"),gsString(mmailAddress));
    mailFinalCommand.replace(QStringLiteral("MAILSUBJECT"),gsString(mmailSubject));
    mailFinalCommand.replace(QStringLiteral("MAILBODY"),gsString(mmailBody));
    mailFinalCommand.replace(QStringLiteral("MAILATTACH"),gsString(mmailFname));
    
    mailSend->start(mailFinalCommand);
    lbl->setText(i18n("kEasySkan is ready."));
}


void kEasySkan::imageWriter(const QUrl fNameUrl, const QByteArray fFormat, int fQuality)
{
    writeOk=false;
    QString fName;
    
    if (fNameUrl.isLocalFile()) {
        fName=fNameUrl.toLocalFile();
        int j;
        if (QFile::exists(fName)==true) {
            j = KMessageBox::warningContinueCancel(0,fName+i18n(" already exists.\nOverwrite ?")); 
        }
        if (j==2) {return;}
    }
    else {
        fName=tmpDir+fNameUrl.fileName();
    }
    
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
    
    if (fNameUrl.isLocalFile()) {
        writeOk=true;
    }
    else {
        docUploader(fName,fNameUrl);
        // todo check if remote file exists
        if (uploadFileOk) writeOk=true;
    }
}



void kEasySkan::pdfWriter(const QString fName, const QString docName, bool confirmOverwrite)
{
    
    lbl->setText(i18n("Creating PDF file..."));
    lbl2->setText(i18n("Creating PDF file..."));
    
    qApp->processEvents();    
    
    pdfWriterSuccess=false;
    
    int j;
    if (QFile::exists(fName)==true && confirmOverwrite) {
        j = KMessageBox::warningContinueCancel(0,fName+i18n(" already exists.\nOverwrite ?")); 
    }
    if (j==2) {return;}
    
    QFileInfo fInfo(fName);
    QString path=fInfo.absolutePath();
    QString bName=fInfo.completeBaseName();
    
    if (QFile::exists(fName)) {QFile::remove(fName);}
    
    QSize size = mImage.size();
    qreal x = qreal(1000) * qreal(size.width()) / qreal(mImage.dotsPerMeterX()) ;
    qreal y = qreal(1000) * qreal(size.height()) / qreal(mImage.dotsPerMeterY()) ;
    QPrinter *printToPdf = new QPrinter(QPrinter::HighResolution);
    
    QPageSize pSize = QPageSize(QSizeF(x,y),QPageSize::Millimeter,QString(),QPageSize::FuzzyMatch);
    
    printToPdf->setOutputFormat(QPrinter::PdfFormat);
    printToPdf->setPageSize(pSize);
    printToPdf->setOutputFileName(fName); 
    
    if (!docName.isEmpty()) { printToPdf->setDocName(docName+QLatin1String(" created by kEasySkan!")); }
    QPainter painter(printToPdf);
    QRect rect = painter.viewport();
    
    size.scale(rect.size(), Qt::KeepAspectRatio);
    painter.setViewport(rect.x(), rect.y(),size.width(), size.height());
    painter.setWindow(mImage.rect());
    painter.drawImage(0, 0, mImage);
    
    lbl->clear();
    lbl2->clear();
    qApp->processEvents();
    
    if (!QFile::exists(fName)) {
        QString strError;
        if (QFile::exists(path)==false) {strError=QString(path+i18n(" : Directory not found or invalid file name"));}
        if (fInfo.isWritable()==false && QFile::exists(path)==true) {strError=QString(path+i18n(" : Directory not writable"));}
        if (fInfo.isWritable()==true && QFile::exists(path)==true) {strError=QString(i18n(" Scanner didn't produce a valid output."));}
        KMessageBox::error(0,i18n("PDF operation failed.\nPlease check your settings and try again.\nIf the proplem persists, restart kEasySkan.\nError : ")+strError);
        pdfWriterSuccess=false;
        return;
    }
    else pdfWriterSuccess=true;
}



bool   kEasySkan::gsMerge(const QString fName1, const QString fName2, const QString pwd)
{ 
    lbl->setText(i18n("Merging PDF files..."));
    lbl2->setText(i18n("Merging PDF files..."));
    qApp->processEvents();
    
    gsMergeOk=false;
    QString Command;
    QString fNameBackup = fName1 + (QStringLiteral(".backup"))  ;
    
    if (QFile::exists(fNameBackup)) {QFile::remove(fNameBackup);}
    
    QFile::copy (fName1,fNameBackup);
    
    if (m_settingsUi.pdfQuality->currentIndex()==2) {
        Command=QStringLiteral("gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -sPDFPassword=PPPP -sOutputFile=OOOO FIRSTFILE LASTFILE");
    }
    
    else {
        Command=QStringLiteral("gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -dPDFSETTINGS=QQQQ -sPDFPassword=PPPP -sOutputFile=OOOO FIRSTFILE LASTFILE");
        
    }
        
        Command.replace(QStringLiteral("QQQQ"),pdfQuality);
        Command.replace(QStringLiteral("PPPP"),gsString(pwd));
        Command.replace(QStringLiteral("OOOO"),gsString(fName1));
        Command.replace(QStringLiteral("FIRSTFILE"),gsString(fNameBackup));
        Command.replace(QStringLiteral("LASTFILE"),gsString(fName2));
        
        QProcess *gs = new QProcess();
        
        gs->start(Command);
        gs->waitForFinished();
        int j = gs->exitCode();
        
        QString err = QString::fromStdString( ((gs->readAllStandardError())).toStdString() );
        QString out = QString::fromStdString( ((gs->readAllStandardOutput())).toStdString() );
        
        lbl->clear();
        lbl2->clear();
        qApp->processEvents();
        
        if (j==0) {
            QFile::remove(fName2);
            QFile::remove(fNameBackup);
            return gsMergeOk=true;
        }
        else {
            KMessageBox::error (0,i18n("PDF operation failed.\nGhostscript output:\n") +out+err);
            QFile::remove (fName1);
            QFile::rename (fNameBackup,fName1);
            return gsMergeOk=false;
        }
}


void kEasySkan::gsEncrypt(const QString fName)

{ 
    gsEncryptOk=false;
    lbl->setText(i18n("Encrypting PDF file..."));
    lbl2->setText(i18n("Encrypting PDF file..."));
    qApp->processEvents();
    QString Command;
    QString fNameBackup = fName + (QStringLiteral(".backup"))  ;
    
    if (QFile::exists(fNameBackup)) {QFile::remove(fNameBackup);}
    
    QFile::copy (fName,fNameBackup);
    
    if (m_settingsUi.pdfQuality->currentIndex()==2) {
        Command=QStringLiteral("gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -sOwnerPassword=PPPP -sUserPassword=PPPP -sOutputFile=OOOO INPUTFILE");}
        else {
            Command=QStringLiteral("gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -dPDFSETTINGS=QQQQ -sOwnerPassword=PPPP -sUserPassword=PPPP -sOutputFile=OOOO INPUTFILE");}
            
            Command.replace(QStringLiteral("QQQQ"),pdfQuality);
            Command.replace(QStringLiteral("PPPP"),gsString(pdfPasswd));
            Command.replace(QStringLiteral("OOOO"),gsString(fName));
            Command.replace(QStringLiteral("INPUTFILE"),gsString(fNameBackup));
            
            QProcess *gs = new QProcess();
            
            gs->start(Command);
            
            gs->waitForFinished();
            lbl->clear();
            lbl2->clear();
            qApp->processEvents();
            int j = gs->exitCode();
            
            QString err = QString::fromStdString( ((gs->readAllStandardError())).toStdString() );
            QString out = QString::fromStdString( ((gs->readAllStandardOutput())).toStdString() );
            
            if (j==0) {
                QFile::remove(fNameBackup);
                gsEncryptOk=true;
            }
            else {
                KMessageBox::error (0,i18n("PDF operation failed.\nGhostscript output:\n") +out+err);
                QFile::remove (fName);
                QFile::rename (fNameBackup,fName);
                gsEncryptOk=false;
            }
}

void kEasySkan::gsQuality(const QString fName)

{ 
    gsQualityOk=false;
    lbl->setText(i18n("Adjusting quality..."));
    lbl2->setText(i18n("Adjusting quality..."));
    qApp->processEvents();
    
    QString Command;
    QString fNameBackup = fName + (QStringLiteral(".backup"))  ;
    
    if (QFile::exists(fNameBackup)) {QFile::remove(fNameBackup);}
    
    QFile::copy (fName,fNameBackup);
    
    if (m_settingsUi.pdfQuality->currentIndex()==2) {
        Command=QStringLiteral("gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -sOutputFile=OOOO INPUTFILE");
    }
    else {
        Command=QStringLiteral("gs -dBATCH -dNOPAUSE -q -sDEVICE=pdfwrite -dPDFSETTINGS=QQQQ -sOutputFile=OOOO INPUTFILE");
    }
    
    Command.replace(QStringLiteral("QQQQ"),pdfQuality);
    Command.replace(QStringLiteral("OOOO"),gsString(fName));
    Command.replace(QStringLiteral("INPUTFILE"),gsString(fNameBackup));
    
    QProcess *gs = new QProcess();
    
    gs->start(Command);
    
    gs->waitForFinished();
    lbl->clear();
    lbl2->clear();
    qApp->processEvents();
    int j = gs->exitCode();
    
    QString err = QString::fromStdString( ((gs->readAllStandardError())).toStdString() );
    QString out = QString::fromStdString( ((gs->readAllStandardOutput())).toStdString() );
    
    if (j==0) {
        QFile::remove(fNameBackup);
        gsQualityOk=true;
    }
    else {
        KMessageBox::error (0,i18n("PDF operation failed.\nGhostscript output:\n") +out+err);
        QFile::remove (fName);
        QFile::rename (fNameBackup,fName);
        gsQualityOk=false;
    }
    
}


void kEasySkan::gsCheckForPasswd (const QString fName, const QString pwd)
{
    gsIsProtected=false;
    gsPasswdIsCorrect=false;
    
    QString Command=QStringLiteral("gs -dBATCH -dNOPAUSE -sNODISPLAY -sPDFPassword=PPPP INPUTFILE");
    
    Command.replace(QStringLiteral("PPPP"),gsString(pwd));
    Command.replace(QStringLiteral("INPUTFILE"),gsString(fName));
    
    QProcess *gs = new QProcess();
    
    gs->start(Command);
    gs->waitForFinished();
    int j = gs->exitCode();
    
    QString err = QString::fromStdString( ((gs->readAllStandardError())).toStdString() );
    QString out = QString::fromStdString( ((gs->readAllStandardOutput())).toStdString() );
    
    if (j==0 && pwd.isEmpty()) gsIsProtected=false;
    if (j!=0 && pwd.isEmpty()) gsIsProtected=true;
    if (j==0 && !pwd.isEmpty()) gsPasswdIsCorrect=true;
    if (j!=0 && !pwd.isEmpty()) gsPasswdIsCorrect=false;
}


int kEasySkan::autoNumber (const QUrl fNamePrefix)
{
    QString bName;
    bool ok,local=false;
    
    QDir *mdir = new QDir();
    QStringList matchingFiles;
    
    if (fNamePrefix.isLocalFile()) local=true;
    
    if (local) {
        QFileInfo fInfo (fNamePrefix.toLocalFile());
        bName=fInfo.completeBaseName();
        mdir->setPath(fInfo.absolutePath());
        QStringList fFilters;
        mdir->setNameFilters(fFilters); // use empty filter
        matchingFiles=mdir->QDir::entryList(); // to list all files
    }
    else //remote
    {
        lbl->setText(i18n("Getting information from remote host..."));
        lbl2->setText(i18n("Getting information from remote host..."));
        qApp->processEvents();
        bName=fNamePrefix.fileName();
        QUrl urlSlash=QUrl(fNamePrefix).resolved(QUrl(QStringLiteral("./")));
        listjob = KIO::listDir(urlSlash,KIO::JobFlag::DefaultFlags,true);
        connect( listjob, &KIO::ListJob::entries, this, &kEasySkan::remoteEntries);
        QTimer *tmr = new QTimer(this);
        tmr->setSingleShot(true);
        QObject::connect(tmr,SIGNAL(timeout()),this,SLOT(killListJob()));
        tmr->start(60000);
        listjob->exec();
        if (tmr->isActive()) {
            tmr->stop(); 
        }
        else {
            return aNumber=-1;
        }
        lbl->clear();
        lbl2->clear();
        qApp->processEvents();
        matchingFiles=remoteFileList;
    }
    
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
    
    aNumber=list[0]+1  ; //fist entry +1 is what we were looking for
    return aNumber;
}  


QString kEasySkan::numberToString (int i, int length)
{
    std::stringstream stdStream;
    stdStream << std::setw(length) << std::setfill('0') << i;
    return QString::fromStdString(stdStream.str());
}

void kEasySkan::docUploader (const QString localName, const QUrl remoteName)
{
    lbl->setText(i18n("Uploading file to remote host..."));
    lbl2->setText(i18n("Uploading file to remote host..."));
    qApp->processEvents();
    uploadFileOk=false;
    QFileInfo fInfo(localName);
    QString bName=fInfo.fileName();
    
    if( KIO::NetAccess::upload( localName, remoteName, nullptr ) ) {
        lbl->clear();
        lbl2->clear();
        qApp->processEvents();
        uploadFileOk=true;
        if (!(localName.right(5)).contains(QStringLiteral(".orig")))  {
            KMessageBox::information(this,bName+i18n("  successfully uploaded to remote host."));\
        }
    } 
    else {
        lbl->clear();
        lbl2->clear();
        qApp->processEvents();
        KMessageBox::error(this, KIO::NetAccess::lastErrorString() );
    }
}

QString kEasySkan::getPasswd()
{
    QString result;
    KNewPasswordDialog dlg;
    dlg.setPrompt( i18n( "Enter password for PDF encryption" ) );
    dlg.setMinimumPasswordLength(1);
    if (dlg.exec()) {
        result=dlg.password();
    }
    else {
        result.clear();
    }
    return result;
}

QString kEasySkan::gsString (QString str0)
{
    str0.prepend(QStringLiteral("\""));
    str0.append(QStringLiteral("\""));
    return str0;
}

void kEasySkan::remoteEntries( KIO::Job *job, const KIO::UDSEntryList &list )
{
    for( KIO::UDSEntryList::ConstIterator it = list.begin(); it != list.end(); ++it )
    {
        const KIO::UDSEntry &entry = *it;
        remoteFileList << entry.stringValue( KIO::UDSEntry::UDS_NAME );
    }
}

void kEasySkan::killListJob()
{
    lbl->setText(i18n("Stopping remote service..."));
    lbl2->setText(i18n("Stopping remote service..."));
    qApp->processEvents();
    if (listjob->kill()) {
        KMessageBox::sorry(0,i18n("Couldn't get information from remote host within 1 min.\nAborting..."));
    }
}
