CC = gcc
CFLAGS = -g
LIBS = -lncurses -lpanel 

TARGET = balls-on-fire

all: $(TARGET)

$(TARGET): main.c cJSON.c
	$(CC) $(CFLAGS) main.c cJSON.c -o $(TARGET) $(LIBS)

install: 
	sudo install -m 755 $(TARGET) /usr/local/bin/

clean:
	rm -f $(TARGET)

