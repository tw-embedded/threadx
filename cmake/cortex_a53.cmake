# Name of the target
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-a53)

set(THREADX_ARCH "cortex_a53")
set(THREADX_TOOLCHAIN "gnu")

set(MCPU_FLAGS "-mcpu=cortex-a53")
set(VFP_FLAGS "")
#set(SPEC_FLAGS "--specs=nosys.specs")
set(LD_FLAGS "-nostartfiles")

include(${CMAKE_CURRENT_LIST_DIR}/aarch64-linux-gnu.cmake)
