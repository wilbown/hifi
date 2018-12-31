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
// #include <QtCore/QUuid>
#include <QAbstractNativeEventFilter>
#include <DependencyManager.h>

#include <vector>
#include <string>

/**jsdoc
 * @namespace Gym
 *
 * @hifi-interface
 * @hifi-client-entity
 */

class Gym : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    void gymAgentChange(int agent);  // relay new agent to actor script (using emit onGymAgentChange)
    void gymMessage(int agent, float action);  // received message from agent, relay to actor script (using emit onGymMessage)
    void sendMessage(int agent, QVariantMap message);  // send a formatted message to agent

private:
    void GymSetup();
    void GymCleanup();

signals:
    void onGymAgentChange(int agent); // notify new agent connected
    void onGymMessage(QVariantMap message); // relay new message from agent

public slots:

    /**jsdoc
     * Get a list of available agents.
     * @function Gym.listGymAgents
     * @returns {QList<int>}
     */
    Q_INVOKABLE QList<int> listGymAgents();

    /**jsdoc
     * Send GYM message to a particular agent.
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

#endif // hifi_Gym_h
