//
//  Gym.h
//  libraries/gym/src
//
//  Created by Wil Bown
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Gym_h
#define hifi_Gym_h

#include <QtCore/QObject>
#include <QAbstractNativeEventFilter>
#include <DependencyManager.h>

// #include <QtCore/QUuid>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include <zmq.hpp>
#include <vector>
#include <string>

/**jsdoc
 * @namespace Gym
 *
 * @hifi-interface
 * @hifi-server-entity
 */

class Gym : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

private:
    void GymSetup();
    void GymCleanup();

public:
    // to actors
    void gymAgentChange(int agent);  // Relay new agent available to actor script (using emit onGymAgentChange)
    void gymMessage(int agent, QVariantMap message);  // Received message from an agent, relay to actor script (using emit onGymMessage)
    // to agent
    void sendMessage(int agent, QVariantMap message);  // Send a formatted message to a particular agent

signals:
    // to actors
    void onGymAgentChange(int agent); // Notify all actors that new agent connected
    void onGymMessage(QVariantMap eventData); // Relay to all actors a new message from an agent
    // to agents
    void onMessage(QVariantMap eventData); // Relay message to agents from an actor
    // for GymWorker
    void startAgentListener(zmq::context_t *context);

public slots:

    /**jsdoc
     * Get a list of available agents.
     * @function Gym.listGymAgents
     * @returns {QList<int>}
     */
    Q_INVOKABLE QList<int> listGymAgents();

    /**jsdoc
     * Send a message to a particular agent.
     * @function Gym.sendGymMessage
     * @param {number} agent
     * @param {QVariantMap} message
     */
    Q_INVOKABLE void sendGymMessage(int agent, QVariantMap message);

    /**jsdoc
     * Broadcast the same observation to all agents.
     * @function Gym.broadcastEnable
     * @param {boolean} enable
     */
    Q_INVOKABLE void broadcastEnable(bool enable);


public:
    Gym();
    virtual ~Gym();
};


class GymWorker : public QObject
{
    Q_OBJECT

public slots:
    void doAgentListen(zmq::context_t *context); // Run the listening loop waiting for agent messages (execute with emit startAgentListener)
    void handleMessage(QVariantMap eventData); // Send message to an agent (execute with emit onMessage)

signals:

public:
    int port;
private:
    bool quit = false;
    QMutex mutex;
    // QWaitCondition cond;
    QVariantMap state; // The current environment state for the agent

public:
    // virtual ~GymWorker();
};

#endif // hifi_Gym_h
