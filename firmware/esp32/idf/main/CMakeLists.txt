get_filename_component(FIRMWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(DHT_SENSOR_LIBRARY_DIR "" CACHE PATH
    "Arduino DHT sensor library folder containing DHT.cpp and DHT.h")
set(ADAFRUIT_UNIFIED_SENSOR_LIBRARY_DIR "" CACHE PATH
    "Arduino Adafruit Unified Sensor library folder containing Adafruit_Sensor.cpp")

function(find_arduino_library_sources library_dir source_name header_name out_sources out_include)
  if(EXISTS "${library_dir}/src/${source_name}" AND EXISTS "${library_dir}/src/${header_name}")
    set(library_src_dir "${library_dir}/src")
  elseif(EXISTS "${library_dir}/${source_name}" AND EXISTS "${library_dir}/${header_name}")
    set(library_src_dir "${library_dir}")
  else()
    message(FATAL_ERROR
      "Arduino library not found. Set its *_LIBRARY_DIR to the folder containing ${source_name} and ${header_name}.")
  endif()
  file(GLOB library_sources CONFIGURE_DEPENDS "${library_src_dir}/*.cpp")
  set(${out_sources} "${library_sources}" PARENT_SCOPE)
  set(${out_include} "${library_src_dir}" PARENT_SCOPE)
endfunction()

find_arduino_library_sources(
  "${DHT_SENSOR_LIBRARY_DIR}" "DHT.cpp" "DHT.h"
  DHT_SENSOR_SOURCES DHT_SENSOR_INCLUDE_DIR)
find_arduino_library_sources(
  "${ADAFRUIT_UNIFIED_SENSOR_LIBRARY_DIR}" "Adafruit_Sensor.cpp" "Adafruit_Sensor.h"
  ADAFRUIT_SENSOR_SOURCES ADAFRUIT_SENSOR_INCLUDE_DIR)

file(GLOB FIRMWARE_SOURCES CONFIGURE_DEPENDS "${FIRMWARE_DIR}/*.cpp")

idf_component_register(
  SRCS
    "main.cpp"
    ${FIRMWARE_SOURCES}
    ${DHT_SENSOR_SOURCES}
    ${ADAFRUIT_SENSOR_SOURCES}
  INCLUDE_DIRS
    "${FIRMWARE_DIR}"
    "${DHT_SENSOR_INCLUDE_DIR}"
    "${ADAFRUIT_SENSOR_INCLUDE_DIR}"
  REQUIRES
    arduino-esp32
    app_update
    bt
    driver
    esp_event
    esp_hw_support
    esp_http_client
    esp_netif
    esp-tls
    esp_timer
    esp_wifi
    heap
    json
    mbedtls
    mqtt
    nvs_flash
    spi_flash
)
