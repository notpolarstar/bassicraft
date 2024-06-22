##
## EPITECH PROJECT, 2024
## Makefile
## File description:
## vulkan 2 la galere
##

SRC	=	src/VkEngine.cpp	\
		src/VkBootstrap.cpp	\
		src/main.cpp	\
		src/Bassicraft.cpp	\
		src/utils.cpp	\
		src/Chunk.cpp	\
		imgui/imgui.cpp	\
		imgui/imgui_draw.cpp	\
		imgui/imgui_widgets.cpp	\
		imgui/imgui_tables.cpp	\
		imgui/backends/imgui_impl_glfw.cpp	\
		imgui/backends/imgui_impl_vulkan.cpp

OBJ	=	$(SRC:.cpp=.o)

NAME	=	bassicraft

CFLAGS	=	-W -Wall -Wextra -Ofast -std=c++20

CPPFLAGS = 	-I./include -I./imgui -I./imgui/backends

LDFLAGS =	-lglfw -lvulkan -ldl -lpthread -lXxf86vm -lXrandr -lXi

CC	=	g++

all: $(NAME)

$(NAME):	$(OBJ)
		$(CC) -o $(NAME) $(OBJ) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

debug:	CFLAGS += -g3 -fsanitize=address
debug:	$(NAME)

clean:
		rm -f $(OBJ)
		find . -name "*~" -delete
		find . -name "#*#" -delete
		find . -name "*.o" -delete

fclean:		clean
		rm -f $(NAME)

re:		fclean all

fresh:	fclean	$(NAME)

.PHONY: all clean fclean re tests fresh