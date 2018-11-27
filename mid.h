#ifndef MID_H
#define MID_H

#include <QObject>
#include <QMap>
#include <QThread>
#include <QVariantMap>
#include "client.h"

class mid : public QObject
{
    Q_OBJECT
public:
    explicit mid(QObject *parent = nullptr);
    ~mid();

   Q_INVOKABLE void connectCall(const QString &id);
   Q_INVOKABLE void snd_msg(const QString &id, const QString &msg);

signals:
    void connectClicked(const QString &id);
    void netStatusChange(int net_status);
    void peer_info_change(const QVariantMap &peer_info);
    void get_talk_msg(QString msg_id, QString msg);

public slots:
    void thread_finished();

private:
    QThread *workThread;
    client *net;
};

#endif // MID_H
