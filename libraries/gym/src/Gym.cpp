//
//  Gym.cpp
//  libraries/gym/src
//
//  Created by Wil Bown
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Gym.h"
#include <QtCore/QUuid>

#include <zmq.hpp>
#include <pthread.h>
#include <cassert>
#include <sstream>
#include <string>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#define sleep(n)	Sleep(n)
#endif


#include <QtCore/QLoggingCategory>


static Gym* instance = NULL;        // communicate this to non-class callbacks
static bool broadcastEnabled = false;

// Stores the agents that have connected
std::vector<int> gymhin;

const int MIM_OPEN = 0x0;
const int MIM_CLOSE = 0x1;
const int MIM_DATA = 0x2;

void Gym::GymInProc(int hGymIn, int wMsg, int dwParam1) {
    switch (wMsg) {
        case MIM_OPEN:
            gymhin.push_back(hGymIn);
            instance->gymAgentChange();
            break;
        case MIM_CLOSE:
            for (uint i = 0; i < gymhin.size(); i++) {
                if (gymhin[i] == hGymIn) {
                    gymhin[i] = -1;
                    instance->gymAgentChange();
                }
            }
            break;
        case MIM_DATA: {
            int agent = -1;
            for (uint i = 0; i < gymhin.size(); i++) {
                if (gymhin[i] == hGymIn) {
                    agent = i;
                }
            }
            // if (agent == -1) {
            //     gymhin.push_back(hGymIn);
            //     agent = gymhin.size();
            // }
            
            int action = dwParam1;
            instance->gymReceived(agent, action);        // notify the javascript
            break;
        }
    }
}

void Gym::sendRawMessage(int agent, int raw) {
    if (broadcastEnabled) {
        for (uint i = 0; i < gymhin.size(); i++) {
            if (gymhin[i] != -1) {
                // gymOutShortMsg(gymhin[i], raw);
            }
        }
    } else {
        qDebug() << "Gym::sendRawMessage agent[" << agent << "] raw[" << raw << "]";
        // gymOutShortMsg(gymhin[agent], raw);
    }
}

void Gym::sendMessage(int agent, int observation, float reward, bool done, int info) { // observation = object, info = dictionary
        qDebug() << "Gym::sendRawMessage agent[" << agent << "] observation[" << observation << "] reward[" << reward << "] done[" << done << "] info[" << info << "]";
}

void Gym::gymReceived(int agent, int action) {
    QVariantMap eventData;
    eventData["agent"] = agent;
    eventData["action"] = action;
    emit gymMessage(eventData);
}

void Gym::gymAgentChange() {
    emit gymReset();
}

void *worker_routine(void *arg)
{
    //  Prepare our context and sockets
    zmq::context_t *context = (zmq::context_t *)arg;
    zmq::socket_t socket(*context, ZMQ_REP);
    socket.bind("tcp://127.0.0.1:5558");

    qDebug() << "Gym::worker connected?[" << socket.connected() << "]";

    while (true) {
        //  Wait for next request from client
        zmq::message_t request;
        socket.recv(&request);

        // qDebug() << "Gym::Received request[" << std::string(static_cast<char*>(request.data()), request.size()) << "]";
        // qDebug() << "Gym::Received request[" << request.size() << "]";

        float action1, action2, action3;
        
        std::istringstream iss(static_cast<char*>(request.data()));
        iss >> action1 >> action2 >> action3;

        qDebug() << "Received request [" << action1 << "] [" << action2 << "] [" << action3 << "]";


        //  Do some 'work'
        sleep(1);

        //  Send reply back to client
        // zmq::message_t reply(5);
        // memcpy(reply.data(), "World", 5);
        zmq::message_t reply(request.size());
        memcpy(reply.data(), request.data(), request.size());
        socket.send(reply);
    }
    return (NULL);
}

// Global ZeroMQ context
static zmq::context_t context(1);

void Gym::GymSetup() {
    gymhin.clear();
    
    qDebug() << "Gym::GymSetup";
    // test
    // GymInProc(0x34552, MIM_OPEN, 0x0);
    // GymInProc(0x34552, MIM_DATA, 0x2);

    // int major, minor, patch;
    // zmq_version (&major, &minor, &patch);
    // qDebug() << "Current 0MQ version is " << major << "." << minor << "." << patch;

    //  Launch pool of ZeroMQ worker threads
    for (int thread_nbr = 0; thread_nbr < 1; thread_nbr++) {
        pthread_t worker;
        pthread_create(&worker, NULL, worker_routine, (void *)&context);
    }
    qDebug() << "Gym::zmq worker threads created";
}

void Gym::GymCleanup() {
    qDebug() << "Gym::GymCleanup";
    // test
    // GymInProc(0x34552, MIM_CLOSE, 0x0);

    for (uint i = 0; i < gymhin.size(); i++) {
        if (gymhin[i] != -1) {
            // gymInStop(gymhin[i]);
            // gymInClose(gymhin[i]);
        }
    }
    gymhin.clear();
}

Gym::Gym() {
    instance = this;
    GymSetup();
}

Gym::~Gym() {
    GymCleanup();
}


// Public scripting (Q_INVOKABLE)

void Gym::registerActor(QUuid actor) {
    qDebug() << "Gym::registerActor actor[" << actor << "]";
    // match actor ID with port number and save to some persistant storage
}

void Gym::sendRawGymMessage(int agent, int raw) {
    sendRawMessage(agent, raw);
}

void Gym::sendGymMessage(int agent, int observation, float reward, bool done, int info) {
    sendMessage(agent, observation, reward, done, info);
}

void Gym::resetAgents() {
    // Send done = true
}

QStringList Gym::listGymAgents() {
    QStringList rv;
    for (uint i = 0; i < gymhin.size(); i++) {
        rv.append("25");
    }
    return rv;
}

void Gym::broadcastEnable(bool enable) {
    broadcastEnabled = enable;
}
