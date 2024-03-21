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
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include "BlockChain.h"
#include "authentication.h"
#include "ConsensusMode.h"
#include "CommunicateMode.h"
#include "ProcessDataMode.h"
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>


//创世块建立
void createBlockChainTable(QSqlDatabase& db, int port,QList<int> intList,int Consensus_Num)
{
    // Open SQLite database


    // Create BlockChain table
    QSqlQuery query(db);
    QString database =  QString("blockchain_%1").arg(port);
    if (!query.exec(QString("DROP TABLE IF EXISTS %1").arg(database))) {
            qDebug() << "Error: Failed to drop BlockChain table:";
            //db.close();
            return;
        }
    else {
            // qDebug() << "BlockChain table dropped successfully!";
        }
    if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1("
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                 "trust_value FLOAT, "
                                 "block_depth INTEGER, "
                                 "is_consensus_node INTEGER,"
                                 "is_bad_node INTEGER,"
                                 "x INTEGER,"
                                 "y INTEGER)").arg(database))) {
            qDebug() << "Error: Failed to create BlockChain table:";
            //db.close();
            return;
        }
    else {
           // qDebug() << "BlockChain table created successfully!";
        }
    // Close the database connection
    QString insertQuery = QString("INSERT INTO %1 (id, trust_value, block_depth, is_consensus_node, is_bad_node, x, y) "
                                      "VALUES (:id, :trust_value, :block_depth, :is_consensus_node, :is_bad_node, :x, :y)").arg(database);
        // Example data to be inserted
        query.prepare(insertQuery);
    Point p;
    p.x = 1;
    p.y = 1;
    int clientCount =  intList.size();

    for (int i = 0; i < clientCount; i++) {

        query.bindValue(":id", intList[i]);
        query.bindValue(":trust_value", 10); // Example trust value
        query.bindValue(":block_depth", 0); // Example block depth
        query.bindValue(":is_consensus_node", 0); // Example is consensus node
        query.bindValue(":is_bad_node", 0);
        query.bindValue(":x", p.x); // X value from clients array
        query.bindValue(":y", p.y); // Y value from clients array
        databaseMutex.lock();
        query.exec();
        query.finish();
        databaseMutex.unlock();
        // Execute the query
//        if (!query.exec()) {
//            qDebug() << "Error: Failed to insert data into BlockChain table:" ;
//        } else {
//            qDebug() << "Data inserted successfully into BlockChain table!";
//        }
    }


    //设置共识节点
    for(int j=0;j<Consensus_Num;j++)
    {

        QString updateQuery = QString("UPDATE %1 SET "
                              "is_consensus_node = :is_consensus_node WHERE id = :id").arg(database);

        // 准备查询
        query.prepare(updateQuery);

        // 绑定值到占位符
        query.bindValue(":id", intList[j]);

        query.bindValue(":is_consensus_node", 1);

        // 执行更新
        databaseMutex.lock();
        if (!query.exec()) {
            qDebug() << "错误：无法在 BlockChain 表中更新数据：" ;
        }
        query.finish();
        databaseMutex.unlock();

    }


 /*
        //设置共识节点
        QString updateQuery = QString("UPDATE %1 SET "
                              "is_consensus_node = :is_consensus_node WHERE id = :id").arg(database);

        // 准备查询
        query.prepare(updateQuery);

        // 绑定值到占位符
        query.bindValue(":id", intList[1]);

        query.bindValue(":is_consensus_node", 1);

        // 执行更新
        databaseMutex.lock();
        if (!query.exec()) {
            qDebug() << "错误：无法在 BlockChain 表中更新数据：" ;
        }
        query.finish();
        databaseMutex.unlock();

        //设置共识节点
    updateQuery = QString("UPDATE %1 SET "
                              "is_consensus_node = :is_consensus_node WHERE id = :id").arg(database);

        // 准备查询
        query.prepare(updateQuery);

        // 绑定值到占位符
        query.bindValue(":id", intList[2]);

        query.bindValue(":is_consensus_node", 1);

        // 执行更新
        databaseMutex.lock();
        if (!query.exec()) {
            qDebug() << "错误：无法在 BlockChain 表中更新数据：" ;
        }
        query.finish();
        databaseMutex.unlock();
*/
        //查寻
            if (!query.exec(QString("SELECT * FROM %1").arg(database))) {
                qDebug() << "Error: Failed to fetch data from " << database << " table:" ;
                // db.close();
                return;
            } else {
                qDebug() << "Data from " << database << " table:";
                while (query.next()) {
                    int id = query.value(0).toInt();
                    float trust_value = query.value(1).toFloat();
                    int block_depth = query.value(2).toInt();
                    int is_consensus_node = query.value(3).toInt();
                    int is_bad_node = query.value(4).toInt();
                    int x = query.value(5).toInt();
                    int y = query.value(6).toInt();

                    qDebug() << "ID:" << id << ", Trust Value:" << trust_value << ", Block Depth:" << block_depth
                             << ", Is Consensus Node:" << is_consensus_node << ", Is BAD Node:" << is_bad_node << ", X:" << x << ", Y:" << y;
                }
            }
            db.commit();

}

