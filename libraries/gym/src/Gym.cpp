//
//  Gym.cpp
//  libraries/gym/src
//
//  Created by Wil Bown on 12/21/2018
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Gym.h"
#include <QtCore/QLoggingCategory>

#include <sstream>


static Gym* instance = NULL;        // communicate this to non-class callbacks
static bool broadcastEnabled = false;

void Gym::gymAgentChange(int agent) {
    emit onGymAgentChange(agent);
}

void Gym::sendMessage(int agent, QVariantMap message) {
    if (broadcastEnabled) {
    } else {
        // qDebug() << "Gym::sendMessage agent[" << agent << "] observation[" << message["observation"] << "] reward[" << message["reward"] << "] done[" << message["done"] << "] info[" << message["info"] << "]";
        gymThreadWorkers[agent]->handleMessage(message);
    }
}

void Gym::gymMessage(int agent, QVariantMap message) {
    QVariantMap eventData;
    eventData["agent"] = agent;
    eventData["message"] = message;
    emit onGymMessage(eventData);
}



// GymThread Class
void GymThread::run() {
    qDebug() << "GymThread::run";

    zmq::socket_t socket(*zmq_context, ZMQ_REP);
    socket.bind("tcp://127.0.0.1:" + std::to_string(port));
    qDebug() << "GymThread::run socket.connected?[" << socket.connected() << "] port[" << port << "]";

    while (!quit) {
        // qDebug() << "GymThread::run LOOP";

        //  Wait for next request from client
        zmq::message_t request;
        socket.recv(&request);
        // if (!socket.waitForConnected(Timeout)) {
        //     emit error(socket.error(), socket.errorString());
        //     return;
        // }

        mutex.lock();


        // qDebug() << "GymThread::run received request[" << QString::fromStdString(std::string(static_cast<char*>(request.data()), request.size())) << "]";
        // qDebug() << "GymThread::run received request[" << request.size() << "]";
        QString qstr = QString::fromUtf8(static_cast<char*>(request.data()), request.size());
        // qDebug() << "GymThread::run received request[" << qstr << "]";

        // float action1, action2, action3;
        // std::istringstream iss(static_cast<char*>(request.data()));
        // iss >> action1 >> action2 >> action3;
        // // qDebug() << "GymThread::run received request [" << action1 << "] [" << action2 << "] [" << action3 << "]";
        // int action;
        // std::istringstream iss(static_cast<char*>(request.data()));
        // iss >> action;
        QStringList action = qstr.split(" ", QString::SkipEmptyParts);
        // qDebug() << "GymThread::run received request [" << action << "]";



        // TODO keep track when agent sends first message
        // if (gymThreadWorkers.empty()) {
        //     qDebug() << "Gym::zmq-worker::No actors available";
        //     continue;
        // }
        // instance->gymAgentChange(port); //only on first message


        // QVariantList action;
        // action << action1 << action2 << action3;
        QVariantMap message;
        message["action"] = action;
        instance->gymMessage(port, message);        // notify the javascript

        // qDebug() << "GymThread::run emit action";

        cond.wait(&mutex);

        // Grab state to send back to client
        QString reply_qstr = (state["done"].toBool()) ? "1" : "0";
        // if (state["done"].toBool()) reply_qstr = "1"; else reply_qstr = "0";
        reply_qstr = reply_qstr + " " + state["reward"].toString();

        QList<QVariant> observation = state["observation"].toList();
        for (QVariant n : observation) {
            reply_qstr = reply_qstr + " "  + n.toString();
        }

        // for (QVariant n : observation) {
        //     if (n.userType() == QMetaType::QVariantList) {
        //         QList<QVariant> m = n.toList();
        //         for (QVariant o : m) {
        //             reply_qstr = reply_qstr + " "  + o.toString();
        //         }
        //     } else {
        //         reply_qstr = reply_qstr + " "  + n.toString();
        //     }
        // }

        // qDebug() << "GymThread::run sending reply_qstr [" << reply_qstr << "]";

        QByteArray ba = reply_qstr.toLocal8Bit();
        const char *reply_cstr = ba.data();


        // QThread::msleep(10); // add arbitrary delay before replying to agent

        // std::ostringstream oss;
        // oss << action1 << " " << action2 << " " << action3 << " " << action3 << " " << action3 << " 0";
        // std::string reply_str(oss.str());
        // qDebug() << "GymThread::run sending reply [" << QString::fromStdString(reply_str) << "]";

        // Send reply back to client
        // zmq::message_t reply(5);
        // memcpy(reply.data(), "World", 5);
        // zmq::message_t reply(request.size());
        // memcpy(reply.data(), request.data(), request.size());
        // zmq::message_t reply(reply_str.length());
        // memcpy(reply.data(), reply_str.c_str(), reply_str.length());
        zmq::message_t reply(strlen(reply_cstr));
        memcpy(reply.data(), reply_cstr, strlen(reply_cstr));
        socket.send(reply);
        
        mutex.unlock();
    }
}
void GymThread::handleMessage(QVariantMap message) {
    // qDebug() << "GymThread::handleMessage: " << message;
    QMutexLocker locker(&mutex);
    // Loop through message and add any new keys to state, replace keys if they already exist in state
    QMapIterator<QString, QVariant> i(message);
    while (i.hasNext()) {
        i.next(); state[i.key()] = i.value();
    }
    cond.wakeOne();
}
// GymThread::GymThread(QObject *parent) : QThread(parent), quit(false) {}
GymThread::GymThread(QObject *parent, zmq::context_t *_zmq_context, int _port) : QThread(parent) {
    // mutex.lock();
    quit = false;
    port = _port;
    zmq_context = _zmq_context;
    // mutex.unlock();
}
GymThread::~GymThread() {
    mutex.lock();
    quit = true;
    cond.wakeOne();
    mutex.unlock();
    wait();
}


// Global ZeroMQ context
static zmq::context_t context(1);

void Gym::GymSetup() {
    qDebug() << "Gym::GymSetup";
    gymThreadWorkers.clear();

    // Check ZeroMQ version
    // int major, minor, patch;
    // zmq_version (&major, &minor, &patch);
    // qDebug() << "Current 0MQ version is " << major << "." << minor << "." << patch;

    
    // TODO Launch pool of ZeroMQ worker threads
    for (int p = 5558; p < 5559; p++) {
        gymThreadWorkers[p] = new GymThread(this, &context, p);

        // connect(&gymThread, SIGNAL(newFortune(QString)), instance, SLOT(showFortune(QString)));
        // connect(&gymThread, SIGNAL(error(int,QString)), instance, SLOT(displayError(int,QString)));

        // GymThread *gymThreadWorker = new GymThread(this);
        // connect(gymThreadWorker, &GymThread::resultReady, this, &Gym::handleResults);

        connect(gymThreadWorkers[p], &GymThread::finished, gymThreadWorkers[p], &QObject::deleteLater);
        gymThreadWorkers[p]->start();
    }

    qDebug() << "Gym::GymWorker threads created";
}
void Gym::GymCleanup() {
    qDebug() << "Gym::GymCleanup";
    gymThreadWorkers.clear();
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
    return gymThreadWorkers.keys();
}

void Gym::sendGymMessage(int agent, QVariantMap message) {
    sendMessage(agent, message);
}

void Gym::broadcastEnable(bool enable) {
    broadcastEnabled = enable;
}
