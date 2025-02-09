

#include "Common.h"
//#include "C:\Users\ahlad\Desktop\Bot_Bakalarka\UAlbertaBot-FIIT-Protoss\UAlbertaBot\Source\StrategyManager.cpp"


// SEARCH PARAMETERS
char SPARCRAFT_LOGFILE[100] { "sparcraft_error_log.txt" };

namespace SparCraft
{
    namespace System
    {
        void FatalError(const std::string & errorMessage)
        {
            std::cerr << "\n\n\nSparCraft Fatal Error: \n\n\n      " << errorMessage << "\n\n";

			/*std::ofstream logStream;
			logStream.open(SPARCRAFT_LOGFILE, std::ofstream::app);
			logStream << "\n\n\nSparCraft Fatal Error: \n\n\n      " << errorMessage << "\n\n";
			logStream.flush();
			logStream.close();*/

            throw(SPARCRAFT_FATAL_ERROR);
        }
        
        void checkSupportedUnitType(const BWAPI::UnitType & type)
        {
            if (type == BWAPI::UnitTypes::None || type == BWAPI::UnitTypes::Unknown)
            {
                std::cout << "Unknown unit type in experiment file, not supported";
                System::FatalError("Unknown unit type in experiment file, not supported");
            }

            if (type == BWAPI::UnitTypes::Protoss_Corsair ||
                type == BWAPI::UnitTypes::Zerg_Devourer || 
                type == BWAPI::UnitTypes::Zerg_Scourge ||
                type == BWAPI::UnitTypes::Terran_Valkyrie)
            { 
                std::cout << "Units with just air weapons currently not supported correctly : " << type.getName();
                System::FatalError("Units with just air weapons currently not supported correctly: " + type.getName());
                }
            

            if (type.isBuilding() && !(type == BWAPI::UnitTypes::Protoss_Photon_Cannon || type == BWAPI::UnitTypes::Zerg_Sunken_Colony || type == BWAPI::UnitTypes::Terran_Missile_Turret))
            {
                System::FatalError("Non-attacking buildings not currently supported: " + type.getName());
            }

            if (type.isSpellcaster())
            {
                System::FatalError("Spell casting units not currently supported: " + type.getName());
            }

             // Don't support units loading other units yet
            if (type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine || 
                type == BWAPI::UnitTypes::Protoss_Carrier || 
                type == BWAPI::UnitTypes::Protoss_Interceptor || 
                type == BWAPI::UnitTypes::Protoss_Reaver ||
                type == BWAPI::UnitTypes::Protoss_Scarab ||
                type == BWAPI::UnitTypes::Zerg_Broodling)
            {

                std::cout << "Units with just air weapons currently not supported correctly : " << type.getName();
                System::FatalError("Units which have unit projectiles not supported: " + type.getName());
            }
        }

        bool isSupportedUnitType(const BWAPI::UnitType & type)
        {
            if (type == BWAPI::UnitTypes::None || type == BWAPI::UnitTypes::Unknown)
            {
                return false;
            }

            if (type == BWAPI::UnitTypes::Protoss_Corsair ||
                type == BWAPI::UnitTypes::Zerg_Devourer || 
                type == BWAPI::UnitTypes::Zerg_Scourge ||
                type == BWAPI::UnitTypes::Terran_Valkyrie)
            {

                return false;
            }

            

            if (type.isBuilding() && !(type == BWAPI::UnitTypes::Protoss_Photon_Cannon || type == BWAPI::UnitTypes::Zerg_Sunken_Colony || type == BWAPI::UnitTypes::Terran_Missile_Turret))
            {
                return false;
            }

            if (type.isSpellcaster())
            {
                return false;
            }

            // Don't support units loading other units yet
            if (type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine || 
                type == BWAPI::UnitTypes::Protoss_Carrier || 
                type == BWAPI::UnitTypes::Protoss_Interceptor || 
                type == BWAPI::UnitTypes::Protoss_Reaver ||
                type == BWAPI::UnitTypes::Protoss_Scarab ||
                type == BWAPI::UnitTypes::Zerg_Broodling)
            {
                return false;
            }

            return true;
        }
    }
};
