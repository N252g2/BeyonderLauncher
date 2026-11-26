#pragma once

#include <QObject>
#include <QString>

#include <tasks/Task.h>

class ServerPingTask : public Task {
    Q_OBJECT
   public:
    explicit ServerPingTask(QString domain, int port) : Task(), m_domain(domain), m_port(port) {}
    ~ServerPingTask() override = default;
    int m_outputOnlinePlayers = -1;
    int m_outputMaxPlayers = -1;   // Beyonder Launcher: also report capacity
    qint64 m_outputLatencyMs = -1; // and round-trip time of the status query

   private:
    QString m_domain;
    int m_port;

   protected:
    virtual void executeTask() override;
};
