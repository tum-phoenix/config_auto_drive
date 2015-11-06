#ifndef CAR_TO_MATLAB_H
#define CAR_TO_MATLAB_H

#include "lms/module.h"
#include "comm/senseboard.h"
#include "lms/extra/time.h"
#include "sensor_utils/car.h"
#include "socket_connection/socket_server.h"
class CarToMatlab : public lms::Module,public socket_connection::SocketListener {
    struct DeltaState{
        DeltaState():valid(false),x(0),y(0),phi(0){
        }
        bool valid;
        float x;
        float y;
        float phi;
    };

public:
    bool initialize() override;
    bool deinitialize() override;
    bool cycle() override;
    void receivedMessage(socket_connection::SocketConnector &from, char* buff, int bytesRead);
    void disconnected(const socket_connection::SocketConnector &disconnected);
    void connected(const socket_connection::SocketConnector &connected);


private:
    lms::ReadDataChannel<sensor_utils::Car> car;
    socket_connection::SocketServer *server;
};

#endif /* CAR_TO_MATLAB_H */
