top:; echo hii

warnings = -Wall -Wextra -Wpedantic -Werror -Wno-unused-function -Wno-unused-variable -Wno-unused-value
CFLAGS += -ggdb -std=c99 $(warnings)

ifeq ($(OS), Windows_NT)
frame-LDFLAGS = -pthread -lopengl32 -lgdi32
else
frame-LDFLAGS = -pthread -lGL -lX11
endif

trem: trem.c; $(CC) $< -o $@ $(CFLAGS) $(frame-LDFLAGS)
moins: moins.c; $(CC) $< -o $@ $(CFLAGS)
makeshift: makeshift.c; $(CC) $< -o $@ $(CFLAGS)
uh: uh.c; $(CC) $< -o $@ $(CFLAGS)
