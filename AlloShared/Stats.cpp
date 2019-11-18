#include <chrono>
#include <thread>
#include <functional>
#include <sstream>
#include <iostream>
#include <boost/ref.hpp>

#include "format.hpp"
#include "to_human_readable_byte_count.hpp"
#include "Stats.hpp"
#include "StatsUtils.hpp"

Stats::Stats()
    :
    activeStorage(&storage1),
    processingStorage(&storage2)
{
    
}

std::map<std::string, double> Stats::query(std::initializer_list<StatVal>                         statVals,
                                           std::function<void (std::map<std::string, double>&)> postCalculator)
{
	std::list<StatVal> statValList(statVals.begin(), statVals.end());
	return query(statValList,
		         postCalculator);
}

std::map<std::string, double> Stats::query(std::list<StatVal>                         statVals,
                                           std::function<void(std::map<std::string, double>&)> postCalculator)
{
	// Create list of makers so that they are active for the time of stats processing
	std::list<StatVal::FilterAccExtractorMaker> faeMakers;
	for (StatVal statVal : statVals)
	{
		faeMakers.push_back(statVal.filterAccExtractorMaker);
	}

	// Make accumulators etc.
	std::list<StatVal::FilterAccExtractor> faes;
	for (auto faeMaker : faeMakers)
	{
		faes.push_back(faeMaker());
	}

	// Process stats
	for (auto datum : *processingStorage)
	{
		for (auto fae : faes)
		{
			fae.first(datum);
		}
	}

	// Get stat values
	std::map<std::string, double> results;
	auto faesIt = faes.begin();
	auto statValsIt = statVals.begin();
	for (; faesIt != faes.end() && statValsIt != statVals.end(); ++faesIt, ++statValsIt)
	{
		results[statValsIt->name] = faesIt->second();
	}

	// Make post calculations
	postCalculator(results);

	return results;
}

std::string Stats::formatDuration(std::chrono::microseconds duration)
{
    std::stringstream result;
	std::chrono::microseconds remainder(duration);

	std::chrono::hours h = std::chrono::duration_cast<std::chrono::hours>(remainder);

    if (h.count() > 0)
    {
        result << h.count() << "h ";
        remainder -= h;
    }

	std::chrono::minutes m = std::chrono::duration_cast<std::chrono::minutes>(remainder);
    if (m.count() > 0)
    {
        result << m.count() << "m ";
        remainder -= m;
    }

	std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(remainder);
    if (s.count() > 0)
    {
        result << s.count() << "s ";
        remainder -= s;
    }

	std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(remainder);
    if (ms.count() > 0)
    {
        result << ms.count() << "ms ";
        remainder -= ms;
    }

	std::chrono::microseconds us = std::chrono::duration_cast<std::chrono::microseconds>(remainder);
    if (us.count() > 0)
    {
        result << us.count() << "µs ";
        remainder -= us;
    }

    std::string format = result.str();
    return format.substr(0, format.size() - 1);
}

// ###### EVENTS ######

void Stats::store(const boost::any& datum)
{
    std::unique_lock<std::mutex> lock(mutex);
    activeStorage->push_back(TimeValueDatum(datum));
}

// ###### UTILITY ######

std::string Stats::summary(std::chrono::microseconds window,
	                       StatValsMaker               statValsMaker,
	                       PostProcessorMaker          postProcessorMaker,
	                       const std::string&          format)
{
    {
        std::unique_lock<std::mutex> lock(mutex);
        
        // swap storages so that events can still be stored while we calculate the statistics
        activeStorage      = (std::list<TimeValueDatum>*)((uintptr_t)activeStorage ^ (uintptr_t)processingStorage);
        processingStorage  = (std::list<TimeValueDatum>*)((uintptr_t)activeStorage ^ (uintptr_t)processingStorage);
        activeStorage      = (std::list<TimeValueDatum>*)((uintptr_t)activeStorage ^ (uintptr_t)processingStorage);
    }
    
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	
	auto results = query(statValsMaker(window, now),
		                 postProcessorMaker(window, now));
    
    format::Dict dict;
    for (auto result : results)
    {
        dict(result.first, result.second);
    }
    dict("duration", formatDuration(window));

    std::stringstream ss;
    ss << "===============================================================================" << std::endl;
    ss << "Stats for last {duration} (" << processingStorage->size() << " items processed):" << std::endl;
    ss << format;
    
    std::string summary = format::fmt(ss.str()) % dict;

	// Empty active storage to remove old data
	processingStorage->clear();
    
	return summary;
}

void Stats::autoSummaryLoop(std::chrono::microseconds frequency,
							StatValsMaker               statValsMaker,
							PostProcessorMaker          postProcessorMaker,
							const std::string&          format)
{
    auto summaryTime = std::chrono::system_clock::now();

    while (true)
    {
        summaryTime += frequency;
        std::this_thread::sleep_until(summaryTime);

        if (stopAutoSummary_)
        {
            return;
        }
		std::cout << summary(frequency, statValsMaker, postProcessorMaker, format);
    }
}

void Stats::autoSummary(std::chrono::microseconds frequency,
						StatValsMaker               statValsMaker,
						PostProcessorMaker          postProcessorMaker,
						const std::string&          format)
{
    stopAutoSummary_ = false;
    autoSummaryThread = std::thread(std::bind(&Stats::autoSummaryLoop, this, frequency, statValsMaker, postProcessorMaker, format));
}

void Stats::stopAutoSummary()
{
    stopAutoSummary_ = true;
}
