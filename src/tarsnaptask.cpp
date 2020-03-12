#include "tarsnaptask.h"

WARNINGS_DISABLE
#include <QFileInfo>
#include <QStandardPaths>
WARNINGS_ENABLE

#include "utils.h"

#include <ConsoleLog.h>

#include <signal.h>

#define DEFAULT_TIMEOUT_MS 5000
#define LOG_MAX_LENGTH 3072
#define LOG_MAX_SEARCH_NL 1024

TarsnapTask::TarsnapTask()
    : QObject(),
      _uuid(QUuid::createUuid()),
      _process(nullptr),
      _exitCode(EXIT_NO_MEANING),
      _truncateLogOutput(false)
{
#ifdef QT_TESTLIB_LIB
    _fake = false;
#endif
}

TarsnapTask::~TarsnapTask()
{
}

void TarsnapTask::run()
{
    // Set up new _process
    _process = new QProcess();
    _process->setProgram(_command);
    _process->setArguments(_arguments);
    if(!_stdOutFilename.isEmpty())
        _process->setStandardOutputFile(_stdOutFilename);

    LOG << tr("Task %1 started:\n[%2 %3]\n")
               .arg(_uuid.toString())
               .arg(_process->program())
               .arg(Utils::quoteCommandLine(_process->arguments()));

#ifdef QT_TESTLIB_LIB
    // Bail if desired.  Make sure this happens before we check if the
    // command exists, otherwise we might bail for the wrong reason.
    if(_fake)
    {
        LOG << "Not running task due to 'fake this task' request.\n";
        _fake = false;
        emit finished(_data, EXIT_FAKE_REQUEST, "", "");
        goto cleanup;
    }
#endif

    // Make sure that the command exists and is executable, because QProcess
    // doesn't clean up its memory if it fails due to "command not found".
    if(QStandardPaths::findExecutable(_command).isEmpty())
    {
        LOG << QString("Command '%1' not found\n").arg(_command);
        emit finished(_data, EXIT_CMD_NOT_FOUND, "", "");
        goto cleanup;
    }

    // Start the _process, and wait for confirmation of it starting.
    _process->start();
    if(_process->waitForStarted(DEFAULT_TIMEOUT_MS))
    {
        emit started(_data);
    }
    else
    {
        _exitCode = EXIT_DID_NOT_START;
        processError();
        goto cleanup;
    }

    // Write to the process' stdin.
    if(!_stdIn.isEmpty())
    {
        _process->write(_stdIn.data(), _stdIn.size());
        _process->closeWriteChannel();
    }

    // Wait indefinitely for the process to finish
    if(_process->waitForFinished(-1))
    {
        readProcessOutput();
        processFinished();
    }
    else
    {
        readProcessOutput();
        processError();
        goto cleanup;
    }

cleanup:
    delete _process;
    _process = nullptr;
    emit dequeue();
}

void TarsnapTask::stop()
{
    if(_process->state() == QProcess::Running)
        _process->terminate();
}

void TarsnapTask::sigquit()
{
    // Assume that Q_PID is a valid pid on this system.
    pid_t pid = static_cast<pid_t>(_process->pid());
    // If this is sent to the Tarsnap client creating an archive, it will
    // truncate it and leave a '.part' partial archive.
    kill(pid, SIGQUIT);
}

void TarsnapTask::emitCanceled()
{
    emit canceled(_data);
}

QString TarsnapTask::command() const
{
    return _command;
}

void TarsnapTask::setCommand(const QString &command)
{
    _command = command;
}

QStringList TarsnapTask::arguments() const
{
    return _arguments;
}

void TarsnapTask::setArguments(const QStringList &arguments)
{
    _arguments = arguments;
}

void TarsnapTask::setStdIn(const QString &standardIn)
{
    _stdIn = QByteArray(standardIn.toUtf8());
}

void TarsnapTask::setStdOutFile(const QString &fileName)
{
    _stdOutFilename = fileName;
}

QVariant TarsnapTask::data() const
{
    return _data;
}

void TarsnapTask::setData(const QVariant &data)
{
    _data = data;
}

void TarsnapTask::setTruncateLogOutput(bool truncateLogOutput)
{
    _truncateLogOutput = truncateLogOutput;
}

void TarsnapTask::readProcessOutput()
{
    if(_stdOutFilename.isEmpty())
        _stdOut.append(_process->readAllStandardOutput().trimmed());
    _stdErr.append(_process->readAllStandardError().trimmed());
}

QByteArray TarsnapTask::truncate_output(QByteArray stdOut)
{
    // Find a good newline to which to truncate.
    int from = LOG_MAX_LENGTH
               + std::min(stdOut.size() - LOG_MAX_LENGTH, LOG_MAX_SEARCH_NL);
    int nextNL = stdOut.lastIndexOf(QChar('\n'), from);
    // Only keep the first part of the logfile.
    stdOut.truncate(std::max(LOG_MAX_LENGTH, nextNL));
    // Notify about truncation in log.
    int num_truncated = _stdOut.mid(stdOut.size()).count('\n');
    stdOut.append(tr("\n...\n-- %1 output lines truncated by Tarsnap GUI --\n")
                      .arg(num_truncated));
    return stdOut;
}

void TarsnapTask::processFinished()
{
    switch(_process->exitStatus())
    {
    case QProcess::NormalExit:
    {
        _exitCode = _process->exitCode();
        emit finished(_data, _exitCode, QString(_stdOut), QString(_stdErr));

        // Truncate LOG output
        QByteArray stdOut(_stdOut);
        if(_truncateLogOutput && (stdOut.size() > LOG_MAX_LENGTH))
            stdOut = truncate_output(stdOut);

        LOG << tr("Task %1 finished with exit code %2:\n[%3 %4]\n%5\n")
                   .arg(_uuid.toString())
                   .arg(_exitCode)
                   .arg(_command)
                   .arg(Utils::quoteCommandLine(_arguments))
                   .arg(QString(stdOut + _stdErr));
        break;
    }
    case QProcess::CrashExit:
    {
        _exitCode = EXIT_CRASHED;
        processError();
        break;
    }
    }
}

void TarsnapTask::processError()
{
    LOG << tr("Task %1 finished with error %2 (%3) occured "
              "(exit code %4):\n[%5 %6]\n%7\n")
               .arg(_uuid.toString())
               .arg(_process->error())
               .arg(_process->errorString())
               .arg(_exitCode)
               .arg(_command)
               .arg(Utils::quoteCommandLine(_arguments))
               .arg(QString(_stdOut + _stdErr).trimmed());
    emit finished(_data, _exitCode, QString(_stdOut), QString(_stdErr));
    emitCanceled();
}

#ifdef QT_TESTLIB_LIB
void TarsnapTask::fake()
{
    _fake = true;
}
#endif
