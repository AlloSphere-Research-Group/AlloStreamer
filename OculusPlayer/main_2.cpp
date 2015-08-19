
#include "Renderer.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/ref.hpp>

#include "AlloShared/Stats.hpp"
#include "AlloReceiver/AlloReceiver.h"

const unsigned int DEFAULT_SINK_BUFFER_SIZE = 200000000;

Stats stats;

void onNextCubemap(CubemapSource* source, StereoCubemap* cubemap)
{
    for (int i = 0; i < cubemap->getEye(0)->getFacesCount(); i++)
    {
        stats.displayedCubemapFace(i);
    }
    stats.displayedFrame();
    StereoCubemap::destroy(cubemap);
}

void onDroppedNALU(CubemapSource* source, int face, uint8_t type, size_t size)
{
    stats.droppedNALU(type, size, face);
}

void onAddedNALU(CubemapSource* source, int face, uint8_t type, size_t size)
{
    stats.addedNALU(type, size, face);
}

void onDisplayedCubemapFace(Renderer* renderer, int face)
{
    stats.displayedCubemapFace(face);
}

void onDisplayedFrame(Renderer* renderer)
{
    stats.displayedFrame();
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        boost::filesystem::path exePath(argv[0]);
        std::cout << "usage: " << exePath.filename().string() << " <RTSP url of stream>" << std::endl;
        return -1;
    }
    
    boost::program_options::options_description desc("");
    desc.add_options()
        ("no-display", "")
        ("url", boost::program_options::value<std::string>(), "url")
        ("interface", boost::program_options::value<std::string>(), "interface");
    
    boost::program_options::positional_options_description p;
    p.add("url", -1);
    
    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
              options(desc).positional(p).run(), vm);
    boost::program_options::notify(vm);
    
    const char* _interface;
    if (vm.count("interface"))
    {
        _interface = vm["interface"].as<std::string>().c_str();
    }
    else
    {
        _interface = "0.0.0.0";
    }

	CubemapSource* cubemapSource = CubemapSource::createFromRTSP(vm["url"].as<std::string>().c_str(), DEFAULT_SINK_BUFFER_SIZE, 1024, AV_PIX_FMT_RGBA, _interface);
    
    std::function<void (CubemapSource*, int, uint8_t, size_t)> callback = boost::bind(&onDroppedNALU, _1, _2, _3, _4);
    cubemapSource->setOnDroppedNALU(callback);
    callback = boost::bind(&onAddedNALU, _1, _2, _3, _4);
    cubemapSource->setOnAddedNALU(callback);
    
    stats.autoSummary(boost::chrono::seconds(10));
    
   
    Renderer renderer(cubemapSource);
    std::function<void (Renderer*, int)> onDisplayedCubemapFaceCallback = boost::bind(&onDisplayedCubemapFace, _1, _2);
    renderer.setOnDisplayedCubemapFace(onDisplayedCubemapFaceCallback);
    std::function<void (Renderer*)> onDisplayedFrameCallback = boost::bind(&onDisplayedFrame, _1);
    renderer.setOnDisplayedFrame(onDisplayedFrameCallback);
    renderer.start(); // Returns when window is closed
    
    CubemapSource::destroy(cubemapSource);
}