#pragma once
#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/Request.h"

class LauncherLoginStep : public AuthStep {
    Q_OBJECT

   public:
    explicit LauncherLoginStep(AccountData* data);
    virtual ~LauncherLoginStep() noexcept = default;

    void perform() override;

    QString describe() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    void sendRequest();

    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
    // Beyonder Launcher: fall back to the older login_with_xbox endpoint when /launcher/login refuses our app ID.
    bool m_useLoginWithXbox = false;
};
