#include "helpwidget.h"

WARNINGS_DISABLE
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QIODevice>
#include <QKeySequence>
#include <QTextBrowser>
#include <QTextStream>
#include <QWidget>
#include <Qt>

#include "ui_consolelogdialog.h"
#include "ui_helpwidget.h"
WARNINGS_ENABLE

#include "LogEntry.h"
#include "TPopupPushButton.h"
#include "TTextView.h"

#include "aboutdialog.h"

#include "debug.h"

HelpWidget::HelpWidget(QWidget *parent)
    : QWidget(parent),
      _ui(new Ui::HelpWidget),
      _aboutWindow(new AboutDialog(this)),
      _consoleWindow(new QDialog(this))
{
    // Ui initialization
    _ui->setupUi(this);

    // Initialize the Help tab text
    QFile helpTabFile(":/text/help-tab.xml");
    if(!helpTabFile.open(QFile::ReadOnly | QIODevice::Text))
    {
        DEBUG << "Failed to load a resource file.";
    }
    _helpTabHTML = QTextStream(&helpTabFile).readAll();
    helpTabFile.close();

    // After reading the Help tab text
    updateKeyboardShortcutInfo();

    // Initialize About window
    _ui->aboutButton->setPopup(_aboutWindow);

    // Initialize console log
    Ui::ConsoleLogDialog consoleUI;
    consoleUI.setupUi(_consoleWindow);
    _consoleLog = consoleUI.log;
    _ui->consoleButton->setPopup(_consoleWindow);
}

HelpWidget::~HelpWidget()
{
    delete _ui;
}

void HelpWidget::appendLogString(const QString &text)
{
    _consoleLog->appendLog(LogEntry{QDateTime::currentDateTime(), text});
}

void HelpWidget::aboutMenuClicked()
{
    // This always displays the About window
    _ui->aboutButton->setChecked(true);
}

void HelpWidget::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        _ui->retranslateUi(this);
        updateKeyboardShortcutInfo();
    }
    QWidget::changeEvent(event);
}

void HelpWidget::updateKeyboardShortcutInfo()
{
    // Keyboard shortcuts
    _ui->helpTabText->setHtml(
        _helpTabHTML
            .arg(QKeySequence(Qt::ControlModifier)
                     .toString(QKeySequence::NativeText))
            .arg(QKeySequence(Qt::ControlModifier + Qt::ShiftModifier)
                     .toString(QKeySequence::NativeText))
            .arg(QKeySequence(Qt::Key_Backspace)
                     .toString(QKeySequence::NativeText))
            .arg(QKeySequence(Qt::Key_Delete)
                     .toString(QKeySequence::NativeText)));
}
