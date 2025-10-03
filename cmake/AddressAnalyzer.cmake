option(ENABLE_ASAN "Enable AddressAnalyzer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()