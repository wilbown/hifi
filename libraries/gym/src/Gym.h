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
#include <QtCore/QUuid>
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
    void gymReceived(int agent, int action);  // revieved message from agent, relay to actor script (using emit gymMessage)
    void gymAgentChange();  // relay agent change to actor script (using emit gymReset)
    void sendRawMessage(int agent, int raw);  // send a raw message to GYM
    void sendMessage(int agent, int observation, float reward, bool done, int info);  // send a formatted message to GYM

private:
    void GymInProc(int hGymIn, int wMsg, int dwParam1);
    void GymSetup();
    void GymCleanup();

signals:
    void gymMessage(QVariantMap eventData);
    void gymReset();

public slots:

    /**jsdoc
     * Register an actor entity to recieve messages from an agent
     * @function Gym.registerActor
     * @param {uuid} actor - entityID of actor
     */
    Q_INVOKABLE void registerActor(QUuid actor);

    /**jsdoc
     * Send raw message to a particular agent.
     * @function Gym.sendRawGymMessage
     * @param {number} agent - agentID
     * @param {number} raw - raw GYM message.
     */
    Q_INVOKABLE void sendRawGymMessage(int agent, int raw);

    /**jsdoc
     * Send GYM message to a particular agent.
     * @function Gym.sendGymMessage
     * @param {number} agent
     * @param {number} observation - 
     * @param {number} reward - 
     * @param {number} done - 
     * @param {number} info - 
     */
    Q_INVOKABLE void sendGymMessage(int agent, int observation, float reward, bool done, int info);

    /**jsdoc
     * Tell agents to reset training.
     * @function Gym.resetAgents
     */
    Q_INVOKABLE void resetAgents();

    /**jsdoc
     * Get a list of available agents.
     * @function Gym.listGymAgents
     * @returns {string[]}
     */
    Q_INVOKABLE QStringList listGymAgents();

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
