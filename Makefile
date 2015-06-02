TARGET = megabrowser
CFLAGS = -Wall -Wextra -O0 -Wdeclaration-after-statement -Wredundant-decls -Wmissing-noreturn -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Winline -Wformat-security -Wswitch-default -Winit-self -Wmissing-include-dirs -Wundef -Waggregate-return -Wmissing-format-attribute -Wnested-externs -Wstrict-overflow=5 -Wformat=2 -Wunreachable-code -Wfloat-equal -ffloat-store -g -ggdb3 -Wno-format-nonliteral -DDEBUG=1

INC = `pkg-config --cflags gtk+-2.0 glib-2.0`
LIBS = `pkg-config --libs gtk+-2.0 glib-2.0 libmega`

SOURCES = main.c megaapi_wrapper.cpp
OBJECTS = main.o megaapi_wrapper.o

all: $(TARGET)

# objects
.c.o:
	$(CC) -c $(CFLAGS) $(INC) $< -o $@

.cpp.o:
	$(CXX) -c `pkg-config --cflags libmega` $< -o $@

# apps
$(TARGET): $(OBJECTS)
	$(CXX) -g -Wall -Wextra -O0 $(OBJECTS) $(LIBS) -o $@

clean:
	-rm -f $(OBJECTS)
	-rm -f core
	-rm -f $(TARGET)
	-rm -f *.o
