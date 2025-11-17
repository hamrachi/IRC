NAME = ircserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

# Files
SRCS = main.cpp Server.cpp Client.cpp Channel.cpp cmd_join.cpp cmd_privmsg.cpp cmd_mode.cpp cmd_topic.cpp cmd_invite.cpp cmd_kick.cpp
OBJS = $(SRCS:.cpp=.o)

# Headers
HEADERS = Server.hpp Client.hpp Channel.hpp Commands.hpp

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

# Rule for .o files (depends on headers)
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re