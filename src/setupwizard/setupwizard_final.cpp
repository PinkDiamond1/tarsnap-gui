#include "setupwizard_final.h"

#include "warnings-disable.h"

WARNINGS_DISABLE
#include <QFileInfo>
#include <QString>
#include <QUrl>

#include "ui_setupwizard_final.h"
WARNINGS_ENABLE

#include <TSettings.h>
#include <TWizardPage.h>

FinalPage::FinalPage(QWidget *parent)
    : TWizardPage(parent), _ui(new Ui::FinalPage)
{
    _ui->setupUi(this);
}

FinalPage::~FinalPage()
{
    delete _ui;
}

void FinalPage::initializePage()
{
    TWizardPage::initializePage();

    TSettings settings;

    // Get keyfile info and generate strings.
    const QString keyfile   = settings.value("tarsnap/key", "").toString();
    const QString dirname   = QFileInfo(keyfile).canonicalPath();
    const QString urlstring = QUrl::fromLocalFile(dirname).toString();

    // Fill in the clickable link to the keyfile.
    _ui->keyfileLabel->setText(
        QString("<a href=\"%1\">%2</a>").arg(urlstring).arg(keyfile));
}
