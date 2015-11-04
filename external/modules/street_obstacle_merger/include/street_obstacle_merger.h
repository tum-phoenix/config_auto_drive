#ifndef LMS_STREET_OBJECT_MERGER
#define LMS_STREET_OBJECT_MERGER

#include "lms/module.h"
#include "sensor_utils/car.h"
#include "street_environment/street_environment.h"
#include "street_environment/obstacle.h"
#include "lms/math/math.h"

class StreetObjectMerger : public lms::Module {
public:
    bool initialize() override;
    bool deinitialize() override;
    bool cycle() override;
private:
    /**
     * @brief merge adds new obstacles to the old once
     * @param obstaclesNew
     * @param obstaclesOld
     */
    void merge(street_environment::EnvironmentObstacles &obstaclesNew,street_environment::EnvironmentObstacles &obstaclesOld);
    void filter(street_environment::EnvironmentObstacles &obstaclesOld);
    void createOutput(street_environment::EnvironmentObstacles &obstaclesOld);


    bool inVisibleArea(float x, float y);

    void getObstacles(const street_environment::EnvironmentObjects &env,street_environment::EnvironmentObstacles &output);
    const street_environment::EnvironmentObjects *envInput;
    street_environment::EnvironmentObjects *envOutput;
    const street_environment::RoadLane *middle;
    const lms::ModuleConfig* config;
    const sensor_utils::Car *car;
    std::vector<lms::math::Rect> visibleAreas;


};

#endif /* LMS_STREET_OBJECT_MERGER */
