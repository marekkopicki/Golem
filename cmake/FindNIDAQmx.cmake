# - Find NIDAQMX
# Find the native NIDAQMX headers and libraries.
#
#  NIDAQMX_INCLUDE_DIR -  where to find the include files of NIDAQMX
#  NIDAQMX_LIBRARIES    - List of libraries when using NIDAQMX.
#  NIDAQMX_FOUND        - True if NIDAQMX found.

GET_FILENAME_COMPONENT(module_file_path ${CMAKE_CURRENT_LIST_FILE} PATH)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	SET(LIB "lib64")
else(CMAKE_SIZEOF_VOID_P EQUAL 8)
	SET(LIB "lib32")
endif(CMAKE_SIZEOF_VOID_P EQUAL 8)

SET(program_files_path "$ENV{PROGRAMFILES}")

IF(program_files_path)
	FILE(TO_CMAKE_PATH $ENV{PROGRAMFILES} program_files_path)
ENDIF(program_files_path)

SET(NIDAQMX_PATHS
	/usr/
	/usr/local/
	"${program_files_path}/National Instruments/Shared/ExternalCompilerSupport/C/"
	"C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C/"
	"C:/Program Files/National Instruments/Shared/ExternalCompilerSupport/C/"
)

# Look for the header file.
FIND_PATH(NIDAQMX_INCLUDE NAMES NIDAQmx.h PATHS ${NIDAQMX_PATHS} PATH_SUFFIXES "include")
MARK_AS_ADVANCED(NIDAQMX_INCLUDE)

# Look for the library file.
if (WIN32)
	FIND_LIBRARY(NIDAQMX_LIBRARY NAMES NIDAQmx PATHS ${NIDAQMX_PATHS} PATH_SUFFIXES "${LIB}/msvc")
elseif (UNIX)
	FIND_LIBRARY(NIDAQMX_LIBRARY NAMES nidaqmxbase PATHS ${NIDAQMX_PATHS} PATH_SUFFIXES "lib")
endif ()
MARK_AS_ADVANCED(NIDAQMX_LIBRARY)

# Copy the results to the output variables.
IF(NIDAQMX_INCLUDE AND NIDAQMX_LIBRARY)
	SET(NIDAQMX_FOUND 1)
	SET(NIDAQMX_LIBRARIES ${NIDAQMX_LIBRARY})
	SET(NIDAQMX_INCLUDE_DIR ${NIDAQMX_INCLUDE})
ELSE(NIDAQMX_INCLUDE AND NIDAQMX_LIBRARY)
	SET(NIDAQMX_FOUND 0)
	SET(NIDAQMX_LIBRARIES)
	SET(NIDAQMX_INCLUDE_DIR)
ENDIF(NIDAQMX_INCLUDE AND NIDAQMX_LIBRARY)

# Report the results.
IF(NOT NIDAQMX_FOUND)
	SET(NIDAQMX_DIR_MESSAGE "NIDAQMX was not found. Make sure NIDAQMX_LIBRARIES and NIDAQMX_INCLUDE_DIR are set.")
	IF(NIDAQMX_FIND_REQUIRED)
		MESSAGE(FATAL_ERROR "${NIDAQMX_DIR_MESSAGE}")
	ELSEIF(NOT NIDAQMX_FIND_QUIETLY)
		MESSAGE(STATUS "${NIDAQMX_DIR_MESSAGE}")
	ENDIF(NIDAQMX_FIND_REQUIRED)
ENDIF(NOT NIDAQMX_FOUND)