#include "backuptask.h"

#include <QFileInfo>

BackupTask::BackupTask():_uuid(QUuid::createUuid()), _optionPreservePaths(true), _skipFilesSize(0),
    _status(TaskStatus::Initialized)
{

}

BackupTask::~BackupTask()
{

}

QStringList BackupTask::getExcludesList()
{
    QStringList skipList;
    if(_job)
    {
        _skipFilesSize = 0;
    }
    else
    {
        QSettings settings;
        if(settings.value("app/skip_files_enabled", false).toBool())
            _skipFilesSize = settings.value("app/skip_files_value", 0).toInt();
        else
            _skipFilesSize = 0;
    }

    if(_skipFilesSize)
    {
        foreach (QUrl url, urls()) {
            QFileInfo file(url.toLocalFile());
            if(file.isFile())
            {
                if(file.size() >= (_skipFilesSize*1024*1024))
                {
                    skipList << url.toLocalFile();
                }
            }
            else if(file.isDir())
            {
                QStack<QDir> dirStack;
                dirStack.push(QDir(file.absoluteFilePath()));
                while (!dirStack.isEmpty())
                {
                    QDir dir(dirStack.pop());
                    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks);
                    foreach (QFileInfo entry, dir.entryInfoList()) {
                        if(entry.isFile())
                        {
                            if(entry.size() >= (_skipFilesSize*1024*1024))
                                skipList << entry.absoluteFilePath();
                        }
                        else if(entry.isDir())
                        {
                            dirStack.push(QDir(entry.absoluteFilePath()));
                        }
                    }
                }
            }
        }
    }

    return skipList;
}

