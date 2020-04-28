#include "settingswidget.h"

WARNINGS_DISABLE
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVariant>
#include <QWidget>

#include "ui_settingswidget.h"
WARNINGS_ENABLE

#include "ConsoleLog.h"
#include "TElidedLabel.h"
#include "TSettings.h"

#include "messages/taskstatus.h"

#include "confirmationdialog.h"
#include "schedulingwidgets.h"
#include "tarsnapaccountdialog.h"
#include "tasks/tasks-defs.h"
#include "translator.h"
#include "utils.h"

#define NUKE_SECONDS_DELAY 8

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent),
      _ui(new Ui::SettingsWidget),
      _nukeConfirmationDialog(new ConfirmationDialog(this)),
      _tarsnapAccountDialog(new TarsnapAccountDialog(this)),
      _schedulingWidgets(new SchedulingWidgets(this))
{

    // Ui initialization
    _ui->setupUi(this);

    // Settings
    loadSettings();
    _ui->outOfDateNoticeLabel->hide();

    initSettingsSetValue();

    // Settings
    connect(_ui->accountMachineKeyLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::validateMachineKeyPath);
    connect(_ui->accountMachineKeyBrowseButton, &QPushButton::clicked, this,
            &SettingsWidget::accountMachineKeyBrowseButtonClicked);
    connect(_ui->nukeArchivesButton, &QPushButton::clicked, this,
            &SettingsWidget::nukeArchivesButtonClicked);
    connect(_nukeConfirmationDialog, &ConfirmationDialog::confirmed, this,
            &SettingsWidget::nukeConfirmed);
    connect(_nukeConfirmationDialog, &ConfirmationDialog::cancelled, this,
            &SettingsWidget::nukeCancelled);
    connect(_tarsnapAccountDialog, &TarsnapAccountDialog::accountCredit, this,
            &SettingsWidget::updateAccountCredit);
    connect(_tarsnapAccountDialog, &TarsnapAccountDialog::getKeyId, this,
            &SettingsWidget::getKeyId);
    connect(_ui->updateAccountButton, &QPushButton::clicked,
            _ui->actionRefreshAccount, &QAction::trigger);
    connect(_tarsnapAccountDialog, &TarsnapAccountDialog::lastMachineActivity,
            this, &SettingsWidget::updateLastMachineActivity);
    connect(_ui->accountActivityShowButton, &QPushButton::clicked,
            [this]() { _tarsnapAccountDialog->getAccountInfo(true, false); });
    connect(_ui->machineActivityShowButton, &QPushButton::clicked,
            [this]() { _tarsnapAccountDialog->getAccountInfo(false, true); });
    connect(_ui->actionRefreshAccount, &QAction::triggered, this,
            &SettingsWidget::getAccountInfo);

    /* Backup tab */
    connect(_ui->enableSchedulingButton, &QPushButton::clicked,
            _schedulingWidgets,
            &SchedulingWidgets::enableJobSchedulingButtonClicked);
    connect(_ui->disableSchedulingButton, &QPushButton::clicked,
            _schedulingWidgets,
            &SchedulingWidgets::disableJobSchedulingButtonClicked);
    connect(_ui->simulationCheckBox, &QCheckBox::stateChanged, this,
            &SettingsWidget::updateSimulationIcon);
    connect(_ui->skipSystemDefaultsButton, &QPushButton::clicked, [this]() {
        _ui->skipSystemLineEdit->setText(DEFAULT_SKIP_SYSTEM_FILES);
    });

    /* Application tab */
    connect(_ui->tarsnapPathLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::validateTarsnapPath);
    connect(_ui->tarsnapCacheLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::validateTarsnapCache);
    connect(_ui->appDataDirLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::validateAppDataDir);

    connect(_ui->tarsnapPathBrowseButton, &QPushButton::clicked, this,
            &SettingsWidget::tarsnapPathBrowseButtonClicked);
    connect(_ui->tarsnapCacheBrowseButton, &QPushButton::clicked, this,
            &SettingsWidget::tarsnapCacheBrowseButton);
    connect(_ui->appDataDirBrowseButton, &QPushButton::clicked, this,
            &SettingsWidget::appDataButtonClicked);
    connect(_ui->runSetupWizard, &QPushButton::clicked, this,
            &SettingsWidget::runSetupWizardClicked);
    connect(_ui->downloadsDirBrowseButton, &QPushButton::clicked, this,
            &SettingsWidget::downloadsDirBrowseButtonClicked);
    connect(_ui->clearJournalButton, &QPushButton::clicked, this,
            &SettingsWidget::clearJournalClicked);

    connect(_ui->downloadsDirLineEdit, &QLineEdit::textChanged, [this]() {
        QFileInfo file(_ui->downloadsDirLineEdit->text());
        if(file.exists() && file.isDir() && file.isWritable())
            _ui->downloadsDirLineEdit->setStyleSheet("QLineEdit{color:black;}");
        else
            _ui->downloadsDirLineEdit->setStyleSheet("QLineEdit{color:red;}");
    });
    connect(_ui->repairCacheButton, &QPushButton::clicked,
            [this]() { emit repairCache(true); });
    connect(_ui->iecPrefixesCheckBox, &QCheckBox::toggled, [this]() {
        QMessageBox::information(this, QCoreApplication::applicationName(),
                                 tr("The new size notation will take global "
                                    "effect on application restart."));
    });

    connect(_ui->languageComboBox, &QComboBox::currentTextChanged,
            [this](const QString &language) {
                if(!language.isEmpty())
                {
                    this->commitSettings();
                    global_translator->translateApp(qApp, language);
                }
            });

    updateUi();
}

