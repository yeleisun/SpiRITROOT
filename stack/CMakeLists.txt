# Create a library called "libBase" which includes the source files given in
# the array .
# The extension is already found.  Any number of sources could be listed here.

set(INCLUDE_DIRECTORIES
${ROOT_INCLUDE_DIR}
${BASE_INCLUDE_DIRECTORIES}
${CMAKE_SOURCE_DIR}/stack
)

include_directories( ${INCLUDE_DIRECTORIES})

include_directories(SYSTEM ${SYSTEM_INCLUDE_DIRECTORIES})

set(LINK_DIRECTORIES
${ROOT_LIBRARY_DIR}
${FAIRROOT_LIBRARY_DIR}
)

link_directories( ${LINK_DIRECTORIES})

set(SRCS
STStack.cxx 
STMCTrack.cxx
)

Set(HEADERS )
Set(LINKDEF MCStackLinkDef.h)
Set(LIBRARY_NAME Stack)
Set(DEPENDENCIES Base EG Physics Core)

GENERATE_LIBRARY()

