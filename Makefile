# Имя программы
NAME := stardict_to_dsl

# Добавлен флаг -fopenmp для поддержки многопоточности компилятором
CPPFLAGS := -std=c++11 -O3 -fopenmp -ffunction-sections -fdata-sections

# Добавлен флаг -fopenmp (или -lgomp) для линковщика
LDFLAGS_LINUX := -pthread -fopenmp -std=c++11 -O3 -s -Wl,--gc-sections -flto -static -static-libgcc -static-libstdc++
LDFLAGS_WIN   := -fopenmp -std=c++11 -O3 -s -Wl,--gc-sections -flto -static -static-libgcc -static-libstdc++ -lws2_32

# Исходники и заголовки
HEADERS := $(wildcard *.h)
SRC     := $(wildcard *.cpp)

# Списки объектных файлов в подпапках сборки
OBJ_LINUX   := $(patsubst %.cpp, build_linux/%.o, $(notdir $(SRC)))
OBJ_WINDOWS := $(patsubst %.cpp, build_windows/%.o, $(notdir $(SRC)))

.PHONY: all linux windows clean

all: linux

# --- СБОРКА ПОД LINUX (СТАТИЧЕСКАЯ) ---
linux: build_linux $(NAME)

$(NAME): $(OBJ_LINUX)
	g++ $(OBJ_LINUX) -o $@ $(LDFLAGS_LINUX)

build_linux/%.o: %.cpp $(HEADERS)
	g++ $(CPPFLAGS) -c $< -o $@

build_linux:
	mkdir -p build_linux


# --- КРОСС-КОМПИЛЯЦИЯ ПОД WINDOWS ---
windows: build_windows $(NAME).exe

$(NAME).exe: $(OBJ_WINDOWS)
	x86_64-w64-mingw32-g++ $(OBJ_WINDOWS) -o $@ $(LDFLAGS_WIN)

build_windows/%.o: %.cpp $(HEADERS)
	x86_64-w64-mingw32-g++ $(CPPFLAGS) -c $< -o $@

build_windows:
	mkdir -p build_windows


# --- ОЧИСТКА ---
clean:
	rm -rf build_linux build_windows $(NAME) $(NAME).exe
