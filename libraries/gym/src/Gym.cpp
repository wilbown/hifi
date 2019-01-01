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
#include <QtCore/QThread>
#include <QtCore/QLoggingCategory>

// #include <zmq.hpp>
// #include <pthread.h>
// #include <cassert>
#include <sstream>
// #include <string>
// #include <iostream>
// #ifndef _WIN32
// #include <unistd.h>
// #else
// #include <windows.h>
// #define sleep(n)	Sleep(n)
// #endif


static Gym* instance = NULL;        // communicate this to non-class callbacks
static bool broadcastEnabled = false;

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

void GymWorker::doAgentListen(zmq::context_t *context) {
    qDebug() << "GymWorker::doAgentListen started";
    int port = 5558;

    zmq::socket_t socket(*context, ZMQ_REP);
    socket.bind("tcp://127.0.0.1:" + std::to_string(port));

    qDebug() << "GymWorker::doAgentListen socket.connected?[" << socket.connected() << "] port[" << port << "]";


    while (true) {
        //  Wait for next request from client
        zmq::message_t request;
        socket.recv(&request);

        // qDebug() << "GymWorker::doAgentListen received request[" << std::string(static_cast<char*>(request.data()), request.size()) << "]";
        // qDebug() << "GymWorker::doAgentListen received request[" << request.size() << "]";

        float action1, action2, action3;

        std::istringstream iss(static_cast<char*>(request.data()));
        iss >> action1 >> action2 >> action3;

        qDebug() << "GymWorker::doAgentListen received request [" << action1 << "] [" << action2 << "] [" << action3 << "]";

        // if (gymagents.empty()) {
        //     qDebug() << "Gym::zmq-worker::No actors available";
        //     continue;
        // }
        // if(gymagents.find(uuid) != gymagents.end())

        instance->gymMessage(port, action1);        // notify the javascript
        // qDebug() << "Gym::zmq-worker::test return [" << test << "]";

        // instance->gymAgentChange(); //only on first connection

        //  Do some 'work'
        QThread::sleep(1);

        //  Send reply back to client
        // zmq::message_t reply(5);
        // memcpy(reply.data(), "World", 5);
        zmq::message_t reply(request.size());
        memcpy(reply.data(), request.data(), request.size());
        socket.send(reply);
    }



    QString result;
    /* ... here is the expensive or blocking operation ... */
    result = "from GymWorker::doAgentListen testing...";
    emit resultReady(result);
}

void Gym::handleResults(const QString &result) {
    qDebug() << "Gym::handleResults: " << result;
}


// Global ZeroMQ context
static zmq::context_t context(1);
QThread gymWorkerThread; // TODO make this a thread pool

void Gym::GymSetup() {
    qDebug() << "Gym::GymSetup";

    gymagents.clear();

    // test
    // GymInProc(0x34552, MIM_OPEN, 0x0);
    // GymInProc(0x34552, MIM_DATA, 0x2);

    // Check ZeroMQ version
    // int major, minor, patch;
    // zmq_version (&major, &minor, &patch);
    // qDebug() << "Current 0MQ version is " << major << "." << minor << "." << patch;

    //  Launch pool of ZeroMQ worker threads
    // int port = portIter;
    // portIter++;

    GymWorker *worker = new GymWorker;
    worker->moveToThread(&gymWorkerThread);
    connect(&gymWorkerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(instance, &Gym::startAgentListener, worker, &GymWorker::doAgentListen);
    connect(worker, &GymWorker::resultReady, instance, &Gym::handleResults);
    gymWorkerThread.start();
    emit startAgentListener(&context);

    qDebug() << "Gym::GymWorker threads created";
}
void Gym::GymCleanup() {
    qDebug() << "Gym::GymCleanup";
    
    gymWorkerThread.quit();
    gymWorkerThread.wait();

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
