#pragma once

#include "Stats.hpp"

class StatsUtils
{
public:
	// PARAMETERS
    class NALU
    {
    public:
        enum Status {RECEIVED, DROPPED, ADDED, PROCESSED, SENT};
        
        NALU(int type, size_t size, int face, Status status) : type(type), size(size), face(face), status(status) {}
        int    type;
        size_t size;
        int    face;
        Status status;
    };
    
    class Frame
    {
    public:
        enum Status {RECEIVED, DECODED, COLOR_CONVERTED};
        
        Frame(int type, size_t size, int face, Status status) : type(type), size(size), face(face), status(status) {}
        int    type;
        size_t size;
        int    face;
        Status status;
    };
    
    class CubemapFace
    {
    public:
        enum Status {ADDED, DISPLAYED, SCHEDULED};
        
        CubemapFace(int face, Status status) : face(face), status(status) {}
        int face;
        Status status;
    };

    class Cubemap
    {
    };
    
    // STAT VALS
	static Stats::StatVal nalusBitSum  (const std::string&                      name,
                                        int                                     face,
                                        NALU::Status                            status,
                                        std::chrono::microseconds             window,
                                        std::chrono::steady_clock::time_point now);
	static Stats::StatVal nalusCount   (const std::string&                      name,
                                        int                                     face,
                                        NALU::Status                            status,
                                        std::chrono::microseconds             window,
                                        std::chrono::steady_clock::time_point now);
	static Stats::StatVal cubemapsCount(const std::string&                      name,
                                        std::chrono::microseconds             window,
                                        std::chrono::steady_clock::time_point now);
	static Stats::StatVal facesCount   (const std::string&                      name,
                                        int                                     face,
                                        std::chrono::microseconds             window,
                                        std::chrono::steady_clock::time_point now);

	// FILTERS
    static std::function<bool(Stats::TimeValueDatum)> timeFilter      (std::chrono::microseconds window,
		                                                                 std::chrono::steady_clock::time_point now);
    static std::function<bool(Stats::TimeValueDatum)> typeFilter      (const std::type_info& type);
    static std::function<bool(Stats::TimeValueDatum)> naluFaceFilter  (int face);
    static std::function<bool(Stats::TimeValueDatum)> naluStatusFilter(NALU::Status status);
    static std::function<bool(Stats::TimeValueDatum)> andFilter       (std::initializer_list<std::function<bool(Stats::TimeValueDatum)> > filters);
};

