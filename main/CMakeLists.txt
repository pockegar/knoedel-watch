idf_component_register(SRCS "main.cpp" "card_ui.cpp" "character.cpp" "knoedel.cpp" "knoedel_bubbles.cpp" "katze.cpp"
                            "deck.cpp" "content.cpp" "progress.cpp" "motion.cpp" "audio_player.cpp" "third_party/minimp3_impl.c"
                       INCLUDE_DIRS ".")

# Third-party decoder: built as-is, without this project's warning settings.
set_source_files_properties("third_party/minimp3_impl.c" PROPERTIES COMPILE_OPTIONS "-w")

if(CONFIG_KNOEDEL_BOARD_WAVESHARE_1_75C)
    # Waveshare ESP32-S3-Touch-AMOLED-1.75C port: this directory's M5Unified.h
    # shadows the real one on the include path, so the application sources
    # compile unchanged against this board's hardware. The real m5unified
    # component stays in the build but nothing links against it.
    target_sources(${COMPONENT_LIB} PRIVATE "board/waveshare_1_75c/board.cpp"
        "board/waveshare_1_75c/display.cpp" "board/waveshare_1_75c/speaker.cpp")
    # The vendored Button/Touch state machines are #included from board.cpp.
    target_include_directories(${COMPONENT_LIB} BEFORE PUBLIC "board/waveshare_1_75c")
endif()
