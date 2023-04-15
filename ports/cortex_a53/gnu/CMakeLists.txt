
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
)

set(EXE_NAME threadxen)
set(APP_NAME ${EXE_NAME}.elf)
set(LDS ${CMAKE_CURRENT_LIST_DIR}/xen_build/threadx.ld)
add_executable(${APP_NAME})
message(STATUS "find libc from ${CMAKE_SOURCE_DIR}")
target_link_libraries(${APP_NAME} ${CMAKE_SOURCE_DIR}/../libc/build/libc.a)
target_link_libraries(${APP_NAME} ${PROJECT_NAME})
target_link_options(${APP_NAME} PRIVATE -T ${LDS})
target_sources(${APP_NAME} PRIVATE
    # {{BEGIN_TARGET_SOURCES}}
	${CMAKE_CURRENT_LIST_DIR}/xen_build/main.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/gicv3_gicd.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/gicv3_gicr.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/sp804_timer.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/timer_interrupts.c
	${CMAKE_CURRENT_LIST_DIR}/xen_build/mp_mutexes.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/startup.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/v8_aarch64.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/v8_utils.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/vectors.s
	${CMAKE_CURRENT_LIST_DIR}/xen_build/pecoff.s
    # {{END_TARGET_SOURCES}}
)
add_custom_command(
    TARGET ${APP_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${APP_NAME} -O binary ${EXE_NAME}
)

