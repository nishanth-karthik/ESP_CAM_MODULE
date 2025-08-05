import cv2
import numpy as np
import serial
import time
from collections import defaultdict

# --- Serial Config ---
ser = serial.Serial('COM9', 115200, timeout=10)
time.sleep(2)

# --- YOLO Parameters ---
whT = 224 # low resolution (faster, less accurate)
#whT = 320 medium resolution
#whT = 416 high resolution (slower, more accurate)

confThreshold = 0.3
nmsThreshold = 0.3
with open("coco.names", 'rt') as f:
    classNames = f.read().rstrip('\n').split('\n')

net = cv2.dnn.readNetFromDarknet("yolov3-tiny.cfg", "yolov3-tiny.weights") #works accurate with yolov3.cfg &yolov3.weights
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

# --- Trigger ESP to capture ---
def request_image():
    ser.write(b"CAPTURE\n")
    print("[Python] Sent: CAPTURE")

# --- Receive image from ESP32 ---
def receive_image():
    length_bytes = ser.read(4)
    if len(length_bytes) != 4:
        print("Timeout or no data received")
        return None

    img_len = int.from_bytes(length_bytes, 'little')
    print(f"[Python] Receiving {img_len} bytes...")
    
    img_data = bytearray()
    while len(img_data) < img_len:
        packet = ser.read(img_len - len(img_data))
        if not packet:
            break
        img_data.extend(packet)

    if len(img_data) != img_len:
        print("Incomplete image")
        return None
    
     # Decode the JPEG byte stream to OpenCV image
    img_np = np.frombuffer(img_data, dtype=np.uint8)
    img = cv2.imdecode(img_np, cv2.IMREAD_COLOR)
    return img

# --- YOLO Object Detection ---
def detect_and_count(img):
    blob = cv2.dnn.blobFromImage(img, 1 / 255, (whT, whT), [0, 0, 0], 1, crop=False)
    net.setInput(blob)
    layernames = net.getLayerNames()
    outputNames = [layernames[i - 1] for i in net.getUnconnectedOutLayers()]
    outputs = net.forward(outputNames)

    hT, wT, _ = img.shape
    bbox, classIds, confs = [], [], []

    for output in outputs:
        for det in output:
            scores = det[5:]
            classId = np.argmax(scores)
            confidence = scores[classId]
            if confidence > confThreshold:
                w, h = int(det[2] * wT), int(det[3] * hT)
                x, y = int(det[0] * wT - w / 2), int(det[1] * hT - h / 2)
                bbox.append([x, y, w, h])
                classIds.append(classId)
                confs.append(float(confidence))

    indices = cv2.dnn.NMSBoxes(bbox, confs, confThreshold, nmsThreshold)
    count_dict = defaultdict(int)

    # Draw results and count objects
    if len(indices) > 0:
        for i in indices.flatten():
            label = classNames[classIds[i]]
            count_dict[label] += 1
            x, y, w, h = bbox[i]
            cv2.rectangle(img, (x, y), (x+w, y+h), (255, 0, 255), 2)
            cv2.putText(img, f"{label} {count_dict[label]}", (x, y - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 255), 2)

    return img, count_dict

# --- Send detection result back to ESP32 ---
def send_result(count_dict):
    if not count_dict:
        print("[Python] No objects detected.")
        return

    msg = ",".join([f"{k}:{v}" for k, v in count_dict.items()])
    framed = f"<{msg}>"
    print(f"[Python] Sending to ESP: {framed}")
    ser.write(framed.encode())

# --- Main Loop ---
'''while True:
    input("Press Enter to trigger image capture...")
    request_image()
    img = receive_image()
    if img is None:
        continue

    result_img, counts = detect_and_count(img)
    send_result(counts)

    cv2.imshow("Detection", result_img)
    if cv2.waitKey(0) & 0xFF == ord('q'):
        break'''

while True:
    print("Capturing new image...")
    request_image()
    img = receive_image()
    if img is None:
        continue

    result_img, counts = detect_and_count(img)
    send_result(counts)

    cv2.imshow("Detection", result_img)
    print("Press 'q' to quit or any other key to capture again...")
    key = cv2.waitKey(0) & 0xFF
    if key == ord('q'):
        break

# Cleanup
cv2.destroyAllWindows()
ser.close()

