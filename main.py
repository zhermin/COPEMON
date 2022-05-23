import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
import matplotlib.animation as animation
import matplotlib.ticker as mticker
from matplotlib import pyplot as plt
from matplotlib import style

import tkinter as tk
from tkinter import ttk

import time
import serial
import threading
import os
import sys
from twilio.rest import Client

account_sid = os.environ.get('TWILIO_ACC_SID')
auth_token = os.environ.get('TWILIO_AUTH_TOKEN')
phone_no = os.environ.get('TWILIO_PHONE')
sms_content = 'Patient Tonald Dump (SXXXXX88K) has been transferred to the ICU - KKK Hospital, Ward 69. You have been alerted because you were the designated point of contact. Please contact +6577777777 for more information on your possible courses of actions.'

client = Client(account_sid, auth_token)

def poll_data():
    f = open("copemon_rawdata.txt", "a")
    f.close()

    while True:
        try:
            port = input("Input Port name >> ")
            ser = serial.Serial(port, 115200, timeout=0)
            print(f"\nSuccessfully connected to {port.upper()}! Listening to port..\n")
            break
        except Exception as e:
            print(f"\nUnable to connect to {port.upper()}, please check the port again..\n")
            continue

    start_saving = 0
    sms_counter = 0
    send_sms = False
    icu_mode = False

    i = 0
    while True:
        try:
            data = ser.read(9999).decode('utf-8')

            if sms_counter >= 30: # allow sms to be sent again after delay if reenter ICU mode
                send_sms = False
                sms_counter = 0

            if len(data) > 0:
                print(data)

                data_split = data.split()

                if not send_sms and "Intensive" in data_split:
                    client.messages.create(from_="+12058963018", to=phone_no, body=sms_content)
                    send_sms = True
                    sms_counter = 0

                data_list = [float(val) for val in [''.join([char for char in word if char in '0123456789.']) for word in data_split] if val]

                if len(data_list) != 13:
                    start_saving = 0
                    raise ValueError
                else:
                    start_saving = 1

                data_number = int(data_list[0])
                data_temp = data_list[1]
                data_acc = (data_list[2]**2 + data_list[3]**2 + data_list[4]**2)**0.5
                data_gyro = data_list[6]
                data_mag = (data_list[7]**2 + data_list[8]**2 + data_list[9]**2)**0.5
                data_hum = data_list[11]
                data_baro = data_list[12]

                # print(f'{start_saving},{data_number},{data_temp},{data_acc:.2f},{data_gyro},{data_mag:.2f},{data_hum},{data_baro}\n')

                if start_saving:
                    with open("copemon_rawdata.txt", "a") as f:
                        f.write(f'{data_number},{data_temp},{data_acc:.2f},{data_gyro},{data_mag:.2f},{data_hum},{data_baro}\n')

            time.sleep(1)
            sms_counter += 1

        except Exception as e:
            print(e)

