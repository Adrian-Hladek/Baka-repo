#include "MapTools.h"
#include "BaseLocationManager.h"
#include "Global.h"

#include "../../BWEM 1.4.1/src/bwem.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <array>



using namespace UAlbertaBot;
namespace { auto& theMap = BWEM::Map::Instance(); }

const size_t LegalActions = 4;
const int actionX[LegalActions] ={1, -1, 0, 0};
const int actionY[LegalActions] ={0, 0, 1, -1};
BWAPI::TilePosition cachedChokepointPos = BWAPI::TilePositions::None;
bool chokepointPosCached = false;

BWAPI::TilePosition cachedChokepointPosNew = BWAPI::TilePositions::None;
bool chokepointPosCachedNew = false;

// constructor for MapTools
MapTools::MapTools()
{
    onStart();
}

void MapTools::onStart()
{
    MapTools::resetCachedData();
    MapTools::resetCachedDataNew();
    PROFILE_FUNCTION();

    m_width          = BWAPI::Broodwar->mapWidth();
    m_height         = BWAPI::Broodwar->mapHeight();
    m_walkable       = Grid<int>(m_width, m_height, 1);
    m_buildable      = Grid<int>(m_width, m_height, 0);
    m_depotBuildable = Grid<int>(m_width, m_height, 0);
    m_lastSeen       = Grid<int>(m_width, m_height, 0);
    m_sectorNumber   = Grid<int>(m_width, m_height, 0);

    // Set the boolean grid data from the Map
    for (int x(0); x < m_width; ++x)
    {
        for (int y(0); y < m_height; ++y)
        {
            m_buildable.set(x, y, canBuild(x, y));
            m_depotBuildable.set(x, y, canBuild(x, y));
            m_walkable.set(x, y, m_buildable.get(x,y) || canWalk(x, y));
        }
    }

    // set tiles that static resources are on as unbuildable
    for (auto & resource : BWAPI::Broodwar->getStaticNeutralUnits())
    {
        if (!resource->getType().isResourceContainer())
        {
            continue;
        }

        const int tileX = resource->getTilePosition().x;
        const int tileY = resource->getTilePosition().y;

        for (int x=tileX; x<tileX+resource->getType().tileWidth(); ++x)
        {
            for (int y=tileY; y<tileY+resource->getType().tileHeight(); ++y)
            {
                m_buildable.set(x, y, false);

                // depots can't be built within 3 tiles of any resource
                for (int rx=-3; rx<=3; rx++)
                {
                    for (int ry=-3; ry<=3; ry++)
                    {
                        if (!BWAPI::TilePosition(x+rx, y+ry).isValid())
                        {
                            continue;
                        }

                        m_depotBuildable.set(x+rx, y+ry, 0);
                    }
                }
            }
        }
    }

    // compute the map connectivity
    computeConnectivity();
    computeMap();

    


    theMap.Initialize();
    theMap.EnableAutomaticPathAnalysis();
    bool startingLocationsOK = theMap.FindBasesForStartingLocations();
    UAB_ASSERT(startingLocationsOK, "Starting locations not OK");
        
}

void MapTools::onFrame()
{
    PROFILE_FUNCTION();

    for (int x=0; x<m_width; ++x)
    {
        for (int y=0; y<m_height; ++y)
        {
            if (isVisible(x, y))
            {
                m_lastSeen.set(x, y, m_frame);
            }
        }
    }
    
    m_frame++;
    draw();
}

