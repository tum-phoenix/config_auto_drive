#include "environment_logger.h"
#include <cmath>
#include <algorithm>
#include "lms/datamanager.h"
#include <fstream>
#include "lms/math/vertex.h"
bool EnvironmentLogger::initialize() {
    count = 0;
    input = datamanager()->readChannel<street_environment::Environment>(this, "ENVIRONMENT_INPUT");
    directory = getConfig()->get<std::string>("directory");
    return true;
}


bool EnvironmentLogger::deinitialize() {
    return true;
}

bool EnvironmentLogger::cycle() {
    count++;
    for(std::shared_ptr<street_environment::EnvironmentObject> obj : input->objects){
        //TODO check if valid
        const street_environment::RoadLane &lane = obj->getAsReference<const street_environment::RoadLane>();
        std::ofstream stream;
        stream.open(directory+"/"+"lines_"+std::to_string(count)+"_"+std::to_string((int)lane.type())+".csv");
        for(lms::math::vertex2f v : lane.points()){
            stream <<v.x<<","<<v.y<<"\n";
        }
        stream.close();
    }
    return true;
}