def main():

    LARGE_FONT= ("Verdana", 12)
    style.use("seaborn")

    f = plt.figure()
    ax = f.add_subplot(111)

    global sensor_name, sensor_title, graph_type, update_counter, pause
    sensor_name = "temp"
    sensor_title = "Temperature Reading (°C)"
    graph_type = "line"
    update_counter = 9000
    pause = False

    def change_sensor(to_sensorname, to_sensortitle):
        global sensor_name, sensor_title, update_counter
        sensor_name = to_sensorname
        sensor_title = to_sensortitle
        update_counter = 9000

    def change_graph(to_graphtype):
        global graph_type, update_counter
        graph_type = to_graphtype
        update_counter = 9000

    def startstop():
        global pause
        pause = not pause

    def reset():
        f = open("copemon_rawdata.txt", "w")
        f.close()

    def alert_nok():
        client.messages.create(from_="+12058963018", to=phone_no, body=sms_content)

    def quit_app():
        root.quit()
        root.destroy()
        sys.exit()

    def animate(i):
        global update_counter
        if not pause:
            try:
                dataList = open("copemon_rawdata.txt","r").read().split('\n')
                linesplit = [line.split(',') for line in dataList if line][-20:]
                listvals = [[float(val[i]) for val in linesplit] for i in range(len(linesplit[0]))]

                ax.clear()

                graph_colors = ["darksalmon", "deepskyblue", "seagreen", "plum", "grey", "c"]
                sensor_list = ["temp", "accel", "gyro", "mag", "hum", "baro"]

                if len(linesplit) > 1:
                    for i in range(len(sensor_list)):
                        if sensor_name == sensor_list[i]:
                            if graph_type == "line":
                                ax.plot(listvals[0], listvals[i+1], color=graph_colors[i])
                            else:
                                plt.ylim([(min(listvals[i+1])-0.5*(max(listvals[i+1])-min(listvals[i+1]))), (max(listvals[i+1])+0.5*(max(listvals[i+1])-min(listvals[i+1])))])
                                if graph_type == "bar":
                                    ax.bar(listvals[0], listvals[i+1], color=graph_colors[i])
                                elif graph_type == "area":
                                    plt.fill_between(listvals[0], listvals[i+1], color=graph_colors[i], alpha=0.75)

                title = sensor_title
                ax.set_title(title)
                ax.locator_params(nbins=40, axis='x')
                plt.gca().xaxis.set_major_locator(mticker.MultipleLocator(1))
                ax.locator_params(nbins=10, axis='y')
            except Exception as e:
                print(e)

    class Copemon(tk.Tk):

        def __init__(self, *args, **kwargs):
            
            tk.Tk.__init__(self, *args, **kwargs)
            tk.Tk.wm_title(self, "COPEMON COVID MONITORING")

            container = tk.Frame(self)
            container.pack(side="top", fill="both", expand=True)
            container.grid_rowconfigure(0, weight=1)
            container.grid_columnconfigure(0, weight=1)

            menubar = tk.Menu(container)
            filemenu = tk.Menu(menubar, tearoff=0)
            filemenu.add_command(label="Pause/Unpause", command=startstop)
            filemenu.add_separator()
            filemenu.add_command(label="Reset", command=reset)
            filemenu.add_command(label="Exit", command=quit_app)
            menubar.add_cascade(label="File", menu=filemenu)

            sensorchoice = tk.Menu(menubar, tearoff=1)
            sensorchoice.add_command(label="Temperature",
                                    command=lambda: change_sensor("temp", "Temperature Reading (°C)"))
            sensorchoice.add_command(label="Accelerometer",
                                    command=lambda: change_sensor("accel", "Accelerometer Reading (g)"))
            sensorchoice.add_command(label="Gyroscope",
                                    command=lambda: change_sensor("gyro", "Gyroscope Reading"))
            sensorchoice.add_command(label="Magnetometer",
                                    command=lambda: change_sensor("mag", "Magnetometer Reading (mG)"))
            sensorchoice.add_command(label="Humidity",
                                    command=lambda: change_sensor("hum", "Humidity Reading (%)"))
            sensorchoice.add_command(label="Barometer",
                                    command=lambda: change_sensor("baro", "Barometer Reading (hPa)"))

            menubar.add_cascade(label="Sensors", menu=sensorchoice)

            graphchoice = tk.Menu(menubar, tearoff=0)
            graphchoice.add_command(label="Line Chart",
                                    command=lambda: change_graph("line"))
            graphchoice.add_command(label="Bar Graph",
                                    command=lambda: change_graph("bar"))
            graphchoice.add_command(label="Area Plot",
                                    command=lambda: change_graph("area"))

            menubar.add_cascade(label="Chart Type", menu=graphchoice)

            tk.Tk.config(self, menu=menubar)

            self.frames = {}
            for F in (HomePage, PatientInfoPage, MonitorPage):
                frame = F(container, self)
                self.frames[F] = frame
                frame.grid(row=0, column=0, sticky="nsew")

            self.show_frame(HomePage)

            tk.Tk.iconbitmap(self, default="copemon_icon.ico")

        def show_frame(self, cont):

            frame = self.frames[cont]
            frame.tkraise()

    class HomePage(tk.Frame):

        def __init__(self, parent, controller):
            tk.Frame.__init__(self, parent)

            label = tk.Label(self, text="COPEMON", font=("Verdana", 40, "bold"))
            label.pack(padx=10, pady=(75,10))

            label2 = tk.Label(self, text="Patient: Tonald Dump\nNRIC: SXXXXX88K", font=("Verdana", 30))
            label2.pack(padx=10, pady=(0,30))

            button = ttk.Button(self, text="Patient Info",
                                command=lambda: controller.show_frame(PatientInfoPage))
            button.pack()

            button3 = ttk.Button(self, text="Monitor",
                                command=lambda: controller.show_frame(MonitorPage))
            button3.pack()

            button3 = ttk.Button(self, text="Reset",
                                command=reset)
            button3.pack(pady=(20,0))

            button4 = ttk.Button(self, text="Exit",
                                command=quit_app)
            button4.pack(padx=10, pady=0)

    class PatientInfoPage(tk.Frame):

        def __init__(self, parent, controller):
            tk.Frame.__init__(self, parent)

            button1 = ttk.Button(self, text="Back",
                                command=lambda: controller.show_frame(HomePage))
            button1.pack(side="top", anchor="nw")

            label = tk.Label(self, text="Patient Information", font=("Verdana", 30, "underline"))
            label.pack(pady=10,padx=10)

            label = tk.Label(self, text="Full Name: Tonald J. Dump", font=("Verdana", 36, "bold"))
            label.pack(padx=10, pady=20)

            label2 = tk.Label(self, text="NRIC: SXXXXX88K\nAge: 69\nGender: Male\nUnderlying Conditions:\n  - Hypertension\n  - Type II Diabetes\nNOK: Boe Jiden\nContact Info: +69 8888 8888", font=("Verdana", 30), justify="left")
            label2.pack(padx=10, pady=10)

            button4 = ttk.Button(self, text="Alert NOK", 
                                command=alert_nok)
            button4.pack(padx=10, pady=(10,0))

    class MonitorPage(tk.Frame):

        def __init__(self, parent, controller):
            tk.Frame.__init__(self, parent)

            button1 = ttk.Button(self, text="Back",
                                command=lambda: controller.show_frame(HomePage))
            button1.pack(side="top", anchor="nw")

            canvas = FigureCanvasTkAgg(f, self)
            canvas.draw()
            canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True)

            toolbar = NavigationToolbar2Tk(canvas, self)
            toolbar.update()
            canvas._tkcanvas.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    root = Copemon()
    root.geometry("800x600")
    root.state("zoomed")
    ani = animation.FuncAnimation(f, animate, interval=1000)
    root.mainloop()

b = threading.Thread(name='background', target=poll_data)
f = threading.Thread(name='foreground', target=main)

b.start()
f.start()