void MapTools::computeMap()
{
    if (m_map.width() > 0) { return; }

    m_map = StarDraftMap(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

    for (size_t x = 0; x < m_map.width(); x++)
    {
        for (size_t y = 0; y < m_map.height(); y++)
        {
            if      (isDepotBuildableTile(x, y)) { m_map.set(x, y, TileType::BuildAll);   }
            else if (isBuildable(x, y))          { m_map.set(x, y, TileType::NoDepot);    }
            else if (isWalkable(x, y))           { m_map.set(x, y, TileType::Walk);       }
            else                                 { m_map.set(x, y, TileType::Unwalkable); }
        }
    }

    for (auto & mineral : BWAPI::Broodwar->getStaticMinerals())
    {
        const BWAPI::TilePosition mineralTile(mineral->getPosition());
        m_map.set(mineralTile.x, mineralTile.y, TileType::Mineral);
        m_map.set(mineralTile.x-1, mineralTile.y, TileType::Mineral);
    }

    for (auto & geyser : BWAPI::Broodwar->getStaticGeysers())
    {
        const BWAPI::TilePosition geyserTile(geyser->getPosition());
        m_map.set(geyserTile.x, geyserTile.y, TileType::Gas);
        m_map.set(geyserTile.x+1, geyserTile.y, TileType::Gas);
        m_map.set(geyserTile.x-1, geyserTile.y, TileType::Gas);
        m_map.set(geyserTile.x-2, geyserTile.y, TileType::Gas);
        m_map.set(geyserTile.x, geyserTile.y-1, TileType::Gas);
        m_map.set(geyserTile.x+1, geyserTile.y-1, TileType::Gas);
        m_map.set(geyserTile.x-1, geyserTile.y-1, TileType::Gas);
        m_map.set(geyserTile.x-2, geyserTile.y-1, TileType::Gas);
    }

    for (auto & tile : BWAPI::Broodwar->getStartLocations())
    {
        m_map.addStartTile(tile.x, tile.y);
    }

    for (size_t x=0; x<4*m_map.width(); x++)
    {
        for (size_t y=0; y<4*m_map.height(); y++)
        {
            m_map.setWalk(x, y, BWAPI::Broodwar->isWalkable(x,y));
        }
    }
}

void MapTools::computeConnectivity()
{
    PROFILE_FUNCTION();

    // the fringe data structe we will use to do our BFS searches
    std::vector<std::array<int, 2>> fringe;
    fringe.reserve(m_width*m_height);
    int sectorNumber = 0;

    // for every tile on the map, do a connected flood fill using BFS
    for (int x=0; x<m_width; ++x)
    {
        for (int y=0; y<m_height; ++y)
        {
            // if the sector is not currently 0, or the map isn't walkable here, then we can skip this tile
            if (getSectorNumber(x, y) != 0 || !isWalkable(x, y))
            {
                continue;
            }

            // increase the sector number, so that walkable tiles have sectors 1-N
            sectorNumber++;

            // reset the fringe for the search and add the start tile to it
            fringe.clear();
            fringe.push_back({x,y});
            m_sectorNumber.set(x, y, sectorNumber);

            // do the BFS, stopping when we reach the last element of the fringe
            for (size_t fringeIndex=0; fringeIndex<fringe.size(); ++fringeIndex)
            {
                auto & tile = fringe[fringeIndex];

                // check every possible child of this tile
                for (size_t a=0; a<LegalActions; ++a)
                {
                    const int nextX = tile[0] + actionX[a];
                    const int nextY = tile[1] + actionY[a];

                    // if the new tile is inside the map bounds, is walkable, and has not been assigned a sector, add it to the current sector and the fringe
                    if (isValidTile(nextX, nextY) && isWalkable(nextX, nextY) && (getSectorNumber(nextX, nextY) == 0))
                    {
                        m_sectorNumber.set(nextX, nextY, sectorNumber);
                        fringe.push_back({nextX, nextY});
                    }
                }
            }
        }
    }
}

bool MapTools::isExplored(const BWAPI::TilePosition & pos) const
{
    return isExplored(pos.x, pos.y);
}

bool MapTools::isExplored(const BWAPI::Position & pos) const
{
    return isExplored(BWAPI::TilePosition(pos));
}

bool MapTools::isExplored(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY)) { return false; }

    return BWAPI::Broodwar->isExplored(tileX, tileY);
}

bool MapTools::isVisible(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY)) { return false; }

    return BWAPI::Broodwar->isVisible(BWAPI::TilePosition(tileX, tileY));
}

bool MapTools::isPowered(int tileX, int tileY) const
{
    return BWAPI::Broodwar->hasPower(BWAPI::TilePosition(tileX, tileY));
}

int MapTools::getGroundDistance(const BWAPI::Position & src, const BWAPI::Position & dest) const
{
    if (m_allMaps.size() > 50)
    {
        m_allMaps.clear();
    }

    return getDistanceMap(dest).getDistance(src);
}

const DistanceMap & MapTools::getDistanceMap(const BWAPI::Position & pos) const
{
    return getDistanceMap(BWAPI::TilePosition(pos));
}

const DistanceMap & MapTools::getDistanceMap(const BWAPI::TilePosition & tile) const
{
    std::pair<int,int> pairTile(tile.x, tile.y);

    if (m_allMaps.find(pairTile) == m_allMaps.end())
    {
        m_allMaps[pairTile] = DistanceMap();
        m_allMaps[pairTile].computeDistanceMap(tile);
    }

    return m_allMaps[pairTile];
}

