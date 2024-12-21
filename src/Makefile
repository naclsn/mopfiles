out ?= ../exe/
top:; echo hii

warnings = -Wall -Wextra -Wpedantic -Werror -Wno-unused-function -Wno-unused-variable -Wno-unused-value
CFLAGS += -ggdb -std=c99 $(warnings)

ifeq ($(OS), Windows_NT)
winnt = 1
endif

$(out)makeshift: makeshift.c; $(CC) $< -o $@ $(CFLAGS)
$(out)moins:     moins.c;     $(CC) $< -o $@ $(CFLAGS)
$(out)trem:      trem.c;      $(CC) $< -o $@ $(CFLAGS) -pthread $(if $(winnt),-lopengl32 -lgdi32,-lGL -lX11)
$(out)uh:        uh.c;        $(CC) $< -o $@ $(CFLAGS)

$(out)comfl:     comfl/*;     (cd comfl && ./build) && cp comfl/comfl $@
$(out)daety:     daety/*;     (cd daety && ./build) && cp daety/daety $@
$(out)watsh:     watsh/*;     (cd watsh && ./build) && cp watsh/watsh $@
