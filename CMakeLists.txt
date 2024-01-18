#
FILE(GLOB_RECURSE SOURCES src/*.cpp)
#
# Included components which are exposed in public header files.
set(INDICATION_REQUIRES
   main
   )
#
# Anything that must be included, but may remain hidden from the public header files.
# Limiting component exposure can reduce Undefined Reference linking problems in larger applications.
set(INDICATION_PRIV_REQUIRES
   nvs_5.2
   driver
   )

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "include"
                       REQUIRES ${INDICATION_REQUIRES}
                       PRIV_REQUIRES ${INDICATION_PRIV_REQUIRES}
                      )