SettingsWidget::~SettingsWidget()
{
    delete _nukeConfirmationDialog;
    delete _tarsnapAccountDialog;
    delete _schedulingWidgets;
    delete _ui;
}

void SettingsWidget::initializeSettingsWidget()
{
    TSettings settings;

    /* Account tab */

    // Check if we should show a "credit might be out of date" warning.
    QDate creditDate = settings.value("tarsnap/credit_date", QDate()).toDate();
    if(creditDate.isValid())
    {
        _ui->accountCreditLabel->setToolTip(creditDate.toString());
        qint64 daysElapsed = creditDate.daysTo(QDate::currentDate());
        if(daysElapsed > 0)
        {
            _ui->outOfDateNoticeLabel->setText(
                _ui->outOfDateNoticeLabel->text().arg(daysElapsed));
            _ui->outOfDateNoticeLabel->show();
        }
        else
        {
            _ui->outOfDateNoticeLabel->hide();
        }
    }

    if(!validateMachineKeyPath())
    {
        QMessageBox::critical(this, tr("Tarsnap error"),
                              tr("Machine key file not found. Go to "
                                 " Settings -> Account page to fix that."));
    }

    /* Application tab */

    // Validate applications paths.
    if(!validateTarsnapPath())
    {
        QMessageBox::critical(this, tr("Tarsnap error"),
                              tr("Tarsnap CLI utilities not found. Go to "
                                 " Settings -> Application page to fix that."));
    }

    if(!validateTarsnapCache())
    {
        QMessageBox::critical(this, tr("Tarsnap error"),
                              tr("Tarsnap cache dir is invalid. Go to "
                                 " Settings -> Application page to fix that."));
    }

    if(!validateAppDataDir())
    {
        QMessageBox::critical(this, tr("Tarsnap error"),
                              tr("Application data dir is invalid. Go to "
                                 " Settings -> Application page to fix that."));
    }
}

