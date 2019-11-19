#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <iomanip>

#include "Renderer.hpp"
#include "AlloShared/StatsUtils.hpp"
#include "AlloShared/to_human_readable_byte_count.hpp"
#include "AlloShared/CommandHandler.hpp"
#include "AlloShared/Console.hpp"
#include "AlloShared/Config.hpp"
#include "AlloShared/CommandLine.hpp"
#include "AlloReceiver/RTSPCubemapSourceClient.hpp"
#include "AlloReceiver/AlloReceiver.h"
#include "AlloReceiver/Stats.hpp"
#include "AlloReceiver/H264CubemapSource.h"

#define DEG_DIV_RAD 57.29577951308233
#define RAD_DIV_DEG  0.01745329251994

const unsigned int DEFAULT_SINK_BUFFER_SIZE = 2000000000;

static Stats         stats;
static Renderer      renderer;
static auto          lastStatsTime    = std::chrono::steady_clock::now();
static std::string   statsFormat      = AlloReceiver::formatStringMaker();
static bool          noDisplay        = false;
static std::string   url              = "";
static std::string   interfaceAddress = "0.0.0.0";
static unsigned long bufferSize       = DEFAULT_SINK_BUFFER_SIZE;
static bool          matchStereoPairs = false;
static boost::filesystem::path configFilePath;
static bool          robustSyncing    = false;
static size_t        maxFrameMapSize  = 2;
static std::string   logPath          = ".";

StereoCubemap* onNextCubemap(CubemapSource* source, StereoCubemap* cubemap)
{
    for (int i = 0; i < cubemap->getEye(0)->getFacesCount(); i++)
    {
        stats.store(StatsUtils::CubemapFace(i, StatsUtils::CubemapFace::DISPLAYED));
    }
    stats.store(StatsUtils::Cubemap());
    return cubemap;
}

void onReceivedNALU(CubemapSource* source, u_int8_t type, size_t size, int face)
{
    //stats.store(StatsUtils::NALU(type, size, face, StatsUtils::NALU::RECEIVED));
}

void onReceivedFrame(CubemapSource* source, u_int8_t type, size_t size, int face)
{
    //stats.store(StatsUtils::Frame(type, size, face, StatsUtils::Frame::RECEIVED));
}

void onDecodedFrame(CubemapSource* source, u_int8_t type, size_t size, int face)
{
    //stats.store(StatsUtils::Frame(type, size, face, StatsUtils::Frame::DECODED));
}

void onColorConvertedFrame(CubemapSource* source, u_int8_t type, size_t size, int face)
{
    stats.store(StatsUtils::Frame(type, size, face, StatsUtils::Frame::COLOR_CONVERTED));
}

void onAddedFrameToCubemap(CubemapSource* source, int face)
{
    //stats.store(StatsUtils::CubemapFace(face, StatsUtils::CubemapFace::ADDED));
}

void setOnScheduledFrameInCubemap(CubemapSource* source, int face)
{
    stats.store(StatsUtils::CubemapFace(face, StatsUtils::CubemapFace::SCHEDULED));
}

void onDisplayedCubemapFace(Renderer* renderer, int face)
{
    stats.store(StatsUtils::CubemapFace(face, StatsUtils::CubemapFace::DISPLAYED));
}

void onDisplayedFrame(Renderer* renderer)
{
    stats.store(StatsUtils::Cubemap());
}

void onDidConnect(RTSPCubemapSourceClient* client, CubemapSource* cubemapSource)
{
    H264CubemapSource* h264CubemapSource = dynamic_cast<H264CubemapSource*>(cubemapSource);
    if (h264CubemapSource)
    {
        using namespace std::placeholders;
        h264CubemapSource->setOnReceivedNALU           (std::bind(&onReceivedNALU,               _1, _2, _3, _4));
        h264CubemapSource->setOnReceivedFrame          (std::bind(&onReceivedFrame,              _1, _2, _3, _4));
        h264CubemapSource->setOnDecodedFrame           (std::bind(&onDecodedFrame,               _1, _2, _3, _4));
        h264CubemapSource->setOnColorConvertedFrame    (std::bind(&onColorConvertedFrame,        _1, _2, _3, _4));
        h264CubemapSource->setOnAddedFrameToCubemap    (std::bind(&onAddedFrameToCubemap,        _1, _2));
        h264CubemapSource->setOnScheduledFrameInCubemap(std::bind(&setOnScheduledFrameInCubemap, _1, _2));
    }
    
    if (noDisplay)
    {
        using namespace std::placeholders;
        cubemapSource->setOnNextCubemap(std::bind(&onNextCubemap, _1, _2));
    }
    else
    {
        renderer.setCubemapSource(cubemapSource);
    }
}



