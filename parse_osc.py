import serial
import cv2
import numpy as np
import time
from math import *
ser = serial.Serial('/dev/tty.usbserial-AL012LU6', 115200, timeout=1)

last = time.time()
last_a = 0
last_b = 0
last_c = 0
phase_a = 0
phase_b = 0
phase_c = 0
line = ""
lbuf = ""
i = 0
last_i = 0

ADC_BASE_VOLTAGE = 3.3
OP_A1            = (0.019 / 0.9)
OP_A2            = (0.036 / 1.739)
OP_A3            = (0.036 / 1.739)

rmsA1 = 0
rmsA2 = 0
rmsA3 = 0
rmsAC_A1 = 0
rmsAC_A2 = 0
rmsAC_A3 = 0
peakA1 = 0
peakA2 = 0
peakA3 = 0
lastVolA1 = 0
lastVolA2 = 0
lastVolA3 = 0
totalCnt = 0
countHZ_A1 = 0
countHZ_A2 = 0
countHZ_A3 = 0
pp_A1 = -1
hp_A1 = 0
lp_A1 = 1024
pp_A2 = -1
hp_A2 = 0
lp_A2 = 1024
pp_A3 = -1
hp_A3 = 0
lp_A3 = 1024

buf = np.zeros((720, 1280, 3), np.uint8)
while True:
    try:
        line = lbuf + ser.readline().decode('utf-8')
        if line[-2:] != "\r\n":
            lbuf += line
            continue
        if line[0:9] == "Samples: ":
            if (countHZ_A1 == 0): countHZ_A1 = 1;
            if (countHZ_A2 == 0): countHZ_A2 = 1;
            if (countHZ_A3 == 0): countHZ_A3 = 1;

            dc_rms_a1 = sqrt(float(rmsA1) / totalCnt)/1024.*ADC_BASE_VOLTAGE
            dc_rms_a2 = sqrt(float(rmsA2) / totalCnt)/1024.*ADC_BASE_VOLTAGE
            dc_rms_a3 = sqrt(float(rmsA3) / totalCnt)/1024.*ADC_BASE_VOLTAGE

            ac_rms_a1 = sqrt(float(rmsAC_A1) / countHZ_A1)/1024.*ADC_BASE_VOLTAGE * 0.707;
            ac_rms_a2 = sqrt(float(rmsAC_A2) / countHZ_A2)/1024.*ADC_BASE_VOLTAGE * 0.707;
            ac_rms_a3 = sqrt(float(rmsAC_A3) / countHZ_A3)/1024.*ADC_BASE_VOLTAGE * 0.707;

            rmsA1 = 0;
            rmsA2 = 0;
            rmsA3 = 0;
            peakA1 = 0;
            peakA2 = 0;
            peakA3 = 0;
            rmsAC_A1 = 0;
            rmsAC_A2 = 0;
            rmsAC_A3 = 0;
            totalCnt = 0;
            countHZ_A1 = 0;
            countHZ_A2 = 0;
            countHZ_A3 = 0;
            hp_A1 = 0;
            lp_A1 = 5000;
            hp_A2 = 0;
            lp_A2 = 5000;
            hp_A3 = 0;
            lp_A3 = 5000;

            print(line.strip())
            print("Local: %.03f,%.03f,%.03f %.03f,%.03f,%.03f" % (dc_rms_a1, dc_rms_a2, dc_rms_a3, ac_rms_a1, ac_rms_a2, ac_rms_a3))

            dc_y = int((dc_rms_a1/ADC_BASE_VOLTAGE - 0.5 - 0.3) * 720 + 360)
            buf[dc_y-1:dc_y+1, :, :] = (255, 0, 0)
            
            dc_y = int((dc_rms_a2/ADC_BASE_VOLTAGE - 0.5) * 720 + 360)
            buf[dc_y-1:dc_y+1, :, :] = (255, 0, 0)
            
            dc_y = int((dc_rms_a3/ADC_BASE_VOLTAGE - 0.5 + 0.3) * 720 + 360)
            buf[dc_y-1:dc_y+1, :, :] = (255, 0, 0)

            last_i = i
        elif line[0:2] == "S:" and line[-4:] == ":D\r\n":
            v = line.strip()[2:-2].split(',')
            if (len(v) != 3): continue
            a1 = int(v[0])
            a2 = int(v[1])
            a3 = int(v[2])
            if (abs(a3 - a2) > 100) or (abs(a2 - a1) > 100) or (abs(a3 - a1) > 100):
                print(v)

            if (a1 > hp_A1):
                hp_A1 = a1
                pp_A1 = lp_A1 + (hp_A1 - lp_A1) / 2

            if (a1 < lp_A1):
                lp_A1 = a1;
                pp_A1 = lp_A1 + (hp_A1 - lp_A1) / 2;

            if (a2 > hp_A2) :
                hp_A2 = a2;
                pp_A2 = lp_A2 + (hp_A2 - lp_A2) / 2;

            if (a2 < lp_A2) :
                lp_A2 = a2;
                pp_A2 = lp_A2 + (hp_A2 - lp_A2) / 2;

            if (a3 > hp_A3) :
                hp_A3 = a3;
                pp_A3 = lp_A3 + (hp_A3 - lp_A3) / 2;

            if (a3 < lp_A3) :
                lp_A3 = a3;
                pp_A3 = lp_A3 + (hp_A3 - lp_A3) / 2;


            if (a1 - pp_A1 <= 0 and lastVolA1 - pp_A1 > 0):  
                rmsAC_A1 += peakA1 * peakA1;
                peakA1 = 0;
                countHZ_A1 = countHZ_A1 + 1;


            if (a2 - pp_A2 <= 0 and lastVolA2 - pp_A2 > 0):
                rmsAC_A2 += peakA2 * peakA2;
                peakA2 = 0;
                countHZ_A2 = countHZ_A2 + 1;

            if (a3 - pp_A3 <= 0 and lastVolA3 - pp_A3 > 0):
                rmsAC_A3 += peakA3 * peakA3;
                peakA3 = 0;
                countHZ_A3 = countHZ_A3 + 1;

            lastVolA1 = a1;
            lastVolA2 = a2;
            lastVolA3 = a3;
            rmsA1 += a1 * a1;
            rmsA2 += a2 * a2;
            rmsA3 += a3 * a3;
            if (a1 - pp_A1 > peakA1): peakA1 = a1 - pp_A1;
            if (a2 - pp_A2 > peakA2): peakA2 = a2 - pp_A2;
            if (a3 - pp_A3 > peakA3): peakA3 = a3 - pp_A3;

            totalCnt += 1;

            phase_a = float(v[0])/1024 - 0.5 - 0.3
            phase_b = float(v[1])/1024 - 0.5
            phase_c = float(v[2])/1024 - 0.5 + 0.3

            cv2.line(buf,(i - 1, int(last_a * 720 + 360)),(i, int(phase_a * 720 + 360)),(0,255,255),1)
            cv2.line(buf,(i - 1, int(last_b * 720 + 360)),(i, int(phase_b * 720 + 360)),(0,255,0),1)
            cv2.line(buf,(i - 1, int(last_c * 720 + 360)),(i, int(phase_c * 720 + 360)),(0,0,255),1)

            last_a = phase_a
            last_b = phase_b
            last_c = phase_c

            # buf[int(720 * ), i, 0] = 255
            # buf[int(720 * (float(v[1])/1024)), i, 1] = 255
            # buf[int(720 * (float(v[2])/1024)), i, 2] = 255
            i+=1
            if i >= 1280:
                i = 0
                # buf = np.zeros((720, 1280, 3), np.uint8)

            if time.time() - last >= 0.1:
                scope = buf.copy()
                scope[:,i+1,:]=128
                cv2.imshow("Scope", scope)
                cv2.waitKey(1)
                last = time.time()

            if i % 10 == 0:
                buf[:,i:i+10,:]=0
    except Exception as e:
        print(phase_a, phase_b, phase_c, line)
        pass
    lbuf = ""