#
FILE(GLOB_RECURSE SOURCES src/indication/*.cpp)
#
# Exposes components to both source and header files.
set(INDICATION_REQUIRES
    nvs
    driver
    main
   )
#
# Anything that must be exposed to the sources files, but may remain hidden from the header files.
set(INDICATION_PRIV_REQUIRES
   )

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "include"
                       REQUIRES ${INDICATION_REQUIRES}
                       PRIV_REQUIRES ${INDICATION_PRIV_REQUIRES}
                      )
