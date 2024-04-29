#include "Common.h"
#include "DetectorManager.h"
#include "Global.h"
#include "BaseLocationManager.h"
#include "Micro.h"
#include "MapTools.h"
#include "InformationManager.h"
#include "BaseLocation.h"

using namespace UAlbertaBot;

DetectorManager::DetectorManager()
{ 

}

void DetectorManager::executeMicro(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & detectorUnits = getUnits();

    if (detectorUnits.empty())
    {
        return;
    }

    for (size_t i(0); i<targets.size(); ++i)
    {
        // do something here if there's targets
    }

    m_cloakedUnitMap.clear();
    BWAPI::Unitset cloakedUnits;

    // figure out targets
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        // conditions for targeting
        if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
            unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
            unit->getType() == BWAPI::UnitTypes::Terran_Wraith)
        {
            cloakedUnits.insert(unit);
            m_cloakedUnitMap[unit] = false;
        }
    }

    bool detectorUnitInBattle = false;

    // for each detectorUnit
    for (auto & detectorUnit : detectorUnits)
    {
        // if we need to regroup, move the detectorUnit to that location
        if (BWAPI::Broodwar->getFrameCount() > 70000) {
            std::vector<const BaseLocation*> baseLocations = Global::Bases().getBaseLocations();
            auto& baseToMove = baseLocations[std::rand() % (baseLocations.size())];
            if (!baseToMove)
                Micro::SmartMove(detectorUnit, baseToMove->getPosition());

        }
        else if ((!detectorUnitInBattle && unitClosestToEnemy && unitClosestToEnemy->getPosition().isValid()) || Global::Info().enemyHasCloakedUnits() && unitClosestToEnemy)
        {
            for (auto& unit : cloakedUnits) {
                if (!m_cloakedUnitMap[unit] && unit->getPosition()) {
                    Micro::SmartMove(detectorUnit, unit->getPosition());
                    m_cloakedUnitMap[unit] = true;
                    break;
                }
            }
            Micro::SmartMove(detectorUnit, unitClosestToEnemy->getPosition());
            detectorUnitInBattle = true;
        }
        // otherwise there is no battle or no closest to enemy so we don't want our detectorUnit to die
        // send him to scout around the map
        else
        {
            BWAPI::Position explorePosition = BWAPI::Position(Global::Map().getLeastRecentlySeenTileEnemy());
            Micro::SmartMove(detectorUnit, explorePosition);
        }
    }
}

BWAPI::Unit DetectorManager::closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit)
{
    BWAPI::Unit closestCloaked = nullptr;
    double closestDist = 100000;

    for (auto & unit : cloakedUnits)
    {
        // if we haven't already assigned an detectorUnit to this cloaked unit
        if (!m_cloakedUnitMap[unit])
        {
            double dist = unit->getDistance(detectorUnit);

            if (!closestCloaked || (dist < closestDist))
            {
                closestCloaked = unit;
                closestDist = dist;
            }
        }
    }

    return closestCloaked;
}