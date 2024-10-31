# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/gflags-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${gflags_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${gflags_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET gflags::gflags)
    add_library(gflags::gflags INTERFACE IMPORTED)
    message(${gflags_MESSAGE_MODE} "Conan: Target declared 'gflags::gflags'")
endif()
if(NOT TARGET gflags)
    add_library(gflags INTERFACE IMPORTED)
    set_property(TARGET gflags PROPERTY INTERFACE_LINK_LIBRARIES gflags::gflags)
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/gflags-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()