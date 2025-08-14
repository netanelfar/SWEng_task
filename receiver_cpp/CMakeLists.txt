
add_executable(receiver
  receiver.cpp
  DoubleListPool.hpp
  ListenerThread.hpp
  ListenerThread.cpp
  WriterThread.hpp
  WriterThread.cpp
)

if (WIN32)
  target_compile_definitions(receiver PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
  target_link_libraries(receiver PRIVATE ws2_32)
endif()

set_target_properties(receiver PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
