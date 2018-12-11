#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QVariantMap>
#include <QVariant>
#include <QUdpSocket>
#include <QTime>
#include "QTimer"
#include "zmq.h"



#define SRV_IP            "192.168.2.40"
#define LOGIN_PEER  "tcp://192.168.2.40:9443"
#define PUB_PEER    "tcp://192.168.2.40:9442"

#define  ID_MAX_LEN   (32)
#define  MSG_MAX_LEN  (256)

#define  CHECK_INTERVAL (3*1000)
#define  MAX_TRY_COUNT  (5)

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

    void snd_msg(const QString &id, const QString & data);

    /*监听函数*/
    void poll_thread();

    /*节点间通信消息结构*/
    typedef enum _msg_e
    {
        MSG_HELLO = 0,
        MSG_HEARTBEAT,
        MSG_ACK,
        MSG_DATA,
    }msg_e;

    typedef struct _msg_t
    {
        char node_id[ID_MAX_LEN];
        char self_id[ID_MAX_LEN];
        msg_e   msg_type;
        char content[MSG_MAX_LEN];

        unsigned int length()
        {
            return (sizeof(node_id)+sizeof(self_id)+sizeof(msg_e)+sizeof(content));
        }

        void clear()
        {
            memset(this, 0, sizeof(msg_t));
        }

        void pack(const msg_e &type,const QString &node_id,const QString &self_id,const QString &content="")
        {
            this->clear();
            this->msg_type = type;
            memcpy(this->node_id, node_id.toStdString().c_str(), sizeof(this->node_id));
            memcpy(this->self_id, self_id.toStdString().c_str(), sizeof(this->self_id));
            memcpy(this->content, content.toStdString().c_str(), sizeof(this->content));
        }
    }msg_t;


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

    /*维护的各个节点的通信状态*/
    typedef enum _node_stat_t
    {
        STAT_NOT_START = 0,
        STAT_HELLO,
        STAT_CONNECTED ,
        STAT_FAIL,
    }node_stat_t;

    typedef struct _node_info_t
    {
        QString ip;
        unsigned short port;
        node_stat_t stat;
        QTime expire;
    }node_info_t;
    QMap<QString,node_info_t> node_info_map;

};

#endif // CLIENT_H
