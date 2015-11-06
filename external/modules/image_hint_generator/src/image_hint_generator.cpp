#include "image_hint_generator.h"
#include "lms/imaging_detection/line.h"
#include "lms/imaging_detection/line_point.h"
#include "lms/imaging_detection/image_hint.h"
#include "lms/imaging_detection/splitted_line.h"
#include "lms/imaging_detection/point_line.h"
#include "lms/math/math.h"
#include <cmath>
#include "lms/math/vertex.h"
#include "lms/imaging_detection/street_crossing.h"
#include "lms/imaging_detection/street_obstacle.h"
#include "lms/imaging/warp.h"
bool ImageHintGenerator::initialize() {
    config = getConfig();
    gaussBuffer = new lms::imaging::Image();
    middleLane = datamanager()->readChannel<street_environment::RoadLane>(this,"MIDDLE_LANE");
    hintContainerLane = datamanager()->
            writeChannel<lms::imaging::detection::HintContainer>(this,"HINTS");

    hintContainerObstacle = datamanager()->
            writeChannel<lms::imaging::detection::HintContainer>(this,"HINTS_OBSTACLE");

    target = datamanager()->readChannel<lms::imaging::Image>(this,"TARGET_IMAGE");
    defaultLinePointParameter.fromConfig(getConfig("defaultLPParameter"));
    defaultLinePointParameter.target =target.get();
    defaultLinePointParameter.gaussBuffer = gaussBuffer;
    return true;
}

bool ImageHintGenerator::deinitialize() {
    return false;
}
bool ImageHintGenerator::cycle() {
    static bool fromMiddle = true;
    hintContainerLane->clear();
    hintContainerObstacle->clear();
    //set the gaussbuffer
    gaussBuffer->resize(target->width(),target->height(),lms::imaging::Format::GREY);
    //clear the gaussBuffer not necessary!

    if(fromMiddle){
        if(middleLane->type() != street_environment::RoadLaneType::MIDDLE){
            logger.error("createHintsFromMiddleLane") << "middle is no middle lane!";
            return true;
        }
        createHintsFromMiddleLane(*middleLane);

        if(config->get<bool>("searchForObstacles",false)){
            createHintForObstacle(*middleLane);
        }else if(config->get<bool>("searchForCrossing",false)){
            createHintForCrossing(*middleLane);
        }
    }else{
        initialHints();
        fromMiddle = true;
    }

    return true;
}

void ImageHintGenerator::createHintForCrossing(const street_environment::RoadLane &middle ){
    lms::imaging::detection::ImageHint<lms::imaging::detection::StreetCrossing> *crossing = new lms::imaging::detection::ImageHint<lms::imaging::detection::StreetCrossing>();
    lms::imaging::detection::StreetCrossing::StreetCrossingParam scp;
    scp.target = target.get();
    scp.gaussBuffer = gaussBuffer;
    scp.fromConfig(getConfig("defaultLPParameter"));
    for(const lms::math::vertex2f &v:middle.points()){
        if(v.length() > 0.5 && v.length() < 1.2){
            scp.middleLine.points().push_back(v);
        }
    }
    crossing->parameter = scp;
    hintContainerObstacle->add(crossing);
}

void ImageHintGenerator::createHintForObstacle(const street_environment::RoadLane &middle ){
    lms::imaging::detection::ImageHint<lms::imaging::detection::StreetObstacle> *obstacleRight = new lms::imaging::detection::ImageHint<lms::imaging::detection::StreetObstacle>();
    lms::imaging::detection::ImageHint<lms::imaging::detection::StreetObstacle> *obstacleLeft = new lms::imaging::detection::ImageHint<lms::imaging::detection::StreetObstacle>();

    lms::imaging::detection::StreetObstacle::StreetObstacleParam sopRight;
    lms::imaging::detection::StreetObstacle::StreetObstacleParam sopLeft;
    sopRight.minPointCount = 3;
    sopRight.edge = true;
    sopRight.target = target.get();
    sopRight.gaussBuffer = gaussBuffer;
    sopRight.fromConfig(getConfig("defaultLPParameter"));
    sopLeft = sopRight;
    for(uint i = 1; i < middle.points().size();i++){
        lms::math::vertex2f v1 = middle.points()[i-1];
        lms::math::vertex2f v2 = middle.points()[i];
        lms::math::vertex2f tmp = v2-v1;
        tmp = tmp.rotateClockwise90deg();
        tmp = tmp.normalize();
        tmp = tmp * 0.2; //move to middle of the street
        if(v1.length() > 0.5 && v1.length() < 1.5){
            //right lane
            sopRight.middleLine.points().push_back(v1+tmp);
            sopLeft.middleLine.points().push_back(v1-tmp);
        }
    }
    obstacleRight->parameter = sopRight;
    obstacleLeft->parameter = sopLeft;
    hintContainerObstacle->add(obstacleRight);
    hintContainerObstacle->add(obstacleLeft);

}

