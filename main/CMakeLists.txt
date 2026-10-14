idf_component_register(SRCS "main.cpp" "card_ui.cpp" "character.cpp" "knoedel.cpp" "knoedel_bubbles.cpp" "katze.cpp"
                            "deck.cpp" "content.cpp" "progress.cpp" "motion.cpp" "audio_player.cpp" "third_party/minimp3_impl.c"
                       INCLUDE_DIRS ".")

# Third-party decoder: built as-is, without this project's warning settings.
set_source_files_properties("third_party/minimp3_impl.c" PROPERTIES COMPILE_OPTIONS "-w")
