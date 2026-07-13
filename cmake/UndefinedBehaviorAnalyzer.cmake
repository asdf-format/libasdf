option(ENABLE_UBSAN "Enable UndefinedBehaviorAnalyzer" OFF)

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()
