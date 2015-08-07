#include <boost/chrono/system_clocks.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/sum.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/thread/thread.hpp>

#include "to_human_readable_byte_count.hpp"
#include "Stats.hpp"

namespace bc = boost::chrono;
namespace ba = boost::accumulators;
namespace bad = boost::adaptors;
namespace bf = boost::fusion;

template <typename ValueType>
Stats::TimeValueDatum<ValueType>::TimeValueDatum(ValueType value)
:
timeSinceEpoch(bc::duration_cast<bc::microseconds>(bc::system_clock::now().time_since_epoch())),
value(value)
{
}

Stats::NALU::NALU(int type, size_t size)
	:
	type(type),
	size(size)
{
}

template <typename ValueType>
boost::function<bool (Stats::TimeValueDatum<ValueType>)> Stats::timeFilter(
    bc::microseconds window,
    bc::microseconds nowSinceEpoch)
{
    return [window, nowSinceEpoch](Stats::TimeValueDatum<ValueType> datum)
    {
        return (nowSinceEpoch - datum.timeSinceEpoch) < window;
    };
}

template <typename Features, typename ValueType, typename AccType>
ba::accumulator_set<AccType, Features> Stats::filter(
    std::vector<TimeValueDatum<ValueType> >& data,
    std::initializer_list<boost::function<bool (TimeValueDatum<ValueType>)> > filters,
	boost::function<AccType(ValueType)> accExtractor)
{
	ba::accumulator_set<AccType, Features> acc;
    
	auto filterAcc = [filters, &acc, accExtractor](TimeValueDatum<ValueType> datum) {
		bool passed = true;
		for (auto filter : filters)
		{
			if (!filter(datum))
			{
				passed = false;
				break;
			}
		}
		if (passed)
		{
			acc(accExtractor(datum.value));
		}
	};

	std::for_each(data.begin(), data.end(), filterAcc);

    return acc;
}


bc::microseconds Stats::nowSinceEpoch()
{
    return bc::duration_cast<bc::microseconds>(bc::system_clock::now().time_since_epoch());
}

std::string Stats::formatDuration(bc::microseconds duration)
{
    std::stringstream result;
    bc::microseconds remainder(duration);
    
    bc::hours h = bc::duration_cast<bc::hours>(remainder);
    if (h.count() > 0)
    {
        result << h.count() << "h ";
        remainder -= h;
    }
    
    bc::minutes m = bc::duration_cast<bc::minutes>(remainder);
    if (m.count() > 0)
    {
        result << m.count() << "m ";
        remainder -= m;
    }
    
    bc::seconds s = bc::duration_cast<bc::seconds>(remainder);
    if (s.count() > 0)
    {
        result << s.count() << "s ";
        remainder -= s;
    }
    
    bc::milliseconds ms = bc::duration_cast<bc::milliseconds>(remainder);
    if (ms.count() > 0)
    {
        result << ms.count() << "ms ";
        remainder -= ms;
    }
    
    bc::microseconds us = bc::duration_cast<bc::microseconds>(remainder);
    if (us.count() > 0)
    {
        result << us.count() << "µs ";
        remainder -= us;
    }
    
    std::string format = result.str();
    return format.substr(0, format.size()-1);
}

// ###### EVENTS ######

void Stats::droppedNALU(int type, size_t size)
{
    boost::mutex::scoped_lock lock(mutex);
    droppedNALUs.push_back(TimeValueDatum<NALU>(NALU(type, size)));
}

void Stats::addedNALU(int type, size_t size)
{
    boost::mutex::scoped_lock lock(mutex);
	addedNALUs.push_back(TimeValueDatum<NALU>(NALU(type, size)));
}

void Stats::displayedCubemapFace(int face)
{
    boost::mutex::scoped_lock lock(mutex);
    displayedCubemapFaces.push_back(TimeValueDatum<int>(face));
}

void Stats::displayedFrame()
{
    boost::mutex::scoped_lock lock(mutex);
    displayedFrames.push_back(TimeValueDatum<int>(0));
}

// ###### STATISTICAL VALUES ######

double Stats::naluDropRate(bc::microseconds window, bc::microseconds nowSinceEpoch)
{
    boost::mutex::scoped_lock lock(mutex);
    
	auto accDropped = filter<ba::features<ba::tag::count>, NALU, NALU>(droppedNALUs,
	                                                                   { timeFilter<NALU>(window,
	                                                                                      nowSinceEpoch) },
																       [](NALU nalu) { return nalu; });
    auto accAdded = filter<ba::features<ba::tag::count>, NALU, NALU>(addedNALUs,
	                                                                 { timeFilter<NALU>(window,
	                                                                                    nowSinceEpoch) },
																	   [](NALU nalu) { return nalu; });
    
    return (double)ba::count(accDropped) / (double)ba::count(accAdded);
}

double Stats::cubemapFaceFramesPS(int face,
    boost::chrono::microseconds window,
    boost::chrono::microseconds nowSinceEpoch)
{
    boost::mutex::scoped_lock lock(mutex);
    
    auto accDisplayedCubemapFaces = filter<ba::features<ba::tag::count>, int, int>(displayedCubemapFaces,
	                                                                          { timeFilter<int>(window, nowSinceEpoch),
																			    [face](Stats::TimeValueDatum<int> datum) { return datum.value == face; } },
																				[](int face) { return face; });
    
    return (double)ba::count(accDisplayedCubemapFaces) / bc::duration_cast<bc::seconds>(window).count();
}