void ImageHintGenerator::createHintForObstacleUsingSinglePoints(const street_environment::RoadLane &middle ){
    using lms::math::vertex2f;
    using lms::math::vertex2i;
    logger.info("createHintForObstacleUsingSinglePoints") << "using";

    //TODO der code ist schon sehr redundant, da sollte man sich was tolles überlegen!

    float streetWidth = 0.4;
    //float startSearchDistance = 0.4;
    int numberOfSearchPoints = 3;
    //vertex2f startPoints[numberOfSearchPoints];
    //+1 damit man nicht auf der Linie sucht
    float distanceBetweenSearchPoints = streetWidth/(numberOfSearchPoints+1);

    //Abstand zum Auto
    float startSearchDistance = 0.6;
    //suchlänge
    float searchLength = 2;

    bool foundStart = false;
    //aufpunkt auf der linie
    vertex2f *middlePoints = new vertex2f[numberOfSearchPoints];
    for(int i = 1; i < (int)middle.points().size(); i++){
        //TODO ziehmlich schlecht so ;D
        vertex2f top = middle.points()[i];
        vertex2f bot = middle.points()[i-1];
        vertex2f distance = top-bot;
        distance = distance.normalize();
        float tmpX = distance.x;
        distance.x = -distance.y;
        distance.y = tmpX;

        if(top.length() > 0.4 && !foundStart){
            for(int k = 0; k < numberOfSearchPoints; k++){
                //TODO auch noch sehr mieser Code
                //such-start-punkte in Auto-Koordinaten
                middlePoints[k] = top-distance*distanceBetweenSearchPoints*(k+1);

                foundStart = true;
            }
        }else if(top.length() > startSearchDistance+searchLength){
            break;
        }else if(top.length() > 0.4){
            lms::imaging::detection::ImageHint<lms::imaging::detection::PointLine> *line = new lms::imaging::detection::ImageHint<lms::imaging::detection::PointLine>();
            line->name = "OBSTACLE_"+std::to_string(i);
            line->parameter.validPoint = [](lms::imaging::detection::LinePoint &lp DRAWDEBUG_PARAM)->bool{
                lms::imaging::detection::EdgePoint check = lp.low_high;
                check.searchParam().x = check.x;
                check.searchParam().y = check.y;
                //20 noch in eine Config packen oder irgendwas anderes tolles tun
                check.searchParam().searchLength = 20;
                check.searchParam().searchType = lms::imaging::detection::EdgePoint::EdgeType::HIGH_LOW;
                bool found = check.find(DRAWDEBUG_ARG_N);
                return !found;
            };
            for(int k = 0; k < numberOfSearchPoints; k++){
                vertex2f endMiddle = top-distance*distanceBetweenSearchPoints*(k+1);

                //convert to image-pixel-value
                vertex2i startMiddleI;
                vertex2i endMiddleI;
                lms::imaging::V2C(&middlePoints[k],&startMiddleI);
                lms::imaging::V2C(&endMiddle,&endMiddleI);
                //create hint
                float imageSearchDistance = (endMiddleI-startMiddleI).length();
                float searchAngle = (endMiddleI-startMiddleI).angle();

                lms::imaging::detection::LinePoint::LinePointParam lpp = defaultLinePointParameter;
                lpp.x = startMiddleI.x;
                lpp.y = startMiddleI.y;
                lpp.edge = true;
                lpp.searchAngle = searchAngle;
                lpp.searchLength = imageSearchDistance;
                //lpp.sobelThreshold = 50;
                line->parameter.addParam(lpp);
                //alter endPunkt wird neuer Startpunkt:
                middlePoints[k] = endMiddle;
            }
            hintContainerObstacle->add(line);
        }
    }
    delete[] middlePoints;
}