int main(int argc, char* argv[])
{
    std::initializer_list<CommandHandler::Command> generalCommands =
    {
        {
            "gamma-min",
            {"val"},
            [](const std::vector<std::string>& values)
            {
                renderer.setGammaMin(boost::lexical_cast<float>(values[0]));
            }
        },
        {
            "gamma-max",
            {"val"},
            [](const std::vector<std::string>& values)
            {
                renderer.setGammaMax(boost::lexical_cast<float>(values[0]));
            }
        },
        {
            "gamma-pow",
            {"val"},
            [](const std::vector<std::string>& values)
            {
                renderer.setGammaPow(boost::lexical_cast<float>(values[0]));
            }
        },
        {
            "for-rotation",
            {"α°", "β°", "γ°"},
            [](const std::vector<std::string>& values)
            {
                renderer.setFORRotation(al::Vec3f(boost::lexical_cast<float>(values[0]) * RAD_DIV_DEG,
                                                  boost::lexical_cast<float>(values[1]) * RAD_DIV_DEG,
                                                  boost::lexical_cast<float>(values[2]) * RAD_DIV_DEG));
            }
        },
        {
            "for-angle",
            {"°"},
            [](const std::vector<std::string>& values)
            {
                renderer.setFORAngle(boost::lexical_cast<float>(values[0]) * RAD_DIV_DEG);
            }
        },
        {
            "rotation",
            {"α°", "β°", "γ°"},
            [](const std::vector<std::string>& values)
            {
                renderer.setRotation(al::Vec3f(boost::lexical_cast<float>(values[0]) * RAD_DIV_DEG,
                                               boost::lexical_cast<float>(values[1]) * RAD_DIV_DEG,
                                               boost::lexical_cast<float>(values[2]) * RAD_DIV_DEG));
            }
        },
        {
            "rotation-speed",
            {"val"},
            [](const std::vector<std::string>& values)
            {
                renderer.setRotationSpeed(boost::lexical_cast<float>(values[0]));
            }
        },
        {
            "force-mono",
            {"yes|no"},
            [](const std::vector<std::string>& values)
            {
                if (values[0] == "yes")
                {
                    renderer.setForceMono(true);
                }
                else if (values[0] == "no")
                {
                    renderer.setForceMono(false);
                }
                else
                {
                    throw std::invalid_argument("Only yes or no are valid values");
                }
            }
        },
    };
    
    CommandHandler configCommandHandler({});
    
    std::initializer_list<CommandHandler::Command> configCommandLineOnlyCommands =
    {
        {
            "config",
            {"file_path"},
            [&configCommandHandler](const std::vector<std::string>& values)
            {
                configFilePath = boost::filesystem::canonical(boost::filesystem::absolute(values[0], configFilePath.parent_path()));
                
                auto configParseResult = Config::parseConfigFile(configCommandHandler,
                                                                 configFilePath.string());
                if (!configParseResult.first)
                {
                    std::cerr << configParseResult.second << std::endl;
                    std::cerr << configCommandHandler.getCommandHelpString();
                    abort();
                }
            }
        },
        {
            "no-display",
            {},
            [](const std::vector<std::string>& values)
            {
                noDisplay = true;
            }
        },
        {
            "url",
            {"rtsp_url"},
            [](const std::vector<std::string>& values)
            {
                url = values[0];
            }
        },
        {
            "interface-address",
            {"ip"},
            [](const std::vector<std::string>& values)
            {
                interfaceAddress = values[0];
            }
        },
        {
            "buffer-size",
            {"bytes"},
            [](const std::vector<std::string>& values)
            {
                bufferSize = boost::lexical_cast<unsigned long>(values[0]);
            }
        },
        {
            "match-stereo-pairs",
            {},
            [](const std::vector<std::string>& values)
            {
                matchStereoPairs = true;
            }
        },
        {
            "robust-syncing",
            {},
            [](const std::vector<std::string>& values)
            {
                robustSyncing = true;
            }
        },
        {
            "cubemap-queue-size",
            {"size"},
            [](const std::vector<std::string>& values)
            {
                maxFrameMapSize = boost::lexical_cast<size_t>(values[0]);
            }
        }
    };
    
    std::initializer_list<CommandHandler::Command> consoleOnlyCommands =
    {
        {
            "stats",
            {},
            [](const std::vector<std::string>& values)
            {
                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                std::cout << stats.summary(std::chrono::duration_cast<std::chrono::microseconds>(now - lastStatsTime),
                                           AlloReceiver::statValsMaker,
                                           AlloReceiver::postProcessorMaker,
                                           statsFormat);
                lastStatsTime = now;
            }
        },
        {
            "quit",
            {},
            [](const std::vector<std::string>& values)
            {
                exit(0);
            }
        },
        {
            "info",
            {},
            [](const std::vector<std::string>& values)
            {
                al::Vec3f forRotation = renderer.getFORRotation() * DEG_DIV_RAD;
                al::Vec3f rotation    = renderer.getRotation()    * DEG_DIV_RAD;
                
                std::cout << std::setiosflags(std::ios::fixed) << std::setprecision(1);
                std::cout << "Display:            " << ((noDisplay) ? "no" : "yes") << std::endl;
                std::cout << "RTSP URL:           " << url << std::endl;
                std::cout << "Interface address:  " << interfaceAddress << std::endl;
                std::cout << "Buffer size:        " << to_human_readable_byte_count(bufferSize, false, false) << std::endl;
                std::cout << "Match stereo pairs: " << ((matchStereoPairs) ? "yes" : "no") << std::endl;
                std::cout << "Gamma min:          " << renderer.getGammaMin() << std::endl;
                std::cout << "Gamma max:          " << renderer.getGammaMax() << std::endl;
                std::cout << "Gamma pow:          " << renderer.getGammaPow() << std::endl;
                std::cout << "FOR angle:          " << renderer.getFORAngle() * DEG_DIV_RAD << "°" << std::endl;
                std::cout << "FOR rotation:       " << "α=" << forRotation[0] << "°\t"
                                                    << "β=" << forRotation[1] << "°\t"
                                                    << "γ=" << forRotation[2] << "°" << std::endl;
                std::cout << "Scene rotation:     " << "α=" << rotation[0] << "°\t"
                                                    << "β=" << rotation[1] << "°\t"
                                                    << "γ=" << rotation[2] << "°" << std::endl;
                std::cout << "Rotation speed:     " << renderer.getRotationSpeed() << std::endl;
                std::cout << "Config file:        " << configFilePath << std::endl;
                std::cout << "Face resolutions:   ";
                auto faceResokutions = renderer.getFaceResolutions();
                for (int eye = 0; eye < StereoCubemap::MAX_EYES_COUNT; eye++)
                {
                    for (int face = 0; face < Cubemap::MAX_FACES_COUNT; face++)
                    {
                        auto faceResolution = faceResokutions[eye * Cubemap::MAX_FACES_COUNT + face];
                        std::cout << faceResolution.first << "x" << faceResolution.second << "px ";
                    }
                    std::cout << std::endl << "                    ";
                }
                std::cout << std::endl;
                std::cout << "Robust syncing:     " << ((robustSyncing) ? "yes" : "no") << std::endl;
                std::cout << "Cubemap queue size: " << maxFrameMapSize << std::endl;
                std::cout << "Force mono:         " << ((renderer.getForceMono()) ? "yes" : "no") << std::endl;
            }
        }
    };
    
    CommandHandler consoleCommandHandler({generalCommands, consoleOnlyCommands});
    configCommandHandler = CommandHandler({generalCommands, configCommandLineOnlyCommands});
    CommandHandler commandLineCommandHandler({generalCommands, configCommandLineOnlyCommands});
    
    auto commandLineParseResult = CommandLine::parseCommandLine(commandLineCommandHandler,
                                                                argc,
                                                                argv);
    if (!commandLineParseResult.first)
    {
        std::cerr << commandLineParseResult.second << std::endl;
        std::cerr << commandLineCommandHandler.getCommandHelpString();
        abort();
    }
    
    if (url == "")
    {
        std::cerr << "No URL specified." << std::endl;
        abort();
    }
    
    Console console(consoleCommandHandler);
    console.start();
    
    
    RTSPCubemapSourceClient* rtspClient = RTSPCubemapSourceClient::create(url.c_str(),
                                                                          bufferSize,
                                                                          AV_PIX_FMT_YUV420P,
                                                                          matchStereoPairs,
                                                                          robustSyncing,
                                                                          maxFrameMapSize,
                                                                          interfaceAddress.c_str());

    using namespace std::placeholders;
    rtspClient->setOnDidConnect(std::bind(&onDidConnect, _1, _2));
    rtspClient->connect();
    
    
    
    if (noDisplay)
    {
        std::cout << "network only" << std::endl;
        while(true)
        {
            std::this_thread::yield();
        }
    }
    else
    {
        renderer.setOnDisplayedCubemapFace(std::bind(&onDisplayedCubemapFace, _1, _2));
        renderer.setOnDisplayedFrame(std::bind(&onDisplayedFrame, _1));
        renderer.start(); // does not return
    }
}
