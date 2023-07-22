CC=gcc
CFLAGS=`pkg-config --cflags aravis-0.8 glib-2.0 gstreamer-1.0 gstreamer-app-1.0`
LIBS=`pkg-config --libs aravis-0.8 glib-2.0 gstreamer-1.0 gstreamer-app-1.0`
SOURCES=src/main.c src/WallClockIntervalometerSource.c
EXECUTABLE=capture

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(SOURCES) $(LIBS)

clean:
	rm -f $(EXECUTABLE)
