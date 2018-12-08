#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QVariantMap>
#include <QVariant>
#include <QUdpSocket>
#include "QTimer"
#include "zmq.h"



#define SRV_IP            "47.106.107.103"
#define LOGIN_PEER  "tcp://47.106.107.103:9443"
#define PUB_PEER    "tcp://47.106.107.103:9442"

class client : public QObject
{
    Q_OBJECT
public:
    explicit client(QObject *parent = nullptr);
    ~client();

    static char *s_recv (void *socket)
    {
        char buffer [256];
        int size = zmq_recv (socket, buffer, 255, 0); //attention here! 255 is keng
        if (size == -1)
            return NULL;
        buffer[size] = '\0';

#if (defined (WIN32))
        return strdup (buffer);
#else
        return strndup (buffer, sizeof(buffer) - 1);
#endif

    }

    static char *s_recvmore (void *socket)
    {
        char buffer [256];
        int size = zmq_recv (socket, buffer, 255, ZMQ_RCVMORE); //attention here! 255 is keng
        if (size == -1)
            return NULL;
        buffer[size] = '\0';

#if (defined (WIN32))
        return strdup (buffer);
#else
        return strndup (buffer, sizeof(buffer) - 1);
#endif

    }

    static int s_send (void *socket, const char *string)
    {
        int size = zmq_send (socket, string, strlen (string), 0);
        return size;
    }

    //  Sends string as 0MQ string, as multipart non-terminal
    static int s_sendmore (void *socket, const char *string)
    {
        int size = zmq_send (socket, string, strlen (string), ZMQ_SNDMORE);
        return size;
    }

    static int has_more(void *socket)
    {
        int more = 0;
        size_t len = sizeof(more);
        zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &len);
        return more;
    }

    void snd_msg(const QString &id, const QString & msg);

    /*监听函数*/
    void poll_thread();

signals:
    void srv_status(int net_status);
    void peer_info_change(const QVariantMap & peer_info);
    void get_talk_msg(QString id, QString msg);

public slots:
    void connect_to_srv(const QString &id);
    void node_msg();

private:

    QString id;

    void *context;
    /*登录服务器请求套接字*/
    void *req_sock;
    /*与对端（客户端）聊天套接字*/
    QUdpSocket *talk_sock;
    /*服务器通知其他用户状态套接字*/
    void *sub_sock;

    /*记录服务器发过来的ID与IP对*/
    QVariantMap peer_info;


};

#endif // CLIENT_H
