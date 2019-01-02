//
//  GymThread.h
//  libraries/gym/src
//
//  Created by Wil Bown
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_GymThread_h
#define hifi_GymThread_h

#include <QtCore/QObject>

#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include <zmq.hpp>
#include <vector>
#include <string>

/**jsdoc
 * @namespace GymThread
 *
 * @hifi-interface
 * @hifi-server-entity
 */

class GymThread : public QThread
{
    Q_OBJECT

public:
    int port;
private:
    bool quit;
    QMutex mutex;
    QWaitCondition cond;
    QVariantMap state; // The current environment state for the agent

public:
    void handleMessage(QVariantMap eventData); // Send message to an agent (execute with emit onMessage)
    void run() override;

signals:
    // void newFortune(const QString &fortune);
    // void error(int socketError, const QString &message);

public:
    GymThread(QObject *parent = 0);
    virtual ~GymThread();
};

#endif // hifi_GymThread_h
