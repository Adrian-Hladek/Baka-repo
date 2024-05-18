/* 
 +----------------------------------------------------------------------+
 | UAlbertaBot                                                          |
 +----------------------------------------------------------------------+
 | University of Alberta - AIIDE StarCraft Competition                  |
 +----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------+
 | Author: David Churchill <dave.churchill@gmail.com>                   |
 +----------------------------------------------------------------------+
*/

#include "Common.h"
#include "UAlbertaBotModule.h"
#include "JSONTools.h"
#include "ParseUtils.h"
#include "UnitUtil.h"
#include "Global.h"
#include "StrategyManager.h"
#include "MapTools.h"



#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>



using namespace UAlbertaBot;
UAlbertaBotModule::UAlbertaBotModule()
{
    Global::GameStart();
}

// This gets called when the bot starts!
void UAlbertaBotModule::onStart()
{
    // Parse the bot's configuration file if it has one, change this file path to where your config file is
    // Any relative path name will be relative to Starcraft installation folder
    ParseUtils::ParseConfigFile(Config::ConfigFile::ConfigFileLocation);

    // Set our BWAPI options here    
	BWAPI::Broodwar->setLocalSpeed(Config::BWAPIOptions::SetLocalSpeed);
	BWAPI::Broodwar->setFrameSkip(Config::BWAPIOptions::SetFrameSkip);
    
    if (Config::BWAPIOptions::EnableCompleteMapInformation)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);
    }

    if (Config::BWAPIOptions::EnableUserInput)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
    }

    if (Config::BotInfo::PrintInfoOnStart)
    {
        BWAPI::Broodwar->printf("Hello! I am %s, written by %s", Config::BotInfo::BotName.c_str(), Config::BotInfo::Authors.c_str());
    }

    // Call BWTA to read and analyze the current map
    if (Config::Modules::UsingGameCommander)
	{
        if (Config::Modules::UsingStrategyIO)
        {
            Global::Strategy().readResults();
            Global::Strategy().setLearnedStrategy();
        }
	}

    //Global::Map().saveMapToFile("map.txt");
}

void UAlbertaBotModule::onEnd(bool isWinner) 
{
	if (Config::Modules::UsingGameCommander)
	{
		Global::Strategy().onEnd(isWinner);
	}
    if (isWinner) {
        std::cout << "Win\n";
        
        auto currentTime = std::chrono::system_clock::now();

        // Convert the time point to a time_t
        std::time_t time = std::chrono::system_clock::to_time_t(currentTime);

        // Convert the time_t to a string in a human-readable format
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        



        std::ofstream outputFile("Vzorky.txt", std::ios::app);

        // Check if the file is opened successfully
        if (outputFile.is_open()) {
            // Write text to the file
            outputFile << "Current time: " << buffer << "\t";
            outputFile << "Win\t";

            outputFile << "Framecount: " << BWAPI::Broodwar->getFrameCount() << "\n";
            // Close the file when done
            outputFile.close();

            std::cout << "Text has been written to the file." << std::endl;
        }
        else {
            std::cerr << "Unable to open the file." << std::endl;
        }
    
    }
    else {
        std::cout << "Lose\n";

        auto currentTime = std::chrono::system_clock::now();

        // Convert the time point to a time_t
        std::time_t time = std::chrono::system_clock::to_time_t(currentTime);

        // Convert the time_t to a string in a human-readable format
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        

        
        std::ofstream outputFile("Vzorky.txt", std::ios::app);

        // Check if the file is opened successfully
        if (outputFile.is_open()) {
            // Write text to the file

            outputFile << "Current time: " << buffer << "\t";

            outputFile << "Lose\t";

            outputFile << "Framecount: " << BWAPI::Broodwar->getFrameCount() << "\n";
            // Close the file when done
            outputFile.close();

            std::cout << "Text has been written to the file." << std::endl;
        }
        else {
            std::cerr << "Unable to open the file." << std::endl;
        }
        


    }
}// chost

void UAlbertaBotModule::onFrame()
{
    if (BWAPI::Broodwar->getFrameCount() > 100000)
    {
        BWAPI::Broodwar->restartGame();
    }

    const char red = '\x08';
    const char green = '\x07';
    const char white = '\x04';

    if (!Config::ConfigFile::ConfigFileFound)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Not Found", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without its configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cCheck that the file below exists. Incomplete paths are relative to Starcraft directory", white);
        BWAPI::Broodwar->drawTextScreen(10, 60, "%cYou can change this file location in Config::ConfigFile::ConfigFileLocation", white);
        BWAPI::Broodwar->drawTextScreen(10, 75, "%cFile Not Found (or is empty): %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }
    else if (!Config::ConfigFile::ConfigFileParsed)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Parse Error", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without a properly formatted configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cThe configuration file was found, but could not be parsed. Check that it is valid JSON", white);
        BWAPI::Broodwar->drawTextScreen(10, 60, "%cFile Not Parsed: %c %s", white, green, Config::ConfigFile::ConfigFileLocation.c_str());
        return;
    }

	if (Config::Modules::UsingGameCommander) 
	{ 
		m_gameCommander.update(); 
	}

    if (Config::Modules::UsingAutoObserver)
    {
        m_autoObserver.onFrame();
    }
}

void UAlbertaBotModule::onUnitDestroy(BWAPI::Unit unit)
{
	if (Config::Modules::UsingGameCommander) { m_gameCommander.onUnitDestroy(unit);
    //std::cout << "Destroy 1\n";
    }
}

void UAlbertaBotModule::onUnitMorph(BWAPI::Unit unit)
{
	if (Config::Modules::UsingGameCommander) { m_gameCommander.onUnitMorph(unit); }
}

void UAlbertaBotModule::onSendText(std::string text) 
{ 
	ParseUtils::ParseTextCommand(text);
}

void UAlbertaBotModule::onUnitCreate(BWAPI::Unit unit)
{ 
	if (Config::Modules::UsingGameCommander) { m_gameCommander.onUnitCreate(unit); 
   
    //if (unit->getPlayer()->getName() == "Chost"){std::cout << unit->getType() << "\n";   }
    
    }
}

void UAlbertaBotModule::onUnitComplete(BWAPI::Unit unit)
{
    if (Config::Modules::UsingGameCommander) {
        m_gameCommander.onUnitComplete(unit);

        /*if (unit->getPlayer()->getName() == "Chost")
        {
            std::cout << "unit complete" << unit->getType() << "\n";
        }*/
    }
}

void UAlbertaBotModule::onUnitShow(BWAPI::Unit unit)
{ 
	if (Config::Modules::UsingGameCommander) { m_gameCommander.onUnitShow(unit); }
}

void UAlbertaBotModule::onUnitHide(BWAPI::Unit unit)
{ 
	if (Config::Modules::UsingGameCommander) { m_gameCommander.onUnitHide(unit); }
}

void UAlbertaBotModule::onUnitRenegade(BWAPI::Unit unit)
{ 
	if (Config::Modules::UsingGameCommander) { m_gameCommander.onUnitRenegade(unit); }
}