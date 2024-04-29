#include "CorsairManager.h"
#include "UnitUtil.h"
#include "Micro.h"

using namespace UAlbertaBot;

CorsairManager::CorsairManager()
{
}

void CorsairManager::executeMicro(const BWAPI::Unitset& targets)
{
    assignTargetsPatrol(targets);
}


void CorsairManager::assignTargetsPatrol(const BWAPI::Unitset& targets)
{
    const BWAPI::Unitset& corsairUnits = getUnits();

    // figure out targets
    BWAPI::Unitset corsairUnitTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(corsairUnitTargets, corsairUnitTargets.end()), [](BWAPI::Unit u) { return u->isVisible(); });
    //std::cout << "ManagerPatrolmain\n"; //chost
    for (auto& corsairUnit : corsairUnits)
    {
        // train sub units such as scarabs or interceptors
        //trainSubUnits(corsairUnit);
        //std::cout << "ManagerPatrol\n"; //chost
        //std::cout << corsairUnit << " Unit\n"; 
        //std::cout << corsairUnit->isPatrolling() << "can patrol\n";
        //std::cout << m_order.getPosition() << " Pos\n";
        Micro::SmartAttackPatrol(corsairUnit, m_order.getPosition());

        //std::cout << corsairUnit->isPatrolling() <<"is patroling\n";
       
        //std::cout << "ManagerPatrolend\n";
        // if the order is to attack or defend
        /*if (m_order.getType() == SquadOrderTypes::Attack || m_order.getType() == SquadOrderTypes::Defend)
        {
            // if there are targets
            if (!corsairUnitTargets.empty())
            {
                // find the best target for this zealot
                BWAPI::Unit target = getTarget(corsairUnit, corsairUnitTargets);

                if (target && Config::Debug::DrawUnitTargetInfo)
                {
                    BWAPI::Broodwar->drawLineMap(corsairUnit->getPosition(), corsairUnit->getTargetPosition(), BWAPI::Colors::Purple);
                }


                // attack it
                if (Config::Micro::KiteWithRangedUnits)
                {
                    if (corsairUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || corsairUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
                    {
                        Micro::MutaDanceTarget(corsairUnit, target);
                    }
                    else
                    {
                        Micro::SmartKiteTarget(corsairUnit, target);
                    }
                }
                else
                {
                    Micro::SmartAttackUnit(corsairUnit, target);
                }
            }
            // if there are no targets
            else
            {
                // if we're not near the order position
                if (corsairUnit->getDistance(m_order.getPosition()) > 100)
                {
                    // move to it
                    Micro::SmartAttackPatrol(corsairUnit, m_order.getPosition());
                }
            }
        }*/
    }
}


void CorsairManager::assignTargetsOld(const BWAPI::Unitset& targets)
{
    const BWAPI::Unitset& corsairUnits = getUnits();

    // figure out targets
    BWAPI::Unitset corsairUnitTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(corsairUnitTargets, corsairUnitTargets.end()), [](BWAPI::Unit u) { return u->isVisible(); });

    for (auto& corsairUnit : corsairUnits)
    {
        // train sub units such as scarabs or interceptors
        //trainSubUnits(corsairUnit);

        // if the order is to attack or defend
        if (m_order.getType() == SquadOrderTypes::Attack || m_order.getType() == SquadOrderTypes::Defend)
        {
            // if there are targets
            if (!corsairUnitTargets.empty())
            {
                // find the best target for this zealot
                BWAPI::Unit target = getTarget(corsairUnit, corsairUnitTargets);

                if (target && Config::Debug::DrawUnitTargetInfo)
                {
                    BWAPI::Broodwar->drawLineMap(corsairUnit->getPosition(), corsairUnit->getTargetPosition(), BWAPI::Colors::Purple);
                }


                // attack it
                if (Config::Micro::KiteWithRangedUnits)
                {
                    if (corsairUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || corsairUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
                    {
                        Micro::MutaDanceTarget(corsairUnit, target);
                    }
                    else
                    {
                        Micro::SmartKiteTarget(corsairUnit, target);
                    }
                }
                else
                {
                    Micro::SmartAttackUnit(corsairUnit, target);
                }
            }
            // if there are no targets
            else
            {
                // if we're not near the order position
                if (corsairUnit->getDistance(m_order.getPosition()) > 100)
                {
                    // move to it
                    Micro::SmartAttackMove(corsairUnit, m_order.getPosition());
                }
            }
        }
    }
}

std::pair<BWAPI::Unit, BWAPI::Unit> CorsairManager::findClosestUnitPair(const BWAPI::Unitset& attackers, const BWAPI::Unitset& targets)
{
    std::pair<BWAPI::Unit, BWAPI::Unit> closestPair(nullptr, nullptr);
    double closestDistance = std::numeric_limits<double>::max();

    for (auto& attacker : attackers)
    {
        BWAPI::Unit target = getTarget(attacker, targets);
        double dist = attacker->getDistance(attacker);

        if (!closestPair.first || (dist < closestDistance))
        {
            closestPair.first = attacker;
            closestPair.second = target;
            closestDistance = dist;
        }
    }

    return closestPair;
}

// get a target for the zealot to attack
BWAPI::Unit CorsairManager::getTarget(BWAPI::Unit corsairUnit, const BWAPI::Unitset& targets)
{
    int bestPriorityDistance = 1000000;
    int bestPriority = 0;

    double bestLTD = 0;

    int highPriority = 0;
    double closestDist = std::numeric_limits<double>::infinity();
    BWAPI::Unit closestTarget = nullptr;

    for (const auto& target : targets)
    {
        if (target->getType() == BWAPI::UnitTypes::Protoss_Observer)
            continue;
        double distance = corsairUnit->getDistance(target);
        double LTD = UnitUtil::CalculateLTD(target, corsairUnit);
        int priority = getAttackPriority(corsairUnit, target);
        bool targetIsThreat = LTD > 0;

        if (!closestTarget || (priority > highPriority) || (priority == highPriority && distance < closestDist))
        {
            closestDist = distance;
            highPriority = priority;
            closestTarget = target;
        }
    }

    return closestTarget;
}

// get the attack priority of a type in relation to a zergling
int CorsairManager::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
    BWAPI::UnitType rangedType = rangedUnit->getType();
    BWAPI::UnitType targetType = target->getType();


    if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
    {
        if (target->getType() == BWAPI::UnitTypes::Protoss_Carrier)
        {

            return 100;
        }

        if (target->getType() == BWAPI::UnitTypes::Protoss_Corsair)
        {
            return 90;
        }
    }

    bool isThreat = rangedType.isFlyer() ? targetType.airWeapon() != BWAPI::WeaponTypes::None : targetType.groundWeapon() != BWAPI::WeaponTypes::None;

    if (target->getType().isWorker())
    {
        isThreat = false;
    }

    if (target->getType() == BWAPI::UnitTypes::Zerg_Larva || target->getType() == BWAPI::UnitTypes::Zerg_Egg)
    {
        return 0;
    }

    if (rangedUnit->isFlying() && target->getType() == BWAPI::UnitTypes::Protoss_Carrier)
    {
        return 101;
    }

    // if the target is building something near our base something is fishy
    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()) && target->getDistance(ourBasePosition) < 1200)
    {
        return 100;
    }

    if (target->getType().isBuilding() && (target->isCompleted() || target->isBeingConstructed()) && target->getDistance(ourBasePosition) < 1200)
    {
        return 90;
    }

    // highest priority is something that can attack us or aid in combat
    if (targetType == BWAPI::UnitTypes::Terran_Bunker || isThreat)
    {
        return 11;
    }
    // next priority is worker
    else if (targetType.isWorker())
    {
        if (rangedUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
        {
            return 11;
        }

        return 11;
    }
    // next is special buildings
    else if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
    {
        return 5;
    }
    // next is special buildings
    else if (targetType == BWAPI::UnitTypes::Protoss_Pylon)
    {
        return 5;
    }
    // next is buildings that cost gas
    else if (targetType.gasPrice() > 0)
    {
        return 4;
    }
    else if (targetType.mineralPrice() > 0)
    {
        return 3;
    }
    // then everything else
    else
    {
        return 1;
    }
}

BWAPI::Unit CorsairManager::closestrangedUnit(BWAPI::Unit target, std::set<BWAPI::Unit>& corsairUnitsToAssign)
{
    double minDistance = 0;
    BWAPI::Unit closest = nullptr;

    for (auto& corsairUnit : corsairUnitsToAssign)
    {
        double distance = corsairUnit->getDistance(target);
        if (!closest || distance < minDistance)
        {
            minDistance = distance;
            closest = corsairUnit;
        }
    }

    return closest;
}


// still has bug in it somewhere, use Old version
void CorsairManager::assignTargetsNew(const BWAPI::Unitset& targets)
{
    const BWAPI::Unitset& corsairUnits = getUnits();

    // figure out targets
    BWAPI::Unitset corsairUnitTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(corsairUnitTargets, corsairUnitTargets.end()), [](BWAPI::Unit u) { return u->isVisible(); });

    BWAPI::Unitset corsairUnitsToAssign(corsairUnits);
    std::map<BWAPI::Unit, int> attackersAssigned;

    for (auto& unit : corsairUnitTargets)
    {
        attackersAssigned[unit] = 0;
    }

    // keep assigning targets while we have attackers and targets remaining
    while (!corsairUnitsToAssign.empty() && !corsairUnitTargets.empty())
    {
        auto attackerAssignment = findClosestUnitPair(corsairUnitsToAssign, corsairUnitTargets);
        BWAPI::Unit& attacker = attackerAssignment.first;
        BWAPI::Unit& target = attackerAssignment.second;

        UAB_ASSERT_WARNING(attacker, "We should have chosen an attacker!");

        if (!attacker)
        {
            break;
        }

        if (!target)
        {
            BWAPI::Position enemyBasePosition = BWAPI::Position(BWAPI::Broodwar->enemy()->getStartLocation());

            Micro::SmartAttackMove(attacker, m_order.getPosition());
            continue;
        }

        if (Config::Micro::KiteWithRangedUnits)
        {
            if (attacker->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || attacker->getType() == BWAPI::UnitTypes::Terran_Vulture)
            {
                Micro::MutaDanceTarget(attacker, target);
            }
            else
            {
                Micro::SmartKiteTarget(attacker, target);
            }
        }
        else
        {
            Micro::SmartAttackUnit(attacker, target);
        }

        // update the number of units assigned to attack the target we found
        int& assigned = attackersAssigned[attackerAssignment.second];
        assigned++;

        // if it's a small / fast unit and there's more than 2 things attacking it already, don't assign more
        if ((target->getType().isWorker() || target->getType() == BWAPI::UnitTypes::Zerg_Zergling) && (assigned > 2))
        {
            corsairUnitTargets.erase(target);
        }
        // if it's a building and there's more than 10 things assigned to it already, don't assign more
        else if (target->getType().isBuilding() && (assigned > 10))
        {
            corsairUnitTargets.erase(target);
        }

        corsairUnitsToAssign.erase(attacker);
    }

    // if there's no targets left, attack move to the order destination
    if (corsairUnitTargets.empty())
    {
        for (auto& unit : corsairUnitsToAssign)
        {
            if (unit->getDistance(m_order.getPosition()) > 100)
            {
                // move to it
                Micro::SmartAttackMove(unit, m_order.getPosition());
            }
        }
    }
}