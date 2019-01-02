//
//  GymThread.cpp
//  libraries/gym/src
//
//  Created by Wil Bown
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GymThread.h"
#include <QtCore/QLoggingCategory>

#include <sstream>

// Global ZeroMQ context
static zmq::context_t context(1);

// GymThread Class
void GymThread::run() {
    qDebug() << "GymThread::run";
    int serverPort = 5558;
    // mutex.lock();
    // int serverPort = port;
    // mutex.unlock();
    // qDebug() << "GymThread::run serverPort: " << serverPort;

    // zmq::socket_t socket(*context, ZMQ_REP);
    // socket.bind("tcp://127.0.0.1:" + std::to_string(serverPort));
    // qDebug() << "GymWorker::doAgentListen socket.connected?[" << socket.connected() << "] port[" << port << "]";

    while (!quit) {
        qDebug() << "GymThread::LOOP";

        // //  Wait for next request from client
        // zmq::message_t request;
        // socket.recv(&request);

        // // qDebug() << "GymWorker::doAgentListen received request[" << std::string(static_cast<char*>(request.data()), request.size()) << "]";
        // // qDebug() << "GymWorker::doAgentListen received request[" << request.size() << "]";

        // float action1, action2, action3;
        // std::istringstream iss(static_cast<char*>(request.data()));
        // iss >> action1 >> action2 >> action3;
        // qDebug() << "GymWorker::doAgentListen received request [" << action1 << "] [" << action2 << "] [" << action3 << "]";


        // if (!socket.waitForConnected(Timeout)) {
        //     emit error(socket.error(), socket.errorString());
        //     return;
        // }


        // TODO keep track when agent sends first message
        // if (gymagents.empty()) {
        //     qDebug() << "Gym::zmq-worker::No actors available";
        //     continue;
        // }
        // if(gymagents.find(uuid) != gymagents.end())
        // gymagents.push_back(port);
        // instance->gymAgentChange(port); //only on first message


        QVariantList action;
        action << 0.3535 << 0.2222 << 0.0;
        QVariantMap message;
        message["action"] = action;
        parent()->instance->gymMessage(serverPort, message);        // notify the javascript

        mutex.lock();
        qDebug() << "GymThread::emit action";

        // QThread::sleep(1);
        cond.wait(&mutex);
        qDebug() << "GymThread::done waiting";

        // TODO Grab state to send back to client
        qDebug() << "GymWorker::doAgentListen read state [" << state["testdata"] << "]";

        mutex.unlock();



        // Send reply back to client
        // // zmq::message_t reply(5);
        // // memcpy(reply.data(), "World", 5);
        // zmq::message_t reply(request.size());
        // memcpy(reply.data(), request.data(), request.size());
        // socket.send(reply);
    }
}
void GymThread::handleMessage(QVariantMap eventData) {
    qDebug() << "GymThread::handleMessage: " << eventData;
    QMutexLocker locker(&mutex);
    // TODO loop through message and add any new keys to state, replace keys if they already exist in state
    state["testdata"] = eventData["agent"];
    cond.wakeOne();
}
GymThread::GymThread(QObject *parent) : QThread(parent), quit(false) { }
GymThread::~GymThread() {
    mutex.lock();
    quit = true;
    cond.wakeOne();
    mutex.unlock();
    wait();
}
