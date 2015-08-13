include_directories(${CMAKE_BINARY_DIR} ../include)

#-----------------------------------------------------------------------------
# simplegrab example

add_executable(simplegrab simplegrab.c)
target_link_libraries(simplegrab vidcap)


#-----------------------------------------------------------------------------
# vidcapTester example

find_package(Qt5)
if (Qt5_FOUND)
	add_subdirectory(vidcapTester)
else()
	message("QT5 not found, not building the vidcapTester example application")
endif()