//num表示
void createOrClearTableForPort(QSqlDatabase& db,int port) {

    QString portInfoTableName = QString("info_%1").arg(port);
    QString portOutputTableName = QString("output_%1").arg(port);
    QString portReceiveTableName = QString("receive_%1").arg(port);

    // Check if tables exist
    bool portInfoExists = isTableExists(db, portInfoTableName);
    bool portOutputExists = isTableExists(db, portOutputTableName);
    bool portReceiveExists = isTableExists(db, portReceiveTableName);

    QSqlQuery query(db);

    if (portInfoExists) {
        // Clear port_info table
        if (!query.exec(QString("DELETE FROM %1").arg(portInfoTableName))) {
            qDebug() << "Error: Failed to clear port_info table:" ;
           // db.close();
            return;
        }
//        if (!query.exec(QString("DROP TABLE %1").arg(portInfoTableName))) {
//            qDebug() << "Error: Failed to clear port_info table:" ;
//            db.close();
//            return;
//        }
    }
    else {
        // Create port_info table
        if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                 "trust_value FLOAT, "
                                 "block_depth INTEGER)").arg(portInfoTableName))) {
            qDebug() << "Error: Failed to create port_info table:" << query.lastError().text() ;
           // db.close();
            return;
        } else {//

            //qDebug() << "port_info table created successfully!";
        }
    }
    QString database = QString("blockchain_%1").arg(port);
    QSqlQuery insertQuery(db);
    //初始化info_port
    if (!query.exec(QString("SELECT * FROM %1").arg(database))) {
        qDebug() << "Error: Failed to fetch data from " << database << " table:" ;
        // db.close();
        return;
    } else {
        //qDebug() << "Data from " << database << " table:";
        while (query.next()) {
            //                    int id = query.value(0).toInt();
            //                    QString message = query.value(1).toString();
            int id = query.value(0).toInt();
            float trustValue = query.value(1).toFloat();
            int blockDepth = query.value(2).toInt();
            insertQuery.prepare(QString("INSERT INTO %1 (id, trust_value, block_depth) VALUES (:id, :trust_value, :block_depth)").arg(portInfoTableName));
            insertQuery.bindValue(":id", id);
            insertQuery.bindValue(":trust_value", trustValue);
            insertQuery.bindValue(":block_depth", blockDepth);
            databaseMutex.lock();
            if (!insertQuery.exec()) {
                qDebug() << "Error: Failed to insert data into destination_table:" << insertQuery.lastError().text();
                //             db.close();
                databaseMutex.unlock();
                return;
            }
            insertQuery.finish();
            databaseMutex.unlock();
            // qDebug() << "ID:" << id << ", trustValue:" <<trustValue<<",blockDepth"<<blockDepth;
        }
    }
//        //查寻
//            if (!query.exec(QString("SELECT * FROM %1").arg(portInfoTableName))) {
//                    qDebug() << "Error: Failed to fetch data from " << portInfoTableName << " table:" ;
//                    db.close();
//                    return;
//                } else {
//                    qDebug() << "Data from " << portInfoTableName << " table:";
//                    while (query.next()) {
//                        int id = query.value(0).toInt();
//                        float trustValue = query.value(1).toFloat();
//                        int blockDepth = query.value(2).toInt();


