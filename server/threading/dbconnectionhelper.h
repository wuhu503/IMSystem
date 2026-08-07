#ifndef DBCONNECTIONHELPER_H
#define DBCONNECTIONHELPER_H

#include <QSqlDatabase>

class DbConnectionHelper
{
public:
    static QSqlDatabase threadLocalConnection();
    static void cleanupCurrentThread();

private:
    DbConnectionHelper() = delete;
};

#endif // DBCONNECTIONHELPER_H