void SettingsWidget::initSettingsSetValue()
{

    /* Account tab */
    connect(_ui->accountUserLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::commitSettings);
    connect(_ui->accountMachineKeyLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::commitSettings);

    /* Backup tab */
    connect(_ui->aggressiveNetworkingCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
    connect(_ui->preservePathsCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
    connect(_ui->traverseMountCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
    connect(_ui->followSymLinksCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
    connect(_ui->skipFilesSizeSpinBox, &QSpinBox::editingFinished, this,
            &SettingsWidget::commitSettings);
    connect(_ui->skipSystemJunkCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
    connect(_ui->skipSystemLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::commitSettings);
    connect(_ui->skipNoDumpCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
    connect(_ui->simulationCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
    connect(_ui->ignoreConfigCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);

    connect(_ui->limitUploadSpinBox, &QSpinBox::editingFinished, this,
            &SettingsWidget::commitSettings);
    connect(_ui->limitDownloadSpinBox, &QSpinBox::editingFinished, this,
            &SettingsWidget::commitSettings);

    /* Application tab */
    connect(_ui->tarsnapPathLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::commitSettings);
    connect(_ui->tarsnapCacheLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::commitSettings);
    connect(_ui->iecPrefixesCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
    connect(_ui->notificationsCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
    connect(_ui->downloadsDirLineEdit, &QLineEdit::textChanged, this,
            &SettingsWidget::commitSettings);
    connect(_ui->saveConsoleLogCheckBox, &QCheckBox::toggled, this,
            &SettingsWidget::commitSettings);
}

void SettingsWidget::loadSettings()
{
    TSettings settings;

    /* Account tab */
    _ui->accountCreditLabel->setText(
        settings.value("tarsnap/credit", tr("click login button")).toString());
    _ui->machineActivity->setText(
        settings.value("tarsnap/machine_activity", tr("click login button"))
            .toString());
    _ui->accountUserLineEdit->setText(
        settings.value("tarsnap/user", "").toString());
    _ui->accountMachineKeyLineEdit->setText(
        settings.value("tarsnap/key", "").toString());

    /* Backup tab */
    _ui->aggressiveNetworkingCheckBox->setChecked(
        settings
            .value("tarsnap/aggressive_networking",
                   DEFAULT_AGGRESSIVE_NETWORKING)
            .toBool());
    _ui->traverseMountCheckBox->setChecked(
        settings.value("tarsnap/traverse_mount", DEFAULT_TRAVERSE_MOUNT)
            .toBool());
    _ui->followSymLinksCheckBox->setChecked(
        settings.value("tarsnap/follow_symlinks", DEFAULT_FOLLOW_SYMLINKS)
            .toBool());
    _ui->preservePathsCheckBox->setChecked(
        settings.value("tarsnap/preserve_pathnames", DEFAULT_PRESERVE_PATHNAMES)
            .toBool());
    _ui->ignoreConfigCheckBox->setChecked(
        settings.value("tarsnap/no_default_config", DEFAULT_NO_DEFAULT_CONFIG)
            .toBool());
    _ui->simulationCheckBox->setChecked(
        settings.value("tarsnap/dry_run", DEFAULT_DRY_RUN).toBool());
    _ui->skipFilesSizeSpinBox->setValue(
        settings.value("app/skip_files_size", DEFAULT_SKIP_FILES_SIZE).toInt());
    _ui->skipSystemJunkCheckBox->setChecked(
        settings.value("app/skip_system_enabled", DEFAULT_SKIP_SYSTEM_ENABLED)
            .toBool());
    _ui->skipSystemLineEdit->setEnabled(
        _ui->skipSystemJunkCheckBox->isChecked());
    _ui->skipSystemLineEdit->setText(
        settings.value("app/skip_system_files", DEFAULT_SKIP_SYSTEM_FILES)
            .toString());
    _ui->skipNoDumpCheckBox->setChecked(
        settings.value("app/skip_nodump", DEFAULT_SKIP_NODUMP).toBool());
    _ui->limitUploadSpinBox->setValue(
        settings.value("app/limit_upload", 0).toInt());
    _ui->limitDownloadSpinBox->setValue(
        settings.value("app/limit_download", 0).toInt());

    /* Application tab */
    _ui->tarsnapPathLineEdit->setText(
        settings.value("tarsnap/path", "").toString());
    _ui->tarsnapCacheLineEdit->setText(
        settings.value("tarsnap/cache", "").toString());
    _ui->iecPrefixesCheckBox->setChecked(
        settings.value("app/iec_prefixes", false).toBool());
    _ui->downloadsDirLineEdit->setText(
        settings.value("app/downloads_dir", DEFAULT_DOWNLOADS).toString());
    _ui->appDataDirLineEdit->setText(
        settings.value("app/app_data", "").toString());
    _ui->notificationsCheckBox->setChecked(
        settings.value("app/notifications", true).toBool());
    _ui->saveConsoleLogCheckBox->setChecked(
        settings.value("app/save_console_log", false).toBool());
    _ui->saveConsoleLogLineEdit->setText(LOG.getLogFile());
    LOG.setWriteToFile(settings.value("app/save_console_log", false).toBool());

    _ui->languageComboBox->addItem(LANG_AUTO);
    _ui->languageComboBox->addItems(global_translator->languageList());
    _ui->languageComboBox->setCurrentText(
        settings.value("app/language", LANG_AUTO).toString());
}

void SettingsWidget::commitSettings()
{
    TSettings settings;

    /* Account tab */
    settings.setValue("tarsnap/key", _ui->accountMachineKeyLineEdit->text());
    settings.setValue("tarsnap/user", _ui->accountUserLineEdit->text());

    /* Backup tab */
    settings.setValue("tarsnap/aggressive_networking",
                      _ui->aggressiveNetworkingCheckBox->isChecked());
    settings.setValue("tarsnap/preserve_pathnames",
                      _ui->preservePathsCheckBox->isChecked());
    settings.setValue("tarsnap/traverse_mount",
                      _ui->traverseMountCheckBox->isChecked());
    settings.setValue("tarsnap/follow_symlinks",
                      _ui->followSymLinksCheckBox->isChecked());
    settings.setValue("tarsnap/no_default_config",
                      _ui->ignoreConfigCheckBox->isChecked());
    settings.setValue("tarsnap/dry_run", _ui->simulationCheckBox->isChecked());
    settings.setValue("app/skip_files_size",
                      _ui->skipFilesSizeSpinBox->value());
    settings.setValue("app/skip_system_enabled",
                      _ui->skipSystemJunkCheckBox->isChecked());
    settings.setValue("app/skip_system_files", _ui->skipSystemLineEdit->text());
    settings.setValue("app/skip_nodump", _ui->skipNoDumpCheckBox->isChecked());

    settings.setValue("app/limit_upload", _ui->limitUploadSpinBox->value());
    settings.setValue("app/limit_download", _ui->limitDownloadSpinBox->value());

    /* Application tab */
    settings.setValue("tarsnap/path", _ui->tarsnapPathLineEdit->text());
    settings.setValue("tarsnap/cache", _ui->tarsnapCacheLineEdit->text());
    settings.setValue("app/iec_prefixes",
                      _ui->iecPrefixesCheckBox->isChecked());
    settings.setValue("app/downloads_dir", _ui->downloadsDirLineEdit->text());
    settings.setValue("app/app_data", _ui->appDataDirLineEdit->text());
    settings.setValue("app/notifications",
                      _ui->notificationsCheckBox->isChecked());
    settings.setValue("app/language", _ui->languageComboBox->currentText());
    settings.setValue("app/save_console_log",
                      _ui->saveConsoleLogCheckBox->isChecked());
    LOG.setWriteToFile(settings.value("app/save_console_log", false).toBool());

    settings.sync();
}

void SettingsWidget::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        _ui->retranslateUi(this);
        updateUi();
    }
    QWidget::changeEvent(event);
}

void SettingsWidget::getAccountInfo()
{
    _tarsnapAccountDialog->getAccountInfo();
}

void SettingsWidget::overallStatsChanged(quint64 sizeTotal,
                                         quint64 sizeCompressed,
                                         quint64 sizeUniqueTotal,
                                         quint64 sizeUniqueCompressed,
                                         quint64 archiveCount)
{
    QString tooltip(tr("\t\tTotal size\tCompressed size\n"
                       "all archives\t%1\t\t%2\n"
                       "unique data\t%3\t\t%4")
                        .arg(sizeTotal)
                        .arg(sizeCompressed)
                        .arg(sizeUniqueTotal)
                        .arg(sizeUniqueCompressed));
    _ui->accountTotalSizeLabel->setText(Utils::humanBytes(sizeTotal));
    _ui->accountTotalSizeLabel->setToolTip(tooltip);
    _ui->accountActualSizeLabel->setText(
        Utils::humanBytes(sizeUniqueCompressed));
    _ui->accountActualSizeLabel->setToolTip(tooltip);
    quint64 storageSaved = sizeTotal >= sizeUniqueCompressed
                               ? sizeTotal - sizeUniqueCompressed
                               : 0;
    _ui->accountStorageSavedLabel->setText(Utils::humanBytes(storageSaved));
    _ui->accountStorageSavedLabel->setToolTip(tooltip);
    _ui->accountArchivesCountLabel->setText(QString::number(archiveCount));
}

bool SettingsWidget::validateMachineKeyPath()
{
    QFileInfo machineKeyFile(_ui->accountMachineKeyLineEdit->text());
    if(machineKeyFile.exists() && machineKeyFile.isFile()
       && machineKeyFile.isReadable())
    {
        _ui->accountMachineKeyLineEdit->setStyleSheet(
            "QLineEdit {color: black;}");
        return true;
    }
    else
    {
        _ui->accountMachineKeyLineEdit->setStyleSheet(
            "QLineEdit {color: red;}");
        return false;
    }
}

void SettingsWidget::saveKeyId(const QString &key, quint64 id)
{
    if(key == _ui->accountMachineKeyLineEdit->text())
    {
        TSettings settings;
        settings.setValue("tarsnap/key_id", id);
        settings.sync();
    }
}

void SettingsWidget::accountMachineKeyBrowseButtonClicked()
{
    QString key =
        QFileDialog::getOpenFileName(this,
                                     tr("Browse for existing machine key"));
    if(!key.isEmpty())
    {
        _ui->accountMachineKeyLineEdit->setText(key);
        commitSettings();
    }
}

void SettingsWidget::nukeArchivesButtonClicked()
{
    const QString confirmationText = tr("No Tomorrow");

    newStatusMessage(tr("Nuke confirmation requested."), "");

    // Set up nuke confirmation
    _nukeConfirmationDialog->start(
        tr("Nuke all archives?"),
        tr("This action will <b>delete all (%1) archives</b> stored for this "
           "key."
           "<br /><br />To confirm, type '%2' and press OK."
           "<br /><br /><i>Warning: This action cannot be undone. "
           "All archives will be <b>lost forever</b></i>.")
            .arg(_ui->accountArchivesCountLabel->text(), confirmationText),
        confirmationText, NUKE_SECONDS_DELAY,
        tr("Deleting all archives: press Cancel to abort"),
        tr("Purging all archives in %1 seconds..."), tr("Confirm nuke"));
}

void SettingsWidget::nukeConfirmed()
{
    emit nukeArchives();
}

void SettingsWidget::nukeCancelled()
{
    newStatusMessage(tr("Nuke cancelled."), "");
}

void SettingsWidget::updateAccountCredit(qreal credit, QDate date)
{
    TSettings settings;
    settings.setValue("tarsnap/credit", QString::number(credit, 'f', 18));
    settings.setValue("tarsnap/credit_date", date);
    _ui->accountCreditLabel->setText(QString::number(credit, 'f', 18));
    _ui->accountCreditLabel->setToolTip(date.toString());
    _ui->outOfDateNoticeLabel->hide();
}

void SettingsWidget::updateLastMachineActivity(QStringList activityFields)
{
    if(activityFields.size() < 2)
        return;
    QString   machineActivity = activityFields[0] + ' ' + activityFields[1];
    TSettings settings;
    settings.setValue("tarsnap/machine_activity", machineActivity);
    _ui->machineActivity->setText(machineActivity);
    _ui->machineActivity->setToolTip(activityFields.join(' '));
    _ui->machineActivity->resize(_ui->machineActivity->fontMetrics()
                                     .boundingRect(_ui->machineActivity->text())
                                     .width(),
                                 _ui->machineActivity->sizeHint().height());
}

void SettingsWidget::updateUi()
{
    _ui->updateAccountButton->setToolTip(
        _ui->updateAccountButton->toolTip().arg(
            _ui->actionRefreshAccount->shortcut().toString(
                QKeySequence::NativeText)));
}

void SettingsWidget::updateSimulationIcon(int state)
{
    emit newSimulationStatus(state);
}

void SettingsWidget::tarsnapVersionResponse(TaskStatus status,
                                            QString    versionString)
{
    TSettings settings;

    // Sanity check.
    if(versionString.isEmpty())
        status = TaskStatus::Failed;

    // Handle response.
    switch(status)
    {
    case TaskStatus::Completed:
        _ui->tarsnapVersionLabel->setText(versionString);
        settings.setValue("tarsnap/version", versionString);
        break;
    case TaskStatus::VersionTooLow:
        // Don't record the too-low version number.
        QMessageBox::critical(
            this, tr("Tarsnap CLI version"),
            tr("Tarsnap CLI version ") + versionString
                + tr(" too low; must be at least %1").arg(TARSNAP_MIN_VERSION));
        break;
    case TaskStatus::Failed:
        QMessageBox::critical(this, tr("Tarsnap CLI version"),
                              tr("Error retrieving Tarsnap CLI version"));
        break;
    default:
        break;
    }
}

bool SettingsWidget::validateTarsnapPath()
{
    struct DirMessage result =
        Utils::findTarsnapClientInPath(_ui->tarsnapPathLineEdit->text());
    if(result.dirname.isEmpty())
    {
        _ui->tarsnapPathLineEdit->setStyleSheet("QLineEdit {color: red;}");
        _ui->tarsnapVersionLabel->clear();
        return false;
    }
    else
    {
        TSettings settings;
        _ui->tarsnapPathLineEdit->setStyleSheet("QLineEdit {color: black;}");
        // Wipe previous version number before asking for a new one.
        settings.setValue("tarsnap/version", "");
        emit tarsnapVersionRequested(_ui->tarsnapPathLineEdit->text());
        return true;
    }
}

bool SettingsWidget::validateTarsnapCache()
{
    const QString cacheDir = _ui->tarsnapCacheLineEdit->text();
    const QString errorMsg = Utils::validate_writeable_dir(cacheDir);
    if(!errorMsg.isEmpty())
    {
        _ui->tarsnapCacheLineEdit->setStyleSheet("QLineEdit {color: red;}");
        return false;
    }
    else
    {
        _ui->tarsnapCacheLineEdit->setStyleSheet("QLineEdit {color: black;}");
        return true;
    }
}

bool SettingsWidget::validateAppDataDir()
{
    const QString cacheDir = _ui->appDataDirLineEdit->text();
    const QString errorMsg = Utils::validate_writeable_dir(cacheDir);
    if(!errorMsg.isEmpty())
    {
        _ui->appDataDirLineEdit->setStyleSheet("QLineEdit {color: red;}");
        return false;
    }
    else
    {
        _ui->appDataDirLineEdit->setStyleSheet("QLineEdit {color: black;}");
        return true;
    }
}

void SettingsWidget::tarsnapPathBrowseButtonClicked()
{
    QString tarsnapPath =
        QFileDialog::getExistingDirectory(this, tr("Find Tarsnap client"),
                                          _ui->tarsnapPathLineEdit->text());
    if(!tarsnapPath.isEmpty())
    {
        _ui->tarsnapPathLineEdit->setText(tarsnapPath);
        commitSettings();
    }
}

void SettingsWidget::tarsnapCacheBrowseButton()
{
    QString tarsnapCacheDir =
        QFileDialog::getExistingDirectory(this, tr("Tarsnap cache location"),
                                          _ui->tarsnapCacheLineEdit->text());
    if(!tarsnapCacheDir.isEmpty())
    {
        _ui->tarsnapCacheLineEdit->setText(tarsnapCacheDir);
        commitSettings();
    }
}

void SettingsWidget::appDataButtonClicked()
{
    QString appDataDir =
        QFileDialog::getExistingDirectory(this,
                                          tr("App data directory location"),
                                          _ui->appDataDirLineEdit->text());
    if(!appDataDir.isEmpty())
    {
        _ui->appDataDirLineEdit->setText(appDataDir);
        commitSettings();
    }
}

void SettingsWidget::runSetupWizardClicked()
{
    if((_runningTasks + _queuedTasks) > 0)
    {
        QMessageBox::warning(this, tr("Confirm action"),
                             tr("Tasks are currently running. Please "
                                "stop executing tasks or wait for "
                                "completion and try again."));
        return;
    }
    QMessageBox::StandardButton confirm =
        QMessageBox::question(this, tr("Confirm action"),
                              tr("Reset current app settings, job definitions "
                                 "and run the setup wizard?"),
                              (QMessageBox::Yes | QMessageBox::No),
                              QMessageBox::No);
    if(confirm == QMessageBox::Yes)
        emit runSetupWizard();
}

void SettingsWidget::downloadsDirBrowseButtonClicked()
{
    QString downDir =
        QFileDialog::getExistingDirectory(this,
                                          tr("Browse for downloads directory"),
                                          DEFAULT_DOWNLOADS);
    if(!downDir.isEmpty())
    {
        _ui->downloadsDirLineEdit->setText(downDir);
        commitSettings();
    }
}

void SettingsWidget::clearJournalClicked()
{
    QMessageBox::StandardButton confirm =
        QMessageBox::question(this, tr("Confirm action"),
                              tr("Clear journal log? All entries will "
                                 "be deleted forever."));
    if(confirm == QMessageBox::Yes)
        emit clearJournal();
}

void SettingsWidget::updateNumTasks(int runningTasks, int queuedTasks)
{
    _runningTasks = runningTasks;
    _queuedTasks  = queuedTasks;
}
