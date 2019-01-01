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

#include <sstream>


static Gym* instance = NULL;        // communicate this to non-class callbacks
static bool broadcastEnabled = false;

// Stores the agents that have connected
QList<int> gymagents;

void Gym::gymAgentChange(int agent) {
    emit onGymAgentChange(agent);
}

void Gym::sendMessage(int agent, QVariantMap message) {
    if (broadcastEnabled) {
    } else {
        qDebug() << "Gym::sendMessage agent[" << agent << "] observation[" << message["observation"] << "] reward[" << message["reward"] << "] done[" << message["done"] << "] info[" << message["info"] << "]";
        QVariantMap eventData;
        eventData["agent"] = agent;
        eventData["message"] = message;
        emit onMessage(eventData);
    }
}

void Gym::gymMessage(int agent, QVariantMap message) {
    QVariantMap eventData;
    eventData["agent"] = agent;
    eventData["message"] = message;
    emit onGymMessage(eventData);
}

void GymWorker::doAgentListen(zmq::context_t *context) {
    qDebug() << "GymWorker::doAgentListen";

    zmq::socket_t socket(*context, ZMQ_REP);
    socket.bind("tcp://127.0.0.1:" + std::to_string(port));

    qDebug() << "GymWorker::doAgentListen socket.connected?[" << socket.connected() << "] port[" << port << "]";


    // while (!quit) {
        qDebug() << "GymWorker::doAgentListen LOOP";
        //  Wait for next request from client
        zmq::message_t request;
        socket.recv(&request);

        // qDebug() << "GymWorker::doAgentListen received request[" << std::string(static_cast<char*>(request.data()), request.size()) << "]";
        // qDebug() << "GymWorker::doAgentListen received request[" << request.size() << "]";

        float action1, action2, action3;

        std::istringstream iss(static_cast<char*>(request.data()));
        iss >> action1 >> action2 >> action3;

        qDebug() << "GymWorker::doAgentListen received request [" << action1 << "] [" << action2 << "] [" << action3 << "]";

        // TODO keep track when agent sends first message
        // if (gymagents.empty()) {
        //     qDebug() << "Gym::zmq-worker::No actors available";
        //     continue;
        // }
        // if(gymagents.find(uuid) != gymagents.end())
        // gymagents.push_back(port);
        // instance->gymAgentChange(port); //only on first message

        QVariantList action;
        action << action1 << action2 << action3;
        QVariantMap message;
        message["action"] = action;

        instance->gymMessage(port, message);        // notify the javascript

        QThread::sleep(1);
        // cond.wait(&mutex);

        mutex.lock();
        // TODO Grab state to send back to client
        qDebug() << "GymWorker::doAgentListen read state [" << state["testdata"] << "]";
        mutex.unlock();

        //  Send reply back to client
        // zmq::message_t reply(5);
        // memcpy(reply.data(), "World", 5);
        zmq::message_t reply(request.size());
        memcpy(reply.data(), request.data(), request.size());
        socket.send(reply);
    // }
}
void GymWorker::handleMessage(QVariantMap eventData) {
    qDebug() << "GymWorker::handleMessage: " << eventData;
    mutex.lock();
    state["testdata"] = eventData["agent"];
    // cond.wakeOne();
    mutex.unlock();
}
// GymWorker::~GymWorker() {
//     mutex.lock();
//     quit = true;
//     cond.wakeOne();
//     mutex.unlock();
// }


// Global ZeroMQ context
static zmq::context_t context(1);
QThread gymWorkerThread;

void Gym::GymSetup() {
    qDebug() << "Gym::GymSetup";

    gymagents.clear();

    // Check ZeroMQ version
    // int major, minor, patch;
    // zmq_version (&major, &minor, &patch);
    // qDebug() << "Current 0MQ version is " << major << "." << minor << "." << patch;

    // TODO Launch pool of ZeroMQ worker threads
    // int port = portIter;
    // portIter++;

    GymWorker *worker = new GymWorker;
    worker->port = 5558;
    worker->moveToThread(&gymWorkerThread);
    connect(&gymWorkerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(instance, &Gym::startAgentListener, worker, &GymWorker::doAgentListen);
    connect(instance, &Gym::onMessage, worker, &GymWorker::handleMessage);
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