double Stats::fps(boost::chrono::microseconds window,
    boost::chrono::microseconds nowSinceEpoch)
{
    boost::mutex::scoped_lock lock(mutex);
    
    auto accDisplayedFrames = filter<ba::features<ba::tag::count>, int, int>(displayedFrames,
	                                                                   { timeFilter<int>(window,
	                                                                     nowSinceEpoch) },
																		 [](int fps) { return fps; });
    
    return (double)ba::count(accDisplayedFrames) / bc::duration_cast<bc::seconds>(window).count();
}

double Stats::receivedNALUsPS(boost::chrono::microseconds window,
    boost::chrono::microseconds nowSinceEpoch)
{
    boost::mutex::scoped_lock lock(mutex);
    
	auto accDropped = filter<ba::features<ba::tag::count>, NALU, NALU>(droppedNALUs,
	                                                             { timeFilter<NALU>(window,
	                                                                                nowSinceEpoch) },
																					[](NALU nalu) { return nalu; });
	auto accAdded = filter<ba::features<ba::tag::count>, NALU, NALU>(addedNALUs,
	                                                           { timeFilter<NALU>(window,
	                                                                              nowSinceEpoch) },
																				  [](NALU nalu) { return nalu; });
    
	return ((double)ba::count(accDropped) + ba::count(accAdded)) / bc::duration_cast<bc::seconds>(window).count();
}

double Stats::processedNALUsPS(boost::chrono::microseconds window,
    boost::chrono::microseconds nowSinceEpoch)
{
    boost::mutex::scoped_lock lock(mutex);
    
	auto accAdded = filter<ba::features<ba::tag::count>, NALU, NALU>(addedNALUs,
	                                                           { timeFilter<NALU>(window,
	                                                                              nowSinceEpoch) },
																				  [](NALU nalu) { return nalu; });
    
    return (double)ba::count(accAdded) / bc::duration_cast<bc::seconds>(window).count();
}

double Stats::receivedNALUsBitRate(boost::chrono::microseconds window,
	boost::chrono::microseconds nowSinceEpoch)
{
	boost::mutex::scoped_lock lock(mutex);

	auto sumDropped = filter<ba::features<ba::tag::sum>, NALU, int>(droppedNALUs,
	{ timeFilter<NALU>(window,
	nowSinceEpoch) },
	[](NALU nalu) { return nalu.size; });

	auto sumAdded = filter<ba::features<ba::tag::sum>, NALU, int>(addedNALUs,
	{ timeFilter<NALU>(window,
	nowSinceEpoch) },
	[](NALU nalu) { return nalu.size; });

	//std::cout << ba::count(accAdded) << std::endl;

	return ((double)ba::sum(sumDropped) + ba::sum(sumAdded)) * 8. / bc::duration_cast<bc::seconds>(window).count();
}

double Stats::processedNALUsBitRate(boost::chrono::microseconds window,
	boost::chrono::microseconds nowSinceEpoch)
{
	boost::mutex::scoped_lock lock(mutex);

	auto accSum = filter<ba::features<ba::tag::sum>, NALU, int>(addedNALUs,
	                                                           { timeFilter<NALU>(window,
	                                                                              nowSinceEpoch) },
																				  [](NALU nalu) { return nalu.size; });
	std::cout << ba::sum(accSum) << std::endl;
	return (double)ba::sum(accSum) * 8. / bc::duration_cast<bc::seconds>(window).count();
}

// ###### UTILITY ######

std::string Stats::summary(bc::microseconds window)
{
    bc::microseconds nowSinceEpoch = bc::duration_cast<bc::microseconds>(bc::system_clock::now().time_since_epoch());
    double receivedNALUsPSVal = receivedNALUsPS(window, nowSinceEpoch);
    double processedNALUsPSVal = processedNALUsPS(window, nowSinceEpoch);
	double receivedNALUsBitRateVal = receivedNALUsBitRate(window, nowSinceEpoch);
	double processedNALUsBitRateVal = processedNALUsBitRate(window, nowSinceEpoch);
    int faceCount = 6;
    std::vector<double> cubemapFacesPSVals(faceCount);
    for (int i = 0; i < faceCount; i++)
    {
        cubemapFacesPSVals[i] = cubemapFaceFramesPS(i, window, nowSinceEpoch);
    }
    double fpsVal = fps(window, nowSinceEpoch);
    
    std::stringstream stream;
    stream << "=================================================" << std::endl;
    stream << "Stats for last " << formatDuration(window) << ": " << std::endl;
	stream << "received NALUs/s: " << receivedNALUsPSVal << "; bit rate: " << to_human_readable_byte_count(receivedNALUsBitRateVal, true, false) << ";" << std::endl;
	stream << "processed NALUs/s: " << processedNALUsPSVal << "; bit rate: " << to_human_readable_byte_count(processedNALUsBitRateVal, true, false) << ";" << std::endl;
    for (int i = 0; i < faceCount; i++)
    {
        stream << "cubemap face " << i << " fps: " << cubemapFacesPSVals[i] << ";" << std::endl;
    }
    stream << "fps: " << fpsVal << std::endl;
    
    std::string result = stream.str();
    return result;
}

void Stats::autoSummaryLoop(boost::chrono::microseconds frequency)
{
    while (true)
    {
        boost::this_thread::sleep(boost::posix_time::microseconds(frequency.count()));
        std::cout << summary(frequency);
    }
}

void Stats::autoSummary(boost::chrono::microseconds frequency)
{
    autoSummaryThread = boost::thread(boost::bind(&Stats::autoSummaryLoop, this, frequency));
}