int MapTools::getSectorNumber(int x, int y) const
{
    if (!isValidTile(x, y))
    {
        return 0;
    }

    return m_sectorNumber.get(x, y);
}

bool MapTools::isValidTile(int tileX, int tileY) const
{
    return tileX >= 0 && tileY >= 0 && tileX < m_width && tileY < m_height;
}

bool MapTools::isValidTile(const BWAPI::TilePosition & tile) const
{
    return isValidTile(tile.x, tile.y);
}

bool MapTools::isValidPosition(const BWAPI::Position & pos) const
{
    return isValidTile(BWAPI::TilePosition(pos));
}

bool MapTools::isConnected(int x1, int y1, int x2, int y2) const
{
    if (!isValidTile(x1, y1) || !isValidTile(x2, y2))
    {
        return false;
    }

    const int s1 = getSectorNumber(x1, y1);
    const int s2 = getSectorNumber(x2, y2);

    return s1 != 0 && (s1 == s2);
}

bool MapTools::isConnected(const BWAPI::TilePosition & p1, const BWAPI::TilePosition & p2) const
{
    return isConnected(p1.x, p1.y, p2.x, p2.y);
}

bool MapTools::isConnected(const BWAPI::Position & p1, const BWAPI::Position & p2) const
{
    return isConnected(BWAPI::TilePosition(p1), BWAPI::TilePosition(p2));
}

bool MapTools::isBuildable(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY))
    {
        return false;
    }

    return m_buildable.get(tileX, tileY);
}

bool MapTools::canBuildTypeAtPosition(int tileX, int tileY, const BWAPI::UnitType & type) const
{
    return BWAPI::Broodwar->canBuildHere(BWAPI::TilePosition(tileX, tileY), type);
}

bool MapTools::isBuildable(const BWAPI::TilePosition & tile) const
{
    return isBuildable(tile.x, tile.y);
}

void MapTools::printMap() const
{
    std::stringstream ss;
    for (int y(0); y < m_height; ++y)
    {
        for (int x(0); x < m_width; ++x)
        {
            ss << isWalkable(x, y);
        }

        ss << "\n";
    }

    std::ofstream out("map.txt");
    out << ss.str();
    out.close();
}

bool MapTools::isDepotBuildableTile(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY))
    {
        return false;
    }

    return m_depotBuildable.get(tileX, tileY);
}

bool MapTools::isWalkable(int tileX, int tileY) const
{
    if (!isValidTile(tileX, tileY))
    {
        return false;
    }

    return m_walkable.get(tileX, tileY);
}

bool MapTools::isWalkable(const BWAPI::TilePosition & tile) const
{
    return isWalkable(tile.x, tile.y);
}

int MapTools::width() const
{
    return m_width;
}

int MapTools::height() const
{
    return m_height;
}

void MapTools::drawTile(int tileX, int tileY, const BWAPI::Color & color) const
{
    const int padding   = 2;
    const int px        = tileX*32 + padding;
    const int py        = tileY*32 + padding;
    const int d         = 32 - 2*padding;

    BWAPI::Broodwar->drawLineMap(px,     py,     px + d, py,     color);
    BWAPI::Broodwar->drawLineMap(px + d, py,     px + d, py + d, color);
    BWAPI::Broodwar->drawLineMap(px + d, py + d, px,     py + d, color);
    BWAPI::Broodwar->drawLineMap(px,     py + d, px,     py,     color);
}

const std::vector<BWAPI::TilePosition> & MapTools::getClosestTilesTo(const BWAPI::TilePosition & tilePos) const
{
    return getDistanceMap(tilePos).getSortedTiles();
}

const std::vector<BWAPI::TilePosition> & MapTools::getClosestTilesTo(const BWAPI::Position & pos) const
{
    return getClosestTilesTo(BWAPI::TilePosition(pos));
}

BWAPI::TilePosition MapTools::getLeastRecentlySeenTile() const
{
    int minSeen = std::numeric_limits<int>::max();
    BWAPI::TilePosition leastSeen;
    const BaseLocation * baseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->self());
    UAB_ASSERT(baseLocation, "Null self baselocation is insanely bad");

    for (auto & tile : baseLocation->getClosestTiles())
    {
        UAB_ASSERT(isValidTile(tile), "How is this tile not valid?");

        const int lastSeen = m_lastSeen.get(tile.x, tile.y);
        if (lastSeen < minSeen)
        {
            minSeen = lastSeen;
            leastSeen = tile;
        }
    }

    return leastSeen;
}

