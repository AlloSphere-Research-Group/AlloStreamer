# Locate ffmpeg
# This module defines
# FFMPEG_LIBRARIES
# FFMPEG_FOUND, if false, do not try to link to ffmpeg
# FFMPEG_INCLUDE_DIR, where to find the headers
#
# $FFMPEG_DIR is an environment variable that would
# correspond to the ./configure --prefix=$FFMPEG_DIR
#
# Created by Robert Osfield.


#In ffmpeg code, old version use "#include <header.h>" and newer use "#include <libname/header.h>"
#In OSG ffmpeg plugin, we use "#include <header.h>" for compatibility with old version of ffmpeg

#We have to search the path which contain the header.h (usefull for old version)
#and search the path which contain the libname/header.h (usefull for new version)

#Then we need to include ${FFMPEG_libname_INCLUDE_DIRS} (in old version case, use by ffmpeg header and osg plugin code)
#                                                       (in new version case, use by ffmpeg header) 
#and ${FFMPEG_libname_INCLUDE_DIRS/libname}             (in new version case, use by osg plugin code)


# Macro to find header and lib directories
# example: FFMPEG_FIND(AVFORMAT avformat avformat.h)
MACRO(FFMPEG_FIND varname shortname headername)
    # old version of ffmpeg put header in $prefix/include/[ffmpeg]
    # so try to find header in include directory
    FIND_PATH(FFMPEG_${varname}_INCLUDE_DIRS ${headername}
        PATHS
        ${FFMPEG_ROOT}/include/lib${shortname}
        $ENV{FFMPEG_DIR}/include/lib${shortname}
        ~/Library/Frameworks/lib${shortname}
        /Library/Frameworks/lib${shortname}
        /usr/local/include/lib${shortname}
        /usr/include/lib${shortname}
        /sw/include/lib${shortname} # Fink
        /opt/local/include/lib${shortname} # DarwinPorts
        /opt/csw/include/lib${shortname} # Blastwave
        /opt/include/lib${shortname}
        /usr/freeware/include/lib${shortname}
        /usr/include/x86_64-linux-gnu/lib${shortname}
        PATH_SUFFIXES ffmpeg
        DOC "Location of FFMPEG Headers"
    )

#    FIND_PATH(FFMPEG_${varname}_INCLUDE_DIRS ${headername}
#        PATHS
#        ${FFMPEG_ROOT}/include
#        $ENV{FFMPEG_DIR}/include
#        ~/Library/Frameworks
#        /Library/Frameworks
#        /usr/local/include
#        /usr/include
#        /sw/include # Fink
#        /opt/local/include # DarwinPorts
#        /opt/csw/include # Blastwave
#        /opt/include
#        /usr/freeware/include
#        /usr/include/x86_64-linux-gnu
#        PATH_SUFFIXES ffmpeg
#        DOC "Location of FFMPEG Headers"
#    )
#  message ("Found FFMPEG header at ${FFMPEG_${varname}_INCLUDE_DIRS}")

    FIND_LIBRARY(FFMPEG_${varname}_LIBRARIES
        NAMES ${shortname}
        PATHS
        ${FFMPEG_ROOT}/lib
        $ENV{FFMPEG_DIR}/lib
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        /usr/freeware/lib64
        /usr/lib/x86_64-linux-gnu
        DOC "Location of FFMPEG Libraries"
    )

    message ("Found FFMPEG library at ${FFMPEG_${varname}_LIBRARIES}")
    IF (FFMPEG_${varname}_LIBRARIES AND FFMPEG_${varname}_INCLUDE_DIRS)
        SET(FFMPEG_${varname}_FOUND 1)
    ENDIF(FFMPEG_${varname}_LIBRARIES AND FFMPEG_${varname}_INCLUDE_DIRS)

ENDMACRO(FFMPEG_FIND)

SET(FFMPEG_ROOT "$ENV{FFMPEG_DIR}" CACHE PATH "Location of FFMPEG")

FIND_PATH(FFMPEG_INCLUDE_DIRS
	libavcodec libavdevice libavfilter libavformat libavutil libpostproc libswresample libswscale
	PATHS ${FFMPEG_ROOT}/include
)

FIND_PATH(FFMPEG_LIBRARY_DIRS
	avcodec.lib avdevice.lib avfilter.lib  avformat.lib avutil.lib postproc.lib swresample.lib swscale.lib
	PATHS ${FFMPEG_ROOT}/lib
)

FFMPEG_FIND(LIBAVFORMAT avformat avformat.h)
FFMPEG_FIND(LIBAVDEVICE avdevice avdevice.h)
FFMPEG_FIND(LIBAVCODEC  avcodec  avcodec.h)
FFMPEG_FIND(LIBAVUTIL   avutil   avutil.h)
FFMPEG_FIND(LIBSWSCALE  swscale  swscale.h)  # not sure about the header to look for here.

SET(FFMPEG_FOUND "NO")
# Note we don't check FFMPEG_LIBSWSCALE_FOUND here, it's optional.
IF (FFMPEG_LIBAVFORMAT_FOUND AND FFMPEG_LIBAVDEVICE_FOUND AND FFMPEG_LIBAVCODEC_FOUND AND FFMPEG_LIBAVUTIL_FOUND AND
	FFMPEG_INCLUDE_DIRS)

    SET(FFMPEG_FOUND "YES")

    SET(FFMPEG_LIBRARIES
        ${FFMPEG_LIBAVFORMAT_LIBRARIES}
        ${FFMPEG_LIBAVDEVICE_LIBRARIES}
        ${FFMPEG_LIBAVCODEC_LIBRARIES}
        ${FFMPEG_LIBAVUTIL_LIBRARIES}
	${FFMPEG_LIBSWSCALE_LIBRARIES}
	)

IF (UNIX)
	LIST(APPEND FFMPEG_LIBRARIES pthread)
ENDIF ()

ELSE ()

#    MESSAGE(STATUS "Could not find FFMPEG")

ENDIF()

if (FFMPEG_FOUND)
	message(STATUS "Found FFmpeg")
endif()
