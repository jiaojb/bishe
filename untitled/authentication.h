//#ifndef AUTHENTICATION_H
//#define AUTHENTICATION_H
//#include <openssl/ec.h>
//#include <openssl/ecdsa.h>
//#include <openssl/obj_mac.h>
//#include <openssl/evp.h>
//#include <QDebug>
// 定义承诺结构体
//struct Commitment {
//    EC_POINT* point;
//};

// 初始化 OpenSSL 库
//void initOpenSSL() {
//    OpenSSL_add_all_algorithms();
//}

// 释放 OpenSSL 库
//void cleanupOpenSSL() {
//    EVP_cleanup();
//}

// 生成承诺的函数
//Commitment generateCommitment() {
//    Commitment commitment;

//    // 创建 EC_KEY 对象
//    EC_KEY* ecKey = EC_KEY_new_by_curve_name(NID_secp256k1);
//    if (!ecKey) {
//        qDebug() << "Error: Failed to create EC_KEY object";
//        return commitment;
//    }

//    // 生成密钥对
//    if (EC_KEY_generate_key(ecKey) != 1) {
//        qDebug() << "Error: Failed to generate key pair";
//        EC_KEY_free(ecKey);
//        return commitment;
//    }

//    // 获取公钥
//    const EC_POINT* publicKey = EC_KEY_get0_public_key(ecKey);
//    if (!publicKey) {
//        qDebug() << "Error: Failed to get public key";
//        EC_KEY_free(ecKey);
//        return commitment;
//    }

//    // 复制公钥
//    commitment.point = EC_POINT_dup(publicKey, NULL);
//    if (!commitment.point) {
//        qDebug() << "Error: Failed to copy public key";
//        EC_KEY_free(ecKey);
//        return commitment;
//    }

//    // 释放 EC_KEY 对象
//    EC_KEY_free(ecKey);

//    return commitment;
//}
//#endif // AUTHENTICATION_H
