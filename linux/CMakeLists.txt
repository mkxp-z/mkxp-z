set(SWARG "")
if (STEAMWORKS)
    set(SWARG ${STEAM_LIBPATH})
endif()

if (APPIMAGE)
    install(FILES
            AppRun
            mkxp-z.png
            mkxp-z.desktop)

    install(SCRIPT "execute_process(COMMAND make_appimg.sh ${CMAKE_PROJECT_NAME} ${APPIMAGEKIT_PATH} ${SWARG})")
else()
    if (CMAKE_SIZEOF_LONG EQUAL 8)
        set(ARCHARG 64)
    else()
        set(ARCHARG 32)
    endif()
    install(SCRIPT "execute_process(COMMAND install.sh ${CMAKE_PROJECT_NAME}.${CMAKE_HOST_SYSTEM_PROCESSOR} ${ARCHARG} ${SWARG})")
endif()
