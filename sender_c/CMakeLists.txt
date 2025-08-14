option(USE_EMULATOR "Build sender with emulator serial backend" ON)

set(SENDER_SOURCES
  sender.c
  reader.c
  ring_buffer.c
  tcp.c
  packer.c
  serial.h
)

if (USE_EMULATOR)
  list(APPEND SENDER_SOURCES serial_emul.c)
  add_compile_definitions(SERIAL_BACKEND_EMULATOR=1)
else()
  list(APPEND SENDER_SOURCES serial_win.c)
  add_compile_definitions(SERIAL_BACKEND_WIN=1)
endif()

add_executable(sender ${SENDER_SOURCES})

if (WIN32)
  target_compile_definitions(sender PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
  target_link_libraries(sender PRIVATE ws2_32)
endif()
