install(DIRECTORY 16x16 22x22 24x24 48x48 DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor"
         PATTERN ".*" EXCLUDE
         PATTERN "*~" EXCLUDE)


# Plus data (see PLUS.md for sources and licenses).
install(FILES plus-emoji-shortcodes.txt RENAME chewing-plus-emoji-shortcodes.txt
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
install(FILES plus-english.txt RENAME chewing-plus-english.txt
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
# Compiled phrase index (src/CMakeLists.txt builds it from plus-phrases.txt).
install(FILES "${CHEWING_PLUS_PHRASES_BIN}" RENAME chewing-plus-phrases.bin
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/data")
install(FILES LICENSE.gemoji LICENSE.scowl
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/fcitx5-chewing-plus")
