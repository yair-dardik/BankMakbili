CC = g++
CFLAGS = -std=c++11 -g -Wall -Werror -pedantic-errors -DNDEBUG -pthread -fsanitize=thread
LDFLAGS = -pthread -fsanitize=thread

SRCS = main.cpp ATM.cpp Bank.cpp Account.cpp VIPManager.cpp RWLock.cpp log.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = bank

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)