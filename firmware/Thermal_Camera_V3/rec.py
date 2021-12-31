import socket
import json
import time
import numpy as np
import matplotlib.pyplot as plt



hostname = socket.gethostname()
UDP_IP = socket.gethostbyname(hostname)
print("***Local ip:" + str(UDP_IP) + "***")
UDP_PORT = 80
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind((UDP_IP, UDP_PORT))
sock.listen(1)  # 接收的连接数
data, addr = sock.accept()



# 主函数
def main():

    x_width = 10
    max_temp = 0

    list_y = []
    for _ in range(x_width):
        list_y.append(0)

    

    while True:
        line = data.recv(1024).decode('UTF-8')


        try:
            mlx_data = json.loads(line)
            print(mlx_data)

            max_temp = mlx_data["MAXTEMP"]

            list_y.pop(0)
            list_y.append(max_temp)

        except:
            print(line)
        print("")

        plt.clf()
        plt.title("Max Temperature:" + str(max_temp) + " C")
        plt.ylabel("Temperature")

        temp_x = np.arange(len(list_y))
        temp_y = np.array(list_y)

        plt.plot(temp_x, temp_y)

        plt.draw()
        plt.pause(0.001)


if __name__ == "__main__":
    main()