//                        qDebug() << "ID:" << id << ", trustValue:" <<trustValue<<",blockDepth"<<blockDepth;
//                    }
//                }
    // Repeat the same process for port_output and port_receive tables...

    if (portOutputExists) {
        if (!query.exec(QString("DELETE FROM %1").arg(portOutputTableName))) {
            qDebug() << "Error: Failed to clear port_output table:" ;
           // db.close();
            return;
        }
//        if (!query.exec(QString("DROP TABLE %1").arg(portInfoTableName))) {
//            qDebug() << "Error: Failed to clear port_info table:" ;
//            db.close();
//            return;
//        }
    } else {
        if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                                 "id INTEGER , "
                                 "message QByteArray,"
                                "hash_result QByteArray "
                                ")").arg(portOutputTableName))) {
            qDebug() << "Error: Failed to create port_output table:" ;
          //  db.close();
            return;
        } else {
            //qDebug() << "port_output table created successfully!";
        }
    }

    if (portReceiveExists) {
        if (!query.exec(QString("DELETE FROM %1").arg(portReceiveTableName))) {
            qDebug() << "Error: Failed to clear port_receive table:" ;
          //  db.close();
            return;
        }
    } else {
        if (!query.exec(QString("CREATE TABLE IF NOT EXISTS %1 ("
                                 "id INTEGER , "
                                 "message QByteArray,"
                                "hash_result QByteArray "
                                ")").arg(portReceiveTableName))) {
            qDebug() << "Error: Failed to create port_receive table:" ;
          //  db.close();
            return;
        } else {
            qDebug() << "port_receive table created successfully!";
        }
    }

    //db.close();
    db.commit();
}


