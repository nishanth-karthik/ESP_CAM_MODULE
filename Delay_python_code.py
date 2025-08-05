import cv2
import numpy as np
import urllib.request
from collections import defaultdict
import serial
import time

# --- Setup ESP32-CAM snapshot URL ---
url = 'http://172.30.91.79/snap'  # <-- Use the /snap endpoint for fresh capture

# --- YOLO model parameters ---
whT = 416
confThreshold = 0.3
nmsThreshold = 0.3

# --- Setup serial communication ---
try:
    ser = serial.Serial('COM9', 115200, timeout=10)
    print("Serial connection established.")
except Exception as e:
    print(f"Error opening serial port: {e}")
    ser = None

# --- Load class names ---
with open('coco.names', 'rt') as f:
    classNames = f.read().rstrip('\n').split('\n')

# --- Load YOLOv3-tiny model ---
modelConfig = 'yolov3.cfg'
modelWeights = 'yolov3.weights'
net = cv2.dnn.readNetFromDarknet(modelConfig, modelWeights)
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

# --- Track last sent detection ---
last_sent = None

def send_serial_data(count_dict):
    global last_sent
    if ser and ser.is_open and count_dict:
        try:
            serial_output = ",".join([f"{k}:{v}" for k, v in count_dict.items()])
            if serial_output != last_sent:
                framed_output = f"<{serial_output}>"
                print(f"Sending to ESP32: {framed_output}")
                ser.write(framed_output.encode())
                ser.flush()
                last_sent = serial_output
        except Exception as e:
            print(f"Error sending serial data: {e}")

def findObject(outputs, im):
    hT, wT, _ = im.shape
    bbox, classIds, confs = [], [], []

    for output in outputs:
        for det in output:
            scores = det[5:]
            classId = np.argmax(scores)
            confidence = scores[classId]
            if confidence > confThreshold:
                w, h = int(det[2]*wT), int(det[3]*hT)
                x, y = int(det[0]*wT - w/2), int(det[1]*hT - h/2)
                bbox.append([x, y, w, h])
                classIds.append(classId)
                confs.append(float(confidence))

    indices = cv2.dnn.NMSBoxes(bbox, confs, confThreshold, nmsThreshold)
    count_dict = defaultdict(int)

    if len(indices) > 0:
        for i in indices.flatten():
            x, y, w, h = bbox[i]
            label = classNames[classIds[i]]
            count_dict[label] += 1
            count_text = f"{label.upper()} {count_dict[label]}"
            cv2.rectangle(im, (x, y), (x+w, y+h), (255, 0, 255), 2)
            cv2.putText(im, count_text, (x, y-10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 255), 2)

    if count_dict:
        print("Detected:")
        for label, count in count_dict.items():
            print(f"{label}: {count}")
        send_serial_data(count_dict)

# --- Main loop ---
while True:
    try:
        img_resp = urllib.request.urlopen(url, timeout=5)
        imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
        im = cv2.imdecode(imgnp, -1)

        if im is None:
            raise ValueError("Empty image received")

        blob = cv2.dnn.blobFromImage(im, 1/255, (whT, whT), [0,0,0], 1, crop=False)
        net.setInput(blob)
        outputNames = [net.getLayerNames()[i - 1] for i in net.getUnconnectedOutLayers().flatten()]
        outputs = net.forward(outputNames)

        findObject(outputs, im)
        cv2.imshow('YOLO Detection', im)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

        time.sleep(10)  # Wait 10 seconds before next capture

    except Exception as e:
        print(f"Error: {e}")
        time.sleep(10)
