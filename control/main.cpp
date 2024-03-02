#include <QUdpSocket>
#include <QTimer>
#include <QCoreApplication>
#include <QInputDialog>
#include <QDataStream>
#include <iostream>
#include <QThread>
#include <QtMath>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>
#include <QCryptographicHash>
#include <QMutex>
#include "BlockChain.h"
QByteArray hashByteArray(const QByteArray& data, QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Md5) {
    // 创建哈希对象
    QCryptographicHash hash(hashAlgorithm);

    // 添加要哈希的数据
    hash.addData(data);

    // 返回哈希结果
    return hash.result();
}
void sendData(QUdpSocket& udpSocket) {
    Message temp;

    Client xx;
    temp.is_val = 2;
    temp.Source_ID = xx;
    temp.ID2 = xx;
    temp.ID1 = xx;

    temp.buffer = "8888";

    xx.x = 1;
    xx.y = 1;
    temp.hops=0;
    temp.hash_result = hashByteArray(temp.buffer, QCryptographicHash::Sha256);
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);

    stream <<temp.is_val<< temp.Source_ID.port
           << temp.ID1.port
           << temp.ID2.port<< temp.ID2.x << temp.ID2.y
           << temp.buffer<<temp.hash_result<<temp.hops;

    udpSocket.writeDatagram(byteArray, QHostAddress("127.0.0.1"), 8081);
}
int main(int argc, char *argv[])
{

    QCoreApplication a(argc, argv);

    // 创建UDP socket
    QUdpSocket udpSocket;
    // 绑定到本地端口
    int port;
//    qDebug() << "Enter a port: ";
//    std::cin >> port;
    port = 1234;
    udpSocket.bind(QHostAddress::AnyIPv4, static_cast<quint16>(port));
    // 暂停程序执行 5 秒钟
    //QThread::sleep(5);
    sendData(udpSocket);
    qDebug() << "send message";
    return a.exec();
}
