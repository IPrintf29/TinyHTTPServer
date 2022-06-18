DIR_BIN = .
DIR_OBJ = ./obj
DIR_SRC = ./src
DIR_BEN = ./webbench_code/WebBench

#wildcard 擴展通配符
#patsubst 字符串替換
#notdir 去除路徑
SRC = $(wildcard $(DIR_SRC)/*.cpp)
OBJ = $(patsubst %.cpp,$(DIR_OBJ)/%.o,$(notdir $(SRC)))


CXX_FLAG = -D _CPLUSPLUS11 -Wall -Iinclude -std=c++11 -pthread -O0 -L/usr/lib64/mysql -lmysqlclient -lrt
CC = g++

TARGET1 = httpserver
TARGET2 = webbench

.PHONY : all
all: $(TARGET1) $(TARGET2) 

$(DIR_BIN)/$(TARGET1) : $(OBJ)
	$(CC) $(CXX_FLAG) -o $@ $^  

$(DIR_BIN)/$(TARGET2) : $(DIR_OBJ)/$(TARGET2).o
	$(CC) $(CXX_FLAG) -o $@ $^

$(DIR_OBJ)/%.o : $(DIR_SRC)/%.cpp
	if [ ! -d $(DIR_OBJ) ];	then mkdir -p $(DIR_OBJ); fi;
	$(CC) $(CXX_FLAG) -c $< -o $@

$(DIR_OBJ)/$(TARGET2).o : $(DIR_BEN)/$(TARGET2).c
	$(CC) $(CXX_FLAG) -c $^ -o $@

.PHONY : clean
clean : 
	-rm -rf $(DIR_OBJ)
	-rm -f $(TARGET1)
	-rm -f $(TARGET2)
