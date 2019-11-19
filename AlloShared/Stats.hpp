#pragma once

#include <vector>
#include <chrono>
#include <mutex>
#include <functional>
#include <initializer_list>
#include <thread>
#include <map>
#include <list>

#include <boost/accumulators/accumulators.hpp>
#include <boost/any.hpp>

class Stats
{
public:
    class TimeValueDatum
    {
    public:
        TimeValueDatum(const boost::any& value) : time(std::chrono::steady_clock::now()), value(value) {}
		const std::chrono::steady_clock::time_point time;
        const boost::any value;
    };
    
    class StatVal
    {
    public:
        template <typename Feature>
        static StatVal makeStatVal(std::function<bool (const TimeValueDatum&)>   filter,
                                   std::function<double (const TimeValueDatum&)> accessor,
                                   const Feature&                                  accumulator,
                                   const std::string&                              name)
        {
            auto filterAccExtractorMaker = [filter, accessor]()
            {
                auto acc = new boost::accumulators::accumulator_set<double, boost::accumulators::features<Feature> >();
                
                auto filterAcc = [acc, filter, accessor](const Stats::TimeValueDatum& datum)
                {
                    if (filter(datum))
                    {
                        (*acc)(accessor(datum.value));
                    }
                };
                
                auto extractor = [acc]()
                {
                    boost::accumulators::extractor<Feature> ex;
                    return ex(*acc);
                };
                
				return FilterAccExtractor(filterAcc, extractor);
            };
            
            return StatVal(filterAccExtractorMaker, name);
        }
        
    private:
        friend class Stats;
        
        typedef std::pair<std::function<void (const Stats::TimeValueDatum&)>, std::function<double ()> > FilterAccExtractor;
        typedef std::function<FilterAccExtractor ()> FilterAccExtractorMaker;
        
        StatVal(FilterAccExtractorMaker filterAccExtractorMaker,
                const std::string& name)
                :
                filterAccExtractorMaker(filterAccExtractorMaker),
                name(name) {}
        
        FilterAccExtractorMaker filterAccExtractorMaker;
        std::string name;
    };
    
    Stats();

    // events
    void store(const boost::any& datum);
    
    // utility

    typedef std::function<void(std::map<std::string, double>&)> PostProcessor;
    typedef std::function<std::list<Stats::StatVal>(std::chrono::microseconds,
		                                              std::chrono::steady_clock::time_point)> StatValsMaker;
    typedef std::function<PostProcessor(std::chrono::microseconds,
		                                  std::chrono::steady_clock::time_point)> PostProcessorMaker;

	std::string summary(std::chrono::microseconds window,
		                StatValsMaker               statValsMaker,
		                PostProcessorMaker          postProcessorMaker,
		                const std::string&          format);
    void autoSummary(std::chrono::microseconds frequency,
					 StatValsMaker               statValsMaker,
					 PostProcessorMaker          postProcessorMaker,
                     const std::string&          format);
	void stopAutoSummary();

private:
    std::list<TimeValueDatum>* activeStorage;
    std::list<TimeValueDatum>* processingStorage;
    
    std::list<TimeValueDatum> storage1;
    std::list<TimeValueDatum> storage2;
    
    std::map<std::string, double> query(std::initializer_list<StatVal>                         statVals,
                                        std::function<void (std::map<std::string, double>&)> postCalculator);
	std::map<std::string, double> query(std::list<StatVal>                                    statVals,
                                        std::function<void(std::map<std::string, double>&)> postCalculator);
  
    
    std::string formatDuration(std::chrono::microseconds duration);
    
    std::mutex mutex;
    std::thread autoSummaryThread;
	bool stopAutoSummary_;
    void autoSummaryLoop(std::chrono::microseconds frequency,
						 StatValsMaker               statValsMaker,
						 PostProcessorMaker          postProcessorMaker,
						 const std::string&          format);
};

