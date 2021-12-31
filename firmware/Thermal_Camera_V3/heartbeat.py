import numpy as np
import matplotlib.pyplot as plt
import json
import time
import random
# Need socket to create a udp link
import socket
BUFSIZE = 1024

# 图像范围
y_width = 100
Y_MID = 0
x_width = 1000

"""
data_list_len = 25
avg_level = 15
"""

# 滤波等级
data_list_len = 15
avg_level = 0

total_avg_level = 60
heart_circle = 6000

# udp设置
# Set server port
ip_port = ('192.168.1.29', 80)
# Set udp
server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# Set bind mod
server.bind(ip_port)


def udp_get_list(list_length, level):

    i = 0
    data_list = []
    while i < list_length:
        data, client_addr = server.recvfrom(BUFSIZE)
        data_str = data.decode('utf-8').encode('utf-8')
        print(data_str)
        server.sendto(data.upper(),client_addr)

        try:
            max30100_data = json.loads(data_str)
            IR_list = max30100_data["IR"]
            print(IR_list)
            
            for IR in IR_list :
                data_list.append(IR)
                i = i + 1

        except:
            print(data_str)
    if level > 0:
        avg_list = avg_filter(data_list, level)
        return avg_list
    else:
        return data_list



# 均值滤波
def avg_filter(src_list, level):
    avg_list = []
    for i in range(len(src_list) - level + 1):
        temp = 0
        for j in range(level):
            temp += src_list[i + j]

        avg_list.append(temp / level)

    return avg_list

# 波谷统计
def lowest_point(src_list):
    lowest_list = []
    last_low = 0

    temp_mid = (max(src_list) + min(src_list)) // 2

    for i in range(len(src_list) - 2):
        temp = src_list[i+1]
        if temp < 3000:
            break
        temp_previous = src_list[i]
        temp_next = src_list[i + 2]

        if temp < temp_previous and temp <= temp_next and temp < temp_mid:
            if i - last_low > 30:
                lowest_list.append(i)
                last_low = i
    return lowest_list


# 主函数
def main():

    list_y = []
    for _ in range(x_width):
        # list_y.append(random.random())
        list_y.append(0)
    # print(list_y)

    temp_y = np.array(list_y)
    heart_rate = 0

    while True:
        temp_list = udp_get_list(data_list_len, avg_level)

        for temp in temp_list:
            list_y.pop(0)
            list_y.append(temp)

        avg_list_y = avg_filter(list_y, total_avg_level)

        plt.clf()
        plt.title("Heart Rate:" + str(heart_rate))
        plt.ylabel("IR data")

        temp_x = np.arange(len(avg_list_y))
        plt.plot(temp_x, temp_y)

        plt.draw()
        plt.pause(0.001)


if __name__ == "__main__":
    main()
