cmake_minimum_required(VERSION 3.21)
project(OSGViewer)

find_package(Qt6 CONFIG REQUIRED)
find_package(Qt6Widgets CONFIG REQUIRED)
find_package(Qt6OpenGL CONFIG REQUIRED)
find_package(Qt6OpenGLWidgets CONFIG REQUIRED)
set(LIBS_QT Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)

find_package(OpenSceneGraph 3.6.0 REQUIRED osgDB osgGA osgUtil osgText osgViewer osgWidget)

add_subdirectory(osgQt)
get_target_property(osgQt_INCLUDES osgQOpenGL INCLUDE_DIRECTORIES)

add_executable(OSGViewer main.cpp)
target_include_directories(OSGViewer PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS})
target_include_directories(OSGViewer PRIVATE ${osgQt_INCLUDES})
target_link_libraries(OSGViewer PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
target_link_libraries(OSGViewer PRIVATE ${OPENGL_LIBRARIES} )
target_link_libraries(OSGViewer PRIVATE ${LIBS_QT})
target_link_libraries(OSGViewer PRIVATE osgQOpenGL)