BWAPI::TilePosition MapTools::getLeastRecentlySeenTileEnemy() const
{
    int minSeen = std::numeric_limits<int>::max();
    BWAPI::TilePosition leastSeen;
    const BaseLocation* baseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->self());
    const BaseLocation* enemyBaseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->enemy());
    
    UAB_ASSERT(baseLocation, "Null baselocation is insanely ");

    if (!enemyBaseLocation)
        enemyBaseLocation = baseLocation;

    for (auto& tile : enemyBaseLocation->getClosestTiles())
    {
        UAB_ASSERT(isValidTile(tile), "How is this tile not valid?");

        const int lastSeen = m_lastSeen.get(tile.x, tile.y);
        if (lastSeen < minSeen)
        {
            minSeen = lastSeen;
            leastSeen = tile;
        }
    }

    return leastSeen;
}

double getDistance(BWAPI::Position p1, BWAPI::Position  p2) {
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

std::vector<const BaseLocation*> MapTools::sortBaseLocationsByDistance(const BaseLocation* enemyBase) const {
    const std::vector<const BaseLocation*> baseLocations(Global::Bases().getBaseLocations());
    std::vector<const BaseLocation*> baseLocationsCopy = baseLocations;
    std::sort(baseLocationsCopy.begin(), baseLocationsCopy.end(), [enemyBase](const BaseLocation* a, const BaseLocation* b) {
        double distanceToA = getDistance(enemyBase->getPosition(), a->getPosition());
        double distanceToB = getDistance(enemyBase->getPosition(), b->getPosition());
        return distanceToA < distanceToB;
    });
    return baseLocationsCopy;
}

BWAPI::TilePosition MapTools::getLeastRecentlySeenBaseEnemy() const
{
    int minSeen = std::numeric_limits<int>::max();
    BWAPI::TilePosition leastSeen;
    const BaseLocation* baseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->self());
    const BaseLocation* enemyBaseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->enemy());
    const std::vector<const BaseLocation*> bases = sortBaseLocationsByDistance(Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->enemy()));
    
    UAB_ASSERT(baseLocation, "Null baselocation is insanely ");
    int leastSeenBaseIndex;
    int i = 0;
    bool blocked = false;
    int lenght;
    for (auto& base : bases)
    {
        UAB_ASSERT(base, "Null base is interesting ");
        lenght = -1;

        const BWEM::CPPath path = theMap.GetPath(baseLocation->getPosition(), base->getPosition(), &lenght);

        for (auto& choke : path) {

            if (choke->Blocked() || !choke->AccessibleFrom(findCLosestChokepoint())) {
                blocked = true;
            }
        }
        if (blocked || lenght == -1) {
            BWAPI::Broodwar->drawCircleMap(base->getPosition().x, base->getPosition().x, 48, BWAPI::Color(255, 0, 0), true);
            blocked = false;
            i++;
            continue;
        }

        const int lastSeen = m_lastSeen.get(base->getClosestTiles()[0].x, base->getClosestTiles()[0].y);
        if (lastSeen < minSeen)
        {
            minSeen = lastSeen;
            leastSeenBaseIndex = i;
        }
        i++;
    }

    minSeen = std::numeric_limits<int>::max();
    //std::cout << "count: " << bases.at(leastSeenBaseIndex)->getClosestTiles().size() << std::endl;
    std::vector<BWAPI::TilePosition> tiles = bases.at(leastSeenBaseIndex)->getClosestTiles(10);
    for (auto& tile : tiles)
    {
        UAB_ASSERT(isValidTile(tile), "How is this tile not valid?");

        

        const int lastSeen = m_lastSeen.get(tile.x, tile.y);
        if (lastSeen < minSeen)
        {
            minSeen = lastSeen;
            leastSeen = tile;
        }
    }
    BWAPI::Broodwar->drawCircleMap(leastSeen.x * 32, leastSeen.y * 32,  26, BWAPI::Color(0, 255, 0), true);

    return leastSeen;
}

BWAPI::TilePosition MapTools::getLeastRecentlySeenStartingBase() const
{
    int minSeen = std::numeric_limits<int>::max();
    BWAPI::TilePosition leastSeen;
    const BaseLocation* baseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->self());
    const std::vector<const BaseLocation*> bases = Global::Bases().getStartingBaseLocations();

    UAB_ASSERT(baseLocation, "Null baselocation is insanely ");
    int leastSeenBaseIndex;
    int i = 0;
    bool blocked = false;
    int lenght;
    for (auto& base : bases)
    {
        UAB_ASSERT(base, "Null base is interesting ");
        lenght = -1;

        const BWEM::CPPath path = theMap.GetPath(baseLocation->getPosition(), base->getPosition(), &lenght);

        for (auto& choke : path) {

            if (choke->Blocked() || !choke->AccessibleFrom(findCLosestChokepoint())) {
                blocked = true;
            }
        }
        if (blocked || lenght == -1) {
            BWAPI::Broodwar->drawCircleMap(base->getPosition().x, base->getPosition().x, 48, BWAPI::Color(255, 0, 0), true);
            blocked = false;
            i++;
            continue;
        }

        const int lastSeen = m_lastSeen.get(base->getClosestTiles()[0].x, base->getClosestTiles()[0].y);
        if (lastSeen < minSeen)
        {
            minSeen = lastSeen;
            leastSeenBaseIndex = i;
        }
        i++;
    }

    minSeen = std::numeric_limits<int>::max();
    //std::cout << "count: " << bases.at(leastSeenBaseIndex)->getClosestTiles().size() << std::endl;
    std::vector<BWAPI::TilePosition> tiles = bases.at(leastSeenBaseIndex)->getClosestTiles(10);
    for (auto& tile : tiles)
    {
        UAB_ASSERT(isValidTile(tile), "How is this tile not valid?");



        const int lastSeen = m_lastSeen.get(tile.x, tile.y);
        if (lastSeen < minSeen)
        {
            minSeen = lastSeen;
            leastSeen = tile;
        }
    }
    BWAPI::Broodwar->drawCircleMap(leastSeen.x * 32, leastSeen.y * 32, 26, BWAPI::Color(0, 255, 0), true);
    //std::cout << leastSeen.x * 32 << ":" << leastSeen.y * 32 << std::endl;

    return leastSeen;
}


BWAPI::TilePosition MapTools::getLeastRecentlySeenBase() const
{
    int minSeen = std::numeric_limits<int>::max();
    BWAPI::TilePosition leastSeen;
    const std::vector<const BaseLocation*> bases = Global::Bases().getBaseLocations();
    const BaseLocation* baseLocation = Global::Bases().getPlayerStartingBaseLocation(BWAPI::Broodwar->self());

    
    int leastSeenBaseIndex;
    int i = 0, lenght;
    bool blocked = false;
    for (auto& base : bases)
    {
        UAB_ASSERT(base, "Null base is interesting ");
        lenght = -1;
        const BWEM::CPPath path = theMap.GetPath(baseLocation->getPosition(), base->getPosition(), &lenght);
        for (auto& choke : path) {
            if (choke->Blocked()) {
                blocked = true;
            }
        }
        if (blocked || lenght == -1) {
            blocked = false;
            i++;
            continue;
        }

        const int lastSeen = m_lastSeen.get(base->getClosestTiles()[0].x, base->getClosestTiles()[0].y);
        if (lastSeen < minSeen)
        {
            minSeen = lastSeen;
            leastSeenBaseIndex = i;
        }
        i++;
    }


    minSeen = std::numeric_limits<int>::max();

    for (auto& tile : bases.at(leastSeenBaseIndex)->getClosestTiles(10))
    {
        UAB_ASSERT(isValidTile(tile), "How is this tile not valid?");

        const int lastSeen = m_lastSeen.get(tile.x, tile.y);
        if (lastSeen < minSeen)
        {
            minSeen = lastSeen;
            leastSeen = tile;
        }
    }

    return leastSeen;
}

bool MapTools::canWalk(int tileX, int tileY) const
{
    for (int i=0; i<4; ++i)
    {
        for (int j=0; j<4; ++j)
        {
            if (!BWAPI::Broodwar->isWalkable(tileX*4 + i, tileY*4 + j))
            {
                return false;
            }
        }
    }

    return true;
}

bool MapTools::canBuild(int tileX, int tileY) const
{
    return BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tileX, tileY));
}

