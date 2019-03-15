#include "../warnings-disable.h"

WARNINGS_DISABLE
#include <QNetworkAccessManager>
#include <QThread>
WARNINGS_ENABLE

#include "../compile-valgrind-test.h"
#include "../console-test.h"

static void pl_nothing()
{
}

static void pl_networkaccessmanager()
{
    T_TEST_CONSOLE_BEGIN

    QNetworkAccessManager *nam = new QNetworkAccessManager();
    QThread::msleep(200);
    delete nam;

    T_TEST_CONSOLE_END
}

static void pl_networkaccessmanager_status()
{
    T_TEST_CONSOLE_BEGIN

    QNetworkAccessManager *nam = new QNetworkAccessManager();

    QThread::msleep(200);
    nam->networkAccessible();
    QThread::msleep(200);

    delete nam;

    T_TEST_CONSOLE_END
}

static void pl_networkaccessmanager_repeated()
{
    T_TEST_CONSOLE_BEGIN

    // QNetworkAccessManger has an unpredictable interactions with dbus,
    // causing variable memory leaks.  This is my attempt at catching them.
    for(int i = 0; i < 10; i++)
    {
        pl_networkaccessmanager_status();
        QThread::msleep(200);
    }

    T_TEST_CONSOLE_END
}

// clang-format off
T_TEST_BEGIN
    MEMLEAKTEST(pl_nothing),
    MEMLEAKTEST(pl_networkaccessmanager),
    MEMLEAKTEST(pl_networkaccessmanager_status),
    MEMLEAKTEST(pl_networkaccessmanager_repeated)
T_TEST_END