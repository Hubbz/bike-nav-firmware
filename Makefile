IDF_PY ?= idf.py
TARGET ?= esp32s3
PORT ?= /dev/ttyACM0

.PHONY: build clean target flash monitor all

build:
	$(IDF_PY) build

clean:
	$(IDF_PY) fullclean

target:
	$(IDF_PY) set-target $(TARGET)

flash:
	$(IDF_PY) -p $(PORT) flash

monitor:
	$(IDF_PY) -p $(PORT) monitor

all: clean target build flash