void MapTools::draw() const
{
    const BWAPI::TilePosition screen(BWAPI::Broodwar->getScreenPosition());
    const int sx = screen.x;
    const int sy = screen.y;
    const int ex = sx + 20;
    const int ey = sy + 15;
    
    for (int x = sx; x < ex; ++x)
    {
        for (int y = sy; y < ey; y++)
        {
            const BWAPI::TilePosition tilePos(x,y);
            if (!tilePos.isValid()) { continue; }

            if (Config::Debug::DrawWalkableSectors)
            {
                std::stringstream ss;
                ss << getSectorNumber(x, y);
                const BWAPI::Position pos = BWAPI::Position(tilePos) + BWAPI::Position(14,9);
                BWAPI::Broodwar->drawTextMap(pos, ss.str().c_str());
            }

            if (Config::Debug::DrawTileInfo)
            {
                BWAPI::Color color = isWalkable(x, y) ? BWAPI::Color(0, 255, 0) : BWAPI::Color(255, 0, 0);
                if (isWalkable(x, y) && !isBuildable(x, y)) { color = BWAPI::Color(255, 255, 0); }
                if (isBuildable(x, y) && !isDepotBuildableTile(x, y)) { color = BWAPI::Color(127, 255, 255); }
                drawTile(x, y, color);
            }
        }
    }
}

void MapTools::getUnits(BWAPI::Unitset & units, BWAPI::Position center, int radius, bool ourUnits, bool oppUnits)
{
	const int radiusSq(radius * radius);

	if (ourUnits)
	{
		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			BWAPI::Position d(unit->getPosition() - center);
			if(d.x * d.x + d.y * d.y <= radiusSq)
			{
				if (!units.contains(unit)) 
				{
					units.insert(unit);
				}
			}
		}
	}

	if (oppUnits)
	{
		for (auto & unit : BWAPI::Broodwar->enemy()->getUnits()) 
		{
            if (unit->getType() == BWAPI::UnitTypes::Unknown || !unit->isVisible())
            {
                continue;
            }

			BWAPI::Position d(unit->getPosition() - center);
			if(d.x * d.x + d.y * d.y <= radiusSq)
			{
				if (!units.contains(unit))
				{ 
					units.insert(unit); 
				}
			}
		}
	}
}

const StarDraftMap & MapTools::getStarDraftMap() const
{
    return m_map;
}
    
void MapTools::saveMapToFile(const std::string & path) const
{
    // replace spaces with underscores
    std::string mapFile = BWAPI::Broodwar->mapFileName();
    std::replace( mapFile.begin(), mapFile.end(), ' ', '_'); 
    getStarDraftMap().save(mapFile + ".txt");
}

// Define member variables to store the cached location and flag (Needs to be at the start i think) 
//BWAPI::TilePosition cachedChokepointPos = BWAPI::TilePositions::None;
//bool chokepointPosCached = false;

void MapTools::resetCachedData()
{
    // Reset the cached location and flag
    cachedChokepointPos = BWAPI::TilePositions::None;
    chokepointPosCached = false;
}

void MapTools::resetCachedDataNew()
{
    // Reset the cached location and flag
    cachedChokepointPosNew = BWAPI::TilePositions::None;
    chokepointPosCachedNew = false;
}


// Define the function to find the closest chokepoint to the enemy base