void ImageHintGenerator::createHintsFromMiddleLane(const street_environment::RoadLane &middle){
    //TODO wenn ein Hinderniss einen Suchpunkt überdeckt könnten wir von der anderen Seite danach suchen!
    using lms::math::vertex2f;
    using lms::math::vertex2i;
    //distance between lines and offset for search
    float lineOffset = 0.1;
    float lineDistance = 0.4-lineOffset;
    lms::imaging::detection::ImageHint<lms::imaging::detection::PointLine> *hintLeft = new lms::imaging::detection::ImageHint<lms::imaging::detection::PointLine>();
    hintLeft->name = "LEFT_LANE";
    lms::imaging::detection::ImageHint<lms::imaging::detection::PointLine> *hintRight = new lms::imaging::detection::ImageHint<lms::imaging::detection::PointLine>();
    hintRight->name = "RIGHT_LANE";
    lms::imaging::detection::ImageHint<lms::imaging::detection::PointLine> *hintMiddle = new lms::imaging::detection::ImageHint<lms::imaging::detection::PointLine>();
    hintMiddle->name = "MIDDLE_LANE";
    //TODO: add more random points - punkte nicht nur an den enden erzeugen, abstand angeben und dazwischen interpolieren ,(mindestabstand zwischen den punkten)
    for(int i = 1; i < (int)middle.points().size(); i++){
        vertex2f bot = middle.points()[i-1];
        vertex2f top = middle.points()[i];
        vertex2f distance = top-bot;
        float dLength = distance.length();
        vertex2f tangent = distance.normalize();
        tangent = tangent*dLength*((double) rand() / (RAND_MAX));

        distance = distance.normalize();
        float tmpX = distance.x;
        distance.x = -distance.y;
        distance.y = tmpX;
        distance *= lineDistance;
        //such-start-punkte in Auto-Koordinaten
        vertex2f left = bot+tangent+distance;
        vertex2f right = bot+tangent-distance;
        //TODO wenn es nicht mehr geht, dann wegen /lineDistance
        vertex2f middle = bot+tangent-distance/lineDistance*lineOffset;

        //such-start-punkte in Bild-Koordinaten
        vertex2i leftI;
        vertex2i rightI;
        vertex2i middleI;
        vertex2i topI;


        lms::imaging::V2C(&left,&leftI);
        lms::imaging::V2C(&right,&rightI);
        lms::imaging::V2C(&middle,&middleI);
        lms::imaging::V2C(&top,&topI);

        float angleLeft = (leftI-topI).angle();

        float searchLength = (leftI-topI).length();
        logger.debug("cycle") <<"searchLength pix: "<<searchLength <<" "<< leftI.x << " "<< leftI.y;
        searchLength = searchLength/lineDistance*lineOffset*2;
        logger.debug("cycle")<<"angleLeft: " <<angleLeft << " length: "<<searchLength;

        defaultLinePointParameter.searchLength = searchLength;
        //add hints
        //add left
        if(defaultLinePointParameter.target->inside(leftI.x , leftI.y)){
            defaultLinePointParameter.x = leftI.x;
            defaultLinePointParameter.y = leftI.y;
            defaultLinePointParameter.searchAngle = angleLeft;
            hintLeft->parameter.addParam(defaultLinePointParameter);
        }else{
            logger.debug("cycle")<<"NOT INSIDE - LEFT";
        }

        //add right
        if(defaultLinePointParameter.target->inside(rightI.x , rightI.y)){
            defaultLinePointParameter.x = rightI.x;
            defaultLinePointParameter.y = rightI.y;
            defaultLinePointParameter.searchAngle = angleLeft+M_PI;
            hintRight->parameter.addParam(defaultLinePointParameter);
        }else{
            logger.debug("cycle")<<"NOT INSIDE - RIGHT";
        }

        //add middle
        if(defaultLinePointParameter.target->inside(middleI.x , middleI.y)){
            defaultLinePointParameter.x = middleI.x;
            defaultLinePointParameter.y = middleI.y;
            defaultLinePointParameter.searchAngle = angleLeft;
            hintMiddle->parameter.addParam(defaultLinePointParameter);
        }else{
            logger.debug("cycle")<<"NOT INSIDE - MIDDLE";
        }
    }
    if(hintLeft->getTarget() != nullptr){
        hintContainerLane->add(hintLeft);
    }
    if(hintRight->getTarget() != nullptr){
        hintContainerLane->add(hintRight);
    }
    if(hintMiddle->getTarget() != nullptr){
        hintContainerLane->add(hintMiddle);
    }
}

