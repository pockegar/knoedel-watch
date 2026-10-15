set(srcs "main.cpp" "card_ui.cpp" "character.cpp" "knoedel.cpp" "knoedel_bubbles.cpp" "katze.cpp"
         "deck.cpp" "content.cpp" "progress.cpp" "motion.cpp" "audio_player.cpp" "third_party/minimp3_impl.c")

# The board's side of main/board/board.h, chosen in Kconfig ("Target board").
if(CONFIG_KNOEDEL_BOARD_M5_STOPWATCH)
    list(APPEND srcs "board/m5_stopwatch.cpp")
endif()

idf_component_register(SRCS ${srcs} INCLUDE_DIRS "." "board")

# Third-party decoder: built as-is, without this project's warning settings.
set_source_files_properties("third_party/minimp3_impl.c" PROPERTIES COMPILE_OPTIONS "-w")