BWAPI::TilePosition MapTools::findCLosestChokepointPosEnemyNew()
{
    // If the location has been cached, return the cached value
    if (chokepointPosCachedNew) {
        // Return the cached chokepoint position
        return cachedChokepointPosNew;
    }

    BWAPI::TilePosition closestChokepointEnemy;

    // Iterate through all enemy units to find their main building
    for (auto& unit : BWAPI::Broodwar->enemy()->getUnits()) {
        //if (unit->getType().isBuilding() && unit->getType().isResourceDepot()) toto padalo pri teranovi z nejakeho dovodu
        if (unit->getType().isBuilding())
        {
            // Found the enemy main building (supply depot), use its position as the target location
            const BWAPI::TilePosition enemyMainBuildingPos = unit->getTilePosition();

            // Find the nearest area to the enemy base location
            const BWEM::Area* ourAreaEnemy = theMap.GetNearestArea(enemyMainBuildingPos);

            // Check if BWEM successfully found the nearest area
            if (ourAreaEnemy) {
                // Iterate through lengths from 3 range up to 10
                for (int length = 5; length <= 8; ++length) {
                    // Initialize variables to store the closest chokepoint
                    int minDistanceEnemy = INT_MAX;
                    bool foundChokepoint = false;

                    // Iterate through chokepoints in the nearest area
                    for (auto& chokepointEnemy : ourAreaEnemy->ChokePoints()) {
                        // Get the position of the chokepoint
                        BWAPI::TilePosition posEnemy = BWAPI::TilePosition(chokepointEnemy->Pos(chokepointEnemy->middle).x / 4, chokepointEnemy->Pos(chokepointEnemy->middle).y / 4);

                        // Calculate the distance from the chokepoint to the main building
                        int distanceToMainBuilding = posEnemy.getApproxDistance(enemyMainBuildingPos);

                        // Update the closest chokepoint if necessary
                        if (!closestChokepointEnemy || distanceToMainBuilding < minDistanceEnemy) {
                            closestChokepointEnemy = posEnemy;
                            minDistanceEnemy = distanceToMainBuilding;
                            foundChokepoint = true;
                        }
                    }

                    if (foundChokepoint) {
                        // Move the chokepoint towards the opposite direction by a constant distance of 'length' tiles
                        BWAPI::TilePosition directionVector = closestChokepointEnemy - enemyMainBuildingPos;

                        // Calculate magnitude of the direction vector using Pythagoras' theorem
                        int magnitudeSquared = directionVector.x * directionVector.x + directionVector.y * directionVector.y;
                        double magnitude = sqrt(static_cast<double>(magnitudeSquared));

                        // Normalize the direction vector
                        if (magnitude != 0) {
                            directionVector.x = static_cast<int>(round(directionVector.x / magnitude));
                            directionVector.y = static_cast<int>(round(directionVector.y / magnitude));
                        }

                        // Limit the distance to the specified length
                        int distance = length;

                        // Adjust the chokepoint position by moving it towards the opposite direction by the limited distance
                        closestChokepointEnemy += directionVector * distance;

                        // Check if the target location is buildable
                        if (BWAPI::Broodwar->isBuildable(closestChokepointEnemy)) {
                            // Cache the adjusted chokepoint position and set the flag
                            cachedChokepointPosNew = closestChokepointEnemy;
                            chokepointPosCachedNew = true;

                            // Return the adjusted chokepoint position (closer to main building)
                            return closestChokepointEnemy;
                        }
                    }
                    else {
                        // If no chokepoint found in the opposite direction, try in the direction towards the enemy base
                        // You need to implement this logic based on your specific requirements or map layout
                        // For demonstration purposes, I'll just move in the direction towards the enemy base by a constant distance of 'length' tiles
                        BWAPI::TilePosition directionVector = enemyMainBuildingPos - closestChokepointEnemy;

                        // Move the chokepoint towards the enemy base by a constant distance of 'length' tiles
                        closestChokepointEnemy += directionVector * length;

                        // Check if the target location is buildable
                        if (BWAPI::Broodwar->isBuildable(closestChokepointEnemy)) {
                            // Cache the adjusted chokepoint position and set the flag
                            cachedChokepointPosNew = closestChokepointEnemy;
                            chokepointPosCachedNew = true;

                            // Return the adjusted chokepoint position (closer to main building)
                            return closestChokepointEnemy;
                        }
                    }
                }

                // Return None if unable to find a suitable buildable location within all the lengths
                return BWAPI::TilePositions::None;
            }
        }
    }

    // Return None if the enemy main building is not found or BWEM failed to find the nearest area
    return BWAPI::TilePositions::None;
}