int main(int argc, char *argv[]) {


    QCoreApplication a(argc, argv);
    int control_port;
    control_port = atoi(argv[1]);
     qDebug() << "control_port: "<<control_port;
    // 创建UDP socket
    QUdpSocket udpSocket;
    // 绑定到本地端口
    int port;
    //qDebug() << "Enter a port: ";
    //::cin >> port;
    port= atoi(argv[2]);
    udpSocket.bind(QHostAddress::AnyIPv4, static_cast<quint16>(port));

    Client clients[20];
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    QString database = QString("BlockChain_%1.db").arg(port);
    db.setDatabaseName(database);

        if (!db.open()) {
            qDebug() << "Error: Failed to open database:" ;
            return -1;
        } else {
            //qDebug() << "Database opened successfully!";
        }
        // 从命令行参数中获取字符串
        QString arg3 = QString(argv[3]);

        // 使用正则表达式匹配所有整数部分（包括负数）
        QRegularExpression re("-?\\d+");
        QStringList integers;

        // 迭代匹配的结果，并将整数添加到列表中
        QRegularExpressionMatchIterator matchIter = re.globalMatch(arg3);
        while (matchIter.hasNext()) {
            QRegularExpressionMatch match = matchIter.next();
            QString integer = match.captured();
            integers.append(integer);
        }

        // 将字符串列表中的整数转换为 int 类型
        QList<int> intList;
        for (const QString &str : integers) {
            intList.append(str.toInt());
        }

        // 输出转换后的整数列表
       // qDebug() << "转换后的整数列表：" << intList;
    int Consensus_Num=3;
    createBlockChainTable(db,port,intList,Consensus_Num);//设置创世块

        // Create QSqlQuery instance
        QSqlQuery query(db);

        // Execute query to count records in BlockChain table
        if (!query.exec(QString("SELECT COUNT(*) FROM blockchain_%1").arg(port))) {
            qDebug() << "Error: Failed to count records:" ;
          //  db.close();
            return -1;
        }

        // Retrieve the count
   int clientCount = 0;
   if (query.next()) {
    clientCount = query.value(0).toInt();
    }
    int my_index = -1;
    int count=0;
    if (!query.exec(QString("SELECT * FROM blockchain_%1").arg(port))) {
            qDebug() << "Error: Failed to fetch data from BlockChain table:" << query.lastError().text();
          //  db.close();

        } else {
           // qDebug() << "Data from BlockChain table:";
            while (query.next()) {
                int id = query.value(0).toInt();
                float trust_value = query.value(1).toFloat();
                int block_depth = query.value(2).toInt();
                int is_consensus_node = query.value(3).toInt();
                 int is_bad_node = query.value(4).toInt();
                int x = query.value(5).toInt();
                int y = query.value(6).toInt();
                clients[count].address = QHostAddress("127.0.0.1");
                clients[count].port = id;
                if (port == id) {
                            my_index = count;

                        }
                clients[count].x=x;
                clients[count].y=y;
                clients[count].is_consensus_node=is_consensus_node;
                clients[count].is_bad_node=is_bad_node;
                clients[count].trust_value=trust_value;
                clients[count].block_depth=block_depth;
                clients[count].is_run_node = 0;
                clients[count].is_out_node = 0;
                count++;
            }
        }
    clients[my_index].is_run_node = 1;
    clients[count].address = QHostAddress("127.0.0.1");
    clients[count].port = control_port;
    clients[count].x=0;
    clients[count].y=0;
    clients[count].is_consensus_node=0;
    clients[count].is_bad_node=0;
    clients[count].trust_value=0;
    clients[count].block_depth=0;
    clients[count].is_run_node = 1;
    clients[count].is_out_node = 0;
    clientCount++;
   // update_client(udpSocket, clients, my_index,port,clientCount);
    //createOrClearTableForPort(db,port);//设置自身数据库
    qDebug() << port << "号无人机启动，监听端口" << port;

    //焦建博test
    // 初始化 OpenSSL 库
    initOpenSSL();

    // 生成承诺
    Commitment commitment = generateCommitment();

    // 输出承诺信息
    qDebug() << "Generated commitment:" << commitment.point;

    // 释放 OpenSSL 库
    cleanupOpenSSL();
    //XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    createOrClearTableForPort(db,port);//设置自身数据库
   // changetable(db,port);
    // 创建定时器，广播client信息  ，此处广播
    QTimer sendTimer_clients;
    sendTimer_clients.setInterval(5000); // 5秒发送一次数据

    // 创建定时器 发送业务数据，数据格式定义  ，此处广播
    QTimer sendTimer_transactions;
    sendTimer_transactions.setInterval(5000); // 5秒发送一次数据

    // 创建定时器 发送共识数据，发送评估数据集，此处组播
    QTimer sendTimer_trust_estimated;
    sendTimer_trust_estimated.setInterval(10000); // 10秒发送一次数据

    QObject::connect(&sendTimer_transactions, &QTimer::timeout, [&]() {
            sendData(udpSocket, clients, clientCount, my_index, port,db);
        });

    QTimer sendTimer_trust_consensus;
    sendTimer_trust_consensus.setInterval(7000); // 12秒发送一次数据
    //提出blockchain信息，更新info_和clients信息
    QTimer sendTimer_tongbu;
    sendTimer_tongbu.setInterval(9000); // 1秒发送一次数据

    QTimer sendTimer_gongshi;
    sendTimer_gongshi.setInterval(10000); // 1秒发送一次数据

    QTimer sendTimer_Score;
    sendTimer_Score.setInterval(6000); // 1秒发送一次数据
    // 定时器触发 主观信任数据发送
    //非共识节点发送共识数据给共识节点
     QObject::connect(&sendTimer_trust_estimated, &QTimer::timeout, [&]() {
         sendTrustData(udpSocket, clients, clientCount, my_index, port,db);

     });
    //共识节点之间相互发送共识数据
     QObject::connect(&sendTimer_gongshi, &QTimer::timeout, [&]() {
         gongshi(udpSocket, clients, clientCount, my_index, port,db);

     });
    //处理接收到的数据
     QObject::connect(&udpSocket, &QUdpSocket::readyRead, [&]() {
             processData(udpSocket, clients, clientCount, my_index, port,db);
         });

    //共识节点将共识数据发送给非共识节点
     QObject::connect(&sendTimer_trust_consensus, &QTimer::timeout, [&]() {
             consensusData(udpSocket, clients, clientCount, my_index, port,db);
         });
        //提出blockchain信息，更新info_和clients信息
     QObject::connect(&sendTimer_tongbu, &QTimer::timeout, [&]() {
             tongbu( clients, my_index, port,db);
         });
     QObject::connect(&sendTimer_Score, &QTimer::timeout, [&]() {
         checkAndUpdateTrustValue(db, clients, clientCount, my_index, port);

     });
     QObject::connect(&sendTimer_clients, &QTimer::timeout, [&]() {
         update_client(udpSocket, clients, my_index,port,clientCount);

      });
    // 启动定时器， 考虑发数据给控制台
    sendTimer_trust_estimated.start();

    sendTimer_transactions.start();

    sendTimer_trust_consensus.start();

    sendTimer_tongbu.start();

    sendTimer_gongshi.start();

    sendTimer_Score.start();

    sendTimer_clients.start();
    return a.exec();
}