void ImageHintGenerator::initialHints(){
    //TODO, don't work with all cams!
    lms::imaging::detection::ImageHint<lms::imaging::detection::Line> *hint = new lms::imaging::detection::ImageHint<lms::imaging::detection::Line>();
    hint->name = "RIGHT_LANE";
    hint->parameter.target =target.get();
    hint->parameter.maxLength = 300;
    hint->parameter.approxEdge = false;
    hint->parameter.lineWidthMax = 10;
    hint->parameter.lineWidthMin = 1;
    hint->parameter.searchAngle = 0;
    hint->parameter.searchLength = 100;
    hint->parameter.gaussBuffer = gaussBuffer;
    hint->parameter.x = 180;
    hint->parameter.y = 120;
    hint->parameter.sobelThreshold = 250;
    hint->parameter.stepLengthMin = 2;
    hint->parameter.stepLengthMax = 20;
    hint->parameter.lineWidthTransMultiplier = 1;
    hint->parameter.edge = false;
    hint->parameter.verify = true;
    hint->parameter.preferVerify = false;
    hint->parameter.validPoint = [](lms::imaging::detection::LinePoint &lp DRAWDEBUG_PARAM)->bool{

#if IMAGING_DRAW_DEBUG == 1
        (void)DRAWDEBUG_ARG_N;
#endif
        //logger.info("check") << x <<" "<< y;
        bool result =  std::abs(160-lp.high_low.x)>50 || std::abs(lp.high_low.y)<140;
        //result =
        return result;
    };
    hintContainerLane->add(hint);
    //hint->parameter.containing;
    //add it
    hint = new lms::imaging::detection::ImageHint<lms::imaging::detection::Line>(*hint);
    hint->name = "LEFT_LANE";
    hint->parameter.x = 40;
    hint->parameter.y = 100;
    hint->parameter.searchAngle = M_PI;
    hintContainerLane->add(hint);

    lms::imaging::detection::ImageHint<lms::imaging::detection::SplittedLine> *hintSplit = new lms::imaging::detection::ImageHint<lms::imaging::detection::SplittedLine>();
    hintSplit->name = "MIDDLE_LANE";
    hintSplit->parameter.target =target.get();
    hintSplit->parameter.maxLength = 300;
    hintSplit->parameter.approxEdge = false;
    hintSplit->parameter.lineWidthMax = 10;
    hintSplit->parameter.lineWidthMin = 5;
    hintSplit->parameter.searchAngle = 0;
    hintSplit->parameter.searchLength = 50;
    hintSplit->parameter.gaussBuffer = gaussBuffer;
    hintSplit->parameter.x = 40;
    hintSplit->parameter.y = 210;
    hintSplit->parameter.sobelThreshold = 250;
    hintSplit->parameter.stepLengthMin = 5;
    hintSplit->parameter.stepLengthMax = 10;
    hintSplit->parameter.lineWidthTransMultiplier = 1;
    hintSplit->parameter.edge = false;
    hintSplit->parameter.verify = true;
    hintSplit->parameter.preferVerify = false;
    hintSplit->parameter.distanceBetween = 40;
    hintSplit->parameter.lineMinLength = 10;
    hintSplit->parameter.lineMaxLength = 80;
    hintSplit->parameter.validPoint = [this](lms::imaging::detection::LinePoint &lp DRAWDEBUG_PARAM){

#if IMAGING_DRAW_DEBUG == 1
        (void)DRAWDEBUG_ARG_N;
#endif
        bool result =  std::abs(160-lp.low_high.x)>50 || std::abs(lp.low_high.y)<140;
        float angle = lms::math::limitAngle_nPI_PI(lp.param().searchAngle);
        result = result && (fabs(angle) < M_PI_2*0.5) &&!(lp.low_high.y < 50);
        return result;
    };
    //TODO atm we can't search for the middle-lane that way (transformer fails)
    //hintContainer->add(hintSplit);



    hint = new lms::imaging::detection::ImageHint<lms::imaging::detection::Line>(*hint);
    hint->name = "BOX";
    hint->parameter.x = 120;
    hint->parameter.y = 50;
    hint->parameter.searchAngle = -M_PI_2*1.5;
    hint->parameter.stepLengthMax = 5;
    hint->parameter.lineWidthMax = 5;
    hint->parameter.maxLength = 20;
    hint->parameter.edge = true;
    hint->parameter.validPoint = [](lms::imaging::detection::LinePoint &lp DRAWDEBUG_PARAM){
        //logger.info("check") << x <<" "<< y;
        lms::imaging::detection::EdgePoint::EdgePointParam param = lp.param();
        param.searchType = lms::imaging::detection::EdgePoint::EdgeType::HIGH_LOW;
        param.searchLength = 20;
        lms::imaging::detection::EdgePoint ep;
        return !ep.find(param DRAWDEBUG_ARG);
    };
    //hintContainer->add(hint);
}