BWAPI::TilePosition MapTools::findCLosestChokepointPosEnemy()
{
    // If the location has been cached, return the cached value
    if (chokepointPosCached) {
        return cachedChokepointPos;
    }

    // Iterate through all enemy units to find their main building
    for (auto& unit : BWAPI::Broodwar->enemy()->getUnits()) {
        //if (unit->getType().isBuilding() && unit->getType().isResourceDepot()) toto padalo pri teranovi z nejakeho dovodu
        if (unit->getType().isBuilding() ) 
        {
            // Found the enemy main building, use its position as the enemy base location
            const BWAPI::TilePosition startLocationEnemy(unit->getTilePosition());

            // Find the nearest area to the enemy base location
            const BWEM::Area* ourAreaEnemy = theMap.GetNearestArea(startLocationEnemy);

            // Check if BWEM successfully found the nearest area

            if (ourAreaEnemy) {
                // Initialize variables to store the closest chokepoint
                BWAPI::TilePosition closestChokepointEnemy;
                int distanceFromHomeEnemy, minDistanceEnemy = 100000;

                // Iterate through chokepoints in the nearest area
                for (auto& chokepointEnemy : ourAreaEnemy->ChokePoints()) {
                    // Get the position of the chokepoint
                    BWAPI::TilePosition posEnemy = BWAPI::TilePosition(chokepointEnemy->Pos(chokepointEnemy->middle).x / 4, chokepointEnemy->Pos(chokepointEnemy->middle).y / 4);

                    // Calculate the distance from the enemy base to the chokepoint
                    distanceFromHomeEnemy = posEnemy.getDistance(startLocationEnemy);

                    // Update the closest chokepoint if necessary
                    if (!closestChokepointEnemy || distanceFromHomeEnemy < minDistanceEnemy) {
                        closestChokepointEnemy = posEnemy;
                        minDistanceEnemy = distanceFromHomeEnemy;
                    }
                }

                // Cache the result and set the flag
                cachedChokepointPos = closestChokepointEnemy;
                chokepointPosCached = true;

                // Return the closest chokepoint
                return closestChokepointEnemy;
            }
            
        }
    }

    // Return None if the enemy main building is not found or BWEM failed to find the nearest area
    return BWAPI::TilePositions::None;
}



BWAPI::TilePosition MapTools::findCLosestChokepointPos()
{
    BWAPI::TilePosition startLocation = BWAPI::Broodwar->self()->getStartLocation();

    //std::cout << "StartLocmy" << startLocation << "\n";

    const BWEM::Area* ourArea = theMap.GetNearestArea(startLocation);

    //std::cout << "StartmyArea" << ourArea << "\n";
    BWAPI::TilePosition closestChokepoint;
    int distanceFromHome, minDistance = 100000;

    for (auto& chokepoint : ourArea->ChokePoints()) {
        BWAPI::TilePosition pos = BWAPI::TilePosition(chokepoint->Pos(chokepoint->middle).x /4 , chokepoint->Pos(chokepoint->middle).y /4);
        
        distanceFromHome = pos.getDistance(startLocation);
        
        if (!closestChokepoint || distanceFromHome < minDistance)
        {
            closestChokepoint = pos;
            minDistance = distanceFromHome;
        }

    }

    return closestChokepoint;
}

const BWEM::ChokePoint* MapTools::findCLosestChokepoint() const
{
    BWAPI::TilePosition startLocation = BWAPI::Broodwar->self()->getStartLocation();
    const BWEM::Area* ourArea = theMap.GetNearestArea(startLocation);

    //std::cout << "ChokepointS" << std::endl;
    BWAPI::TilePosition closestChokepointPos;
    const BWEM::ChokePoint* closestChokepoint = NULL;
    int distanceFromHome, minDistance = 100000;

    for (auto& chokepoint : ourArea->ChokePoints()) {
        BWAPI::TilePosition pos = BWAPI::TilePosition(chokepoint->Pos(chokepoint->middle).x /4 , chokepoint->Pos(chokepoint->middle).y /4);
        
        distanceFromHome = pos.getDistance(startLocation);
        
        if (!closestChokepoint || distanceFromHome < minDistance)
        {
            closestChokepointPos = pos;
            closestChokepoint = chokepoint;
            minDistance = distanceFromHome;
        }

    }

    return closestChokepoint;
}

bool MapTools::isAccessiblefromBWEM(BWAPI::TilePosition target, BWAPI::TilePosition from) const {
    int lenght = -1;
    bool blocked = false;
    const BWEM::CPPath path = theMap.GetPath(BWAPI::Position(target.x * 32, target.y * 32), BWAPI::Position(from.x * 32, from.y * 32), &lenght);

    for (auto& choke : path) {

        if (choke->Blocked() || !choke->AccessibleFrom(findCLosestChokepoint())) {
            blocked = true;
        }
    }
    if (blocked || lenght == -1) {
        return false;
    }
    return true;
}
bool MapTools::isAccessiblefromBWEM(BWAPI::Position target, BWAPI::Position from) const {
    int lenght = -1;
    bool blocked = false;
    const BWEM::CPPath path = theMap.GetPath(target, from, &lenght);

    for (auto& choke : path) {

        if (choke->Blocked() || !choke->AccessibleFrom(findCLosestChokepoint())) {
            blocked = true;
        }
    }
    if (blocked || lenght == -1) {
        return false;
    }
    return true;
}