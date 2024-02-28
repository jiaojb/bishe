#include <QUdpSocket>
#include <QTimer>
#include <QCoreApplication>
#include <QInputDialog>
#include <QDataStream>
#include <iostream>

#include <QtMath>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>
#include <QCryptographicHash>
#include <QMutex>

int main(int argc, char *argv[])
{

    QCoreApplication a(argc, argv);

    // 创建UDP socket
    QUdpSocket udpSocket;
    // 绑定到本地端口
    int port;
    qDebug() << "Enter a port: ";
    std::cin >> port;
    udpSocket.bind(QHostAddress::AnyIPv4, static_cast<quint16>(port));
    return a.exec();
}
