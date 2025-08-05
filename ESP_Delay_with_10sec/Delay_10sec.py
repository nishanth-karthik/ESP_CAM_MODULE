import cv2
import numpy as np
import urllib.request
from collections import defaultdict
import serial
import time
 
# --- Setup ESP32-CAM snapshot URL ---
#url = 'http://192.168.108.79/cam-mid.jpg' # Replace with your ESP32-CAM IP
#url = 'http://172.30.91.79/cam-hi.jpg'
url = 'http://172.30.91.79/cam-mid.jpg'
#ESP32_IP = 'http://172.30.91.79'
#url = f'{ESP32_IP}/capture'



 
# --- YOLO model parameters ---
#whT = 320 # <- Reduced from 320 to 224 for faster processing
whT = 416
#confThreshold = 0.5  # <- Increased to reduce weak detections (default=0.5,0.6)
confThreshold = 0.3
nmsThreshold = 0.3   # <- Increased to reduce overlapping boxes (default=0.3)
 
#--- Setup serial communication ---
try:
    ser = serial.Serial('COM9', 115200, timeout=0)  # Replace COM10 with your port,,, up to 5 seconds for the ESP32-CAM HTTP snapshot to respond.
    #time.sleep(2)  # Allow time for ESP32 to boot/reset after port opens
    #time.sleep(1)
  
    
    print("Serial connection established.")
except Exception as e:
    print(f"Error opening serial port: {e}")
    ser = None
 
#--- Load class names ---
DELAY_CHANGES_PYTHON = 'coco.names'
with open(DELAY_CHANGES_PYTHON, 'rt') as f:
    classNames = f.read().rstrip('\n').split('\n')
 
#--- Load YOLOv3 model ---
#modelConfig = 'yolov3.cfg'
#modelWeights = 'yolov3.weights'

# Use yolov3-tiny for faster performance
modelConfig = 'yolov3-tiny.cfg'     # <- Change here
modelWeights = 'yolov3-tiny.weights'  # <- Change here
net = cv2.dnn.readNetFromDarknet(modelConfig, modelWeights)
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

 
#--- Function to send data via serial in <label:count,...> format ---
def send_serial_data(count_dict):
    if ser and ser.is_open and count_dict:
        try:
            serial_output = ",".join([f"{k}:{v}" for k, v in count_dict.items()])
            framed_output = f"<{serial_output}>"  # Example: <person:2,car:1>
            print(f"Sending to ESP32: {framed_output}")
            ser.write(framed_output.encode())
            ser.flush()
        except Exception as e:
            print(f"Error sending serial data: {e}")
 
#--- Object Detection Function ---
def findObject(outputs, im):
    hT, wT, cT = im.shape
    bbox = []
    classIds = []
    confs = []
 
    for output in outputs:
        for det in output:
            scores = det[5:]
            classId = np.argmax(scores)
            confidence = scores[classId]
            if confidence > confThreshold:
                w, h = int(det[2] * wT), int(det[3] * hT)
                x, y = int((det[0] * wT) - w / 2), int((det[1] * hT) - h / 2)
                bbox.append([x, y, w, h])
                classIds.append(classId)
                confs.append(float(confidence))
 
    indices = cv2.dnn.NMSBoxes(bbox, confs, confThreshold, nmsThreshold)

    # Debug: print how many objects were detected
    print(f"Total Detections (raw): {len(classIds)}")
    print(f"Filtered with NMS: {len(indices)}")
 
    # Dictionary to count detected object types
    count_dict = defaultdict(int)

 
    if len(indices) > 0:
        indices = indices.flatten()
        for i in indices:
            box = bbox[i]
            x, y, w, h = box
            label = classNames[classIds[i]]
            count_dict[label] += 1
            count_text = f"{label.upper()} {count_dict[label]}"
            cv2.rectangle(im, (x, y), (x + w, y + h), (255, 0, 255), 2)
            cv2.putText(im, count_text, (x, y - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 255), 2)
 
    # Console Output + Serial Send
    if count_dict:
        print("Detected objects:")
        for label, count in count_dict.items():
            print(f"{label}: {count}")
        print("-" * 30)
        send_serial_data(count_dict)
 
#--- Main loop ---
while True:
    try:
        # Request JPEG image from ESP32-CAM
        img_resp = urllib.request.urlopen(url, timeout=5)
        imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
        im = cv2.imdecode(imgnp, -1)

        if im is None:
            raise ValueError("Empty image received from ESP32")
        
        # Prepare blob for YOLO
        blob = cv2.dnn.blobFromImage(im, 1 / 255, (whT, whT), [0, 0, 0], 1, crop=False)
        #blob = cv2.dnn.blobFromImage(im, 1/255, (320, 320), [0,0,0], 1, crop=False)

        net.setInput(blob)
        layernames = net.getLayerNames()
        outputNames = [layernames[i - 1] for i in net.getUnconnectedOutLayers()]
        outputs = net.forward(outputNames)

        # Perform detection
        findObject(outputs, im)

        # Display result
        cv2.imshow('YOLO Detection with Count', im)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

        #Optional short sleep to reduce load (can tweak/remove)
        #time.sleep(0.1)
        time.sleep(10)  # Wait __ seconds before next trigger

    except Exception as e:
        print(f"Error fetching image or processing: {e}")
        


