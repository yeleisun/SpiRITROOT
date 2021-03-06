# Create a library called "libSTDigi" which includes the source files given in the array .

set(INCLUDE_DIRECTORIES
${ROOT_INCLUDE_DIR} 
${GENFIT2_INCLUDE_DIR}
${BASE_INCLUDE_DIRECTORIES}
# Add all the folders having header files you used in the code.
${CMAKE_SOURCE_DIR}/mc
${CMAKE_SOURCE_DIR}/par
${CMAKE_SOURCE_DIR}/format
${CMAKE_SOURCE_DIR}/tool
${CMAKE_SOURCE_DIR}/digi
${CMAKE_SOURCE_DIR}/util
${CMAKE_SOURCE_DIR}/reco
${CMAKE_SOURCE_DIR}/reco/PulseShapeAnalyzer
)

include_directories(${INCLUDE_DIRECTORIES})

include_directories(SYSTEM ${SYSTEM_INCLUDE_DIRECTORIES})

set(LINK_DIRECTORIES
${ROOT_LIBRARY_DIR}
${GENFIT2_LIBRARY_DIR}
${FAIRROOT_LIBRARY_DIR}
) 

link_directories(${LINK_DIRECTORIES})

set(SRCS
# Add all the source files below this line. Those must have cc for their extension.
STDriftTask.cc
STPadResponseTask.cc
STElectronicsTask.cc
)

CHANGE_FILE_EXTENSION(*.cc *.hh HEADERS "${SRCS}")

Set(LINKDEF STDigiLinkDef.h)
Set(LIBRARY_NAME STDigi)
Set(DEPENDENCIES
  Base
  Stack
# Add all library names your code depending on.
# Basically those are the libraries in the header files' directory.
  STMC
  STPar
  STFormat
  STUtil
  STReco
  STTool
)

GENERATE_LIBRARY()
