if(CONFIG_LAYOUT_SHIFT)
  if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    target_sources_ifdef(CONFIG_LAYOUT_SHIFT app PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/layout_shift/src/layout_shift_map.c
      ${CMAKE_CURRENT_LIST_DIR}/layout_shift/src/behavior_layout_shift_key_press.c
      ${CMAKE_CURRENT_LIST_DIR}/layout_shift/src/behavior_layout_shift_toggle.c
    )
  endif()
endif()
