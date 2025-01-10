
target_sources(${PROJECT_NAME} PRIVATE
    # {{BEGIN_TARGET_SOURCES}}
	${CMAKE_CURRENT_LIST_DIR}/src/tx_initialize_low_level.S
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_context_restore.S
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_context_save.S
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_fp_disable.c
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_fp_enable.c
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_interrupt_control.S
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_interrupt_disable.S
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_interrupt_restore.S
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_schedule.S
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_stack_build.S
	${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_system_return.S
	${CMAKE_CURRENT_LIST_DIR}/src/tx_timer_interrupt.S
    # {{END_TARGET_SOURCES}}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/inc
    ${CMAKE_SOURCE_DIR}/../dtc/libfdt
)

target_compile_options(${PROJECT_NAME} PRIVATE -DEL1)

set(APP_NAME threadxen)
set(EXE_NAME ${APP_NAME}.elf)
set(LDS ${CMAKE_CURRENT_LIST_DIR}/xen_build/threadx.ld)
add_executable(${EXE_NAME})
message(STATUS "find libc from ${CMAKE_SOURCE_DIR}")
target_link_libraries(${EXE_NAME} ${CMAKE_SOURCE_DIR}/../dtc/libfdt/libfdt.a ${CMAKE_SOURCE_DIR}/../libc/build/libc.a ${CMAKE_SOURCE_DIR}/../app/rust/target/aarch64-unknown-none/release/librust.a)
target_link_libraries(${EXE_NAME} ${PROJECT_NAME})
target_link_options(${EXE_NAME} PRIVATE -T ${LDS} -Wl,--no-warn-rwx-segments -Wl,--build-id=none)
target_sources(${EXE_NAME} PRIVATE
    # {{BEGIN_TARGET_SOURCES}}
	${CMAKE_CURRENT_LIST_DIR}/xen_build/main.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/gicv3_gicd.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/gicv3_gicr.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/gicv3.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/timer.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/putc.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/mp_mutexes.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/startup.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/v8_aarch64.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/v8_utils.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/vectors.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/pecoff.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/hypercall.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/mmap.c
    # {{END_TARGET_SOURCES}}
)
add_custom_command(
    TARGET ${EXE_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${EXE_NAME} -O binary ${APP_NAME}
)

