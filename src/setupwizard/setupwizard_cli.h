#ifndef SETUPWIZARD_CLI_H
#define SETUPWIZARD_CLI_H

#include <TWizardPage.h>

#include "taskstatus.h"

/* Forward declaration. */
namespace Ui
{
class CliPage;
}

/**
 * \ingroup widgets-setup
 * \brief The CLIPage is a TWizardPage which searches for the
 * CLI and configures directories.
 */
class CliPage : public TWizardPage
{
    Q_OBJECT

public:
    //! Constructor.
    CliPage(QWidget *parent = nullptr);
    ~CliPage() override;

    //! Override the default TWizardPage functions
    void initializePage() override;

#ifdef QT_TESTLIB_LIB
    Ui::CliPage *get_ui() const { return _ui; }
#endif

public slots:
    //! The TaskManager has finished determining the version of the CLI app.
    //! \param status did the task complete?
    //! \param versionString the version number.
    void tarsnapVersionResponse(TaskStatus status, QString versionString);

signals:
    //! We need to determine the Tarsnap CLI version number.
    void tarsnapVersionRequested();

protected:
    //! Has the user filled in all mandatory data on the page?
    bool checkComplete() override;

private slots:
    void tarsnapPathChanged(const QString &text);
    void tarsnapCacheChanged(const QString &text);
    void appDataDirChanged(const QString &text);

private:
    Ui::CliPage *_ui;

    // Has there been any problems?  If there haven't, and the CLI
    // version is high enough, then we want to auto-focus the
    // "Next" button.  But if we've been manually poking around in
    // the details, we don't want to steal the focus from where
    // the user is currently working.
    bool _problemOccurred;

    // We store the final configured state in the TSettings.  Namely:
    // "tarsnap/path": directory of the CLI.
    // "tarsnap/version": version number (if high enough).
    // "tarsnap/cache": directory of tarsnap --cachedir.
    // "app/app_data": directory of the application database and
    //                 default key location.

    void reportError(const QString &text);
    bool ensureDirExists(const QString &dirname);
};

#endif /* !SETUPWIZARD_CLI_H */
