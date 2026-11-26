#include "LauncherLoginStep.h"

#include "BuildConfig.h"

#include <QNetworkRequest>
#include <QUrl>

#include "Application.h"
#include "Logging.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"
#include "net/Request.h"

LauncherLoginStep::LauncherLoginStep(AccountData* data) : AuthStep(data) {}

QString LauncherLoginStep::describe()
{
    return tr("Fetching Minecraft access token");
}

void LauncherLoginStep::perform()
{
    m_useLoginWithXbox = false;
    sendRequest();
}

void LauncherLoginStep::sendRequest()
{
    auto uhs = m_data->mojangservicesToken.extra["uhs"].toString();
    auto xToken = m_data->mojangservicesToken.token;

    QUrl url;
    QString requestBody;
    if (m_useLoginWithXbox) {
        url = QUrl("https://api.minecraftservices.com/authentication/login_with_xbox");
        requestBody = QString(R"XXX({ "identityToken": "XBL3.0 x=%1;%2" })XXX").arg(uhs, xToken);
    } else {
        url = QUrl("https://api.minecraftservices.com/launcher/login");
        QString mc_auth_template = R"XXX(
{
    "xtoken": "XBL3.0 x=%1;%2",
    "platform": "PC_LAUNCHER"
}
)XXX";
        requestBody = mc_auth_template.arg(uhs, xToken);
    }

    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };

    auto [request, response] = Net::Request::makeByteArray(url, requestBody.toUtf8());
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("LauncherLoginStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "Getting Minecraft access token...";
}

void LauncherLoginStep::onRequestDone(QByteArray* response)
{
    qCDebug(authCredentials()) << *response;
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Reply error:" << m_request->error();
        // Beyonder Launcher: /launcher/login can refuse app IDs that Mojang approved for login_with_xbox; try that next.
        if (m_request->replyStatusCode() == 403 && !m_useLoginWithXbox) {
            qDebug() << "launcher/login refused this app ID, retrying with login_with_xbox";
            m_useLoginWithXbox = true;
            sendRequest();
            return;
        }
        // Both endpoints answer 403 for app IDs Mojang hasn't allow-listed yet.
        if (m_request->replyStatusCode() == 403) {
            emit finished(AccountTaskState::STATE_FAILED_HARD,
                          tr("Minecraft sign-in for %1 is waiting for approval from Mojang. Your Microsoft account is fine; "
                             "please try again later.")
                              .arg(BuildConfig.LAUNCHER_DISPLAYNAME));
            return;
        }
        if (Net::isApplicationError(m_request->error()) && !Net::isServerError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Failed to get Minecraft access token: %1").arg(m_request->errorString()));
        } else {
            m_data->networkError = m_request->error();
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Failed to get Minecraft access token: %1").arg(m_request->errorString()));
        }
        return;
    }

    if (!Parsers::parseMojangResponse(*response, m_data->yggdrasilToken)) {
        qWarning() << "Could not parse login_with_xbox response...";
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse the Minecraft access token response."));
        return;
    }
    emit finished(AccountTaskState::STATE_WORKING, tr("Got Minecraft access token"));
}
