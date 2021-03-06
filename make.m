HEADERS = $(wildcard *.h) $(wildcard *.H)
SRCS_C = $(wildcard *.c)
SRCS_CPP = $(wildcard *.C)

DBOBJS = $(wildcard /root/src/mysql_common/obj/*.o)

OBJ = obj/
OBJS_C = $(SRCS_C:.c=.o)
OBJS_CPP = $(SRCS_CPP:.C=.o)
OBJS := $(addprefix $(OBJ),$(OBJS_C)) $(addprefix $(OBJ),$(OBJS_CPP))

CMPPARMS = -g -c -w -D_LINUX_ -I/usr/include/mysql -I/root/src/liboliver -I/root/src/mysql_common
LLIBS = -L/root/src/liboliver -L/usr/lib64/mysql

rpt_list_hist1 : $(OBJS)
	g++ $(LLIBS) -o rpt_list_hist1 $(OBJS) $(DBOBJS) -Wl,-Bstatic -l oliver -Wl,-Bdynamic -l mysqlclient -pthread

./obj/%.o: %.c $(HEADERS)
	g++ $(CMPPARMS) $(INCLUDES) -c $< -o $@

./obj/%.o: %.C $(HEADERS)
	g++ $(CMPPARMS) $(INCLUDES) -c $< -o $@

clean :
	rm $(OBJS)

