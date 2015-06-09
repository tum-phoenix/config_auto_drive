#include "velocity_controller.h"
#include "lms/datamanager.h"

bool VelocityController::initialize() {
    envInput = datamanager()->readChannel<Environment>(this,"ENVIRONMENT_INPUT");
    controlData = datamanager()->writeChannel<Comm::SensorBoard::ControlData>(this,"CONTROL_DATA");
    config = getConfig();
    lastCall = lms::extra::PrecisionTime::now()-lms::extra::PrecisionTime::fromMillis(config->get<float>("maxDeltaTInMs")*10);
    return true;
}

bool VelocityController::deinitialize() {
    return true;
}

bool VelocityController::cycle() {
    float currentVelocity = controlData->control.velocity.velocity;
    if(!defaultDrive())
        return true;
    float newVeolocity = controlData->control.velocity.velocity;
    logger.debug("cycle") << "defaultDrive-velocity: " << newVeolocity;
    launchControll(newVeolocity,currentVelocity);
    controlData->control.velocity.velocity = newVeolocity;

    logger.debug("cycle") << "end-velocity: " << newVeolocity;
    return true;
}

bool VelocityController::defaultDrive(){
    if(envInput->lanes.size() != 1){
        logger.warn("defaultDrive") << "no valid lane given, laneCount: "<<envInput->lanes.size();
        return false;
    }
    const Environment::RoadLane &middle = envInput->lanes[0];
    float maxSpeed = config->get<float>("maxSpeed",1);
    float minCurveSpeed = config->get<float>("minCurveSpeed",maxSpeed/2);
    float maxCurvation = config->get<float>("maxCurvation",1);
    int partsNeeded = config->get<float>("forcastLength",1)/middle.polarPartLength;
    if(partsNeeded > (int)middle.polarDarstellung.size()-2){
        logger.warn("cycle")<<"not enough parts available: " << partsNeeded;
        partsNeeded = (int)middle.points().size()-2;

    }
    logger.debug("defaultDrive") << "parts needed: " << partsNeeded;
    if(partsNeeded == 0 || partsNeeded == INFINITY || partsNeeded == NAN){
        logger.warn("cycle")<<"parsNeeded not valid: " << partsNeeded;
        return false;
    }

    //TODO gewichteter mittelwert!
    //TODO der ansatz ist prinzipiell bei S-Kurven schlecht!
    float middleCurvation = 0;
    std::string s;
    for(int i = 0; i < partsNeeded; i++){
        middleCurvation += middle.polarDarstellung[i+2];
        s += std::to_string(middle.polarDarstellung[i+2])+ " ; ";
    }
    middleCurvation = fabs(middleCurvation)/partsNeeded;
    logger.debug("defaultDrive")<<"curvations: " << s;
    logger.debug("defaultDrive") <<"middle-curcation: " << middleCurvation;
    float velocity = (minCurveSpeed-maxSpeed)/(maxCurvation)*(middleCurvation)+maxSpeed;
    controlData->control.velocity.velocity = velocity;
    return true;
}


bool VelocityController::launchControll(float newVelocity,float currentVelocity){
    float deltaT = lms::extra::PrecisionTime::since(lastCall).toFloat();
    if(deltaT*1000 > config->get<float>("maxDeltaTInMs")){
        return false;
    }
    float acc = (newVelocity -currentVelocity)/deltaT;
    float velocityLimited = newVelocity;
    if(acc > config->get<float>("maxAcc",1)){
        velocityLimited = currentVelocity+config->get<float>("maxAcc")*deltaT;
    }else if(acc < config->get<float>("minAcc",-1)){
        velocityLimited = currentVelocity+config->get<float>("minAcc")*deltaT;
    }else{
        return false;
    }
    controlData->control.velocity.velocity = velocityLimited;
    return true;
}

