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

static int portIter = 5558; //current used port number

// Stores the agents that have connected
QList<int> gymagents;

void Gym::gymAgentChange(int agent) {
    emit onGymAgentChange(agent);
}

void Gym::sendMessage(int agent, QVariantMap message) { // observation = object, info = dictionary
    if (broadcastEnabled) {
    } else {
        qDebug() << "Gym::sendMessage agent[" << agent << "] observation[" << message["observation"] << "] reward[" << message["reward"] << "] done[" << message["done"] << "] info[" << message["info"] << "]";
    }
}

void Gym::gymMessage(int agent, float action) {
    QVariantMap eventData;
    eventData["agent"] = agent;
    eventData["action"] = action;
    emit onGymMessage(eventData);
}

void *worker_routine(void *arg)
{
    int port = portIter;
    //  Prepare our context and sockets
    zmq::context_t *context = (zmq::context_t *)arg;
    zmq::socket_t socket(*context, ZMQ_REP);
    socket.bind("tcp://127.0.0.1:" + std::to_string(port));

    qDebug() << "Gym::worker connected?[" << socket.connected() << "] port[" << port << "]";

    portIter++;

    while (true) {
        //  Wait for next request from client
        zmq::message_t request;
        socket.recv(&request);

        // qDebug() << "Gym::zmq-worker::Received request[" << std::string(static_cast<char*>(request.data()), request.size()) << "]";
        // qDebug() << "Gym::zmq-worker::Received request[" << request.size() << "]";

        float action1, action2, action3;

        std::istringstream iss(static_cast<char*>(request.data()));
        iss >> action1 >> action2 >> action3;

        qDebug() << "Gym::zmq-worker::Received request [" << action1 << "] [" << action2 << "] [" << action3 << "]";

        // if (gymagents.empty()) {
        //     qDebug() << "Gym::zmq-worker::No actors available";
        //     continue;
        // }
        // if(gymagents.find(uuid) != gymagents.end())

        instance->gymMessage(port, action1);        // notify the javascript
        // qDebug() << "Gym::zmq-worker::test return [" << test << "]";

        // instance->gymAgentChange(); //only on first connection

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
    qDebug() << "Gym::GymSetup";

    gymagents.clear();

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
    
    gymagents.clear();
}

Gym::Gym() {
    instance = this;
    GymSetup();
}
Gym::~Gym() {
    GymCleanup();
}


// Public scripting (Q_INVOKABLE)

QList<int> Gym::listGymAgents() {
    return gymagents;
}

void Gym::sendGymMessage(int agent, QVariantMap message) {
    sendMessage(agent, message);
}

void Gym::broadcastEnable(bool enable) {
    broadcastEnabled = enable;
}
