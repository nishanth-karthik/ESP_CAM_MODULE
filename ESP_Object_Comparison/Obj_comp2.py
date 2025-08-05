import cv2
import numpy as np
import serial
import time
from collections import defaultdict

# --- Serial Configuration (Update COM Port as Needed) ---
ser = serial.Serial('COM9', 115200, timeout=10)  # Update COM port to match your TTL module
time.sleep(20)

# --- YOLO Parameters ---
whT = 224  # Resize image for YOLO
confThreshold = 0.3
nmsThreshold = 0.3

# Load class names from COCO dataset
with open("coco.names", 'rt') as f:
    classNames = f.read().rstrip('\n').split('\n')

# Load YOLOv3 model
net = cv2.dnn.readNetFromDarknet("yolov3.cfg", "yolov3.weights")
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

# --- Trigger ESP32 to Capture Image ---
def request_image():
    ser.write(b"CAPTURE\n")
    print("[Python] Sent: CAPTURE")

# --- Receive Image from ESP32 (JPEG) ---
def receive_image():
    length_bytes = ser.read(4)
    if len(length_bytes) != 4:
        print("[Error] Timeout or no data received")
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
        print("[Error] Incomplete image received")
        return None

    img_np = np.frombuffer(img_data, dtype=np.uint8)
    img = cv2.imdecode(img_np, cv2.IMREAD_COLOR)
    return img

# --- YOLO Object Detection ---
def detect_and_count(img):
    blob = cv2.dnn.blobFromImage(img, 1/255, (whT, whT), [0, 0, 0], 1, crop=False)
    net.setInput(blob)
    layernames = net.getLayerNames()
    outputNames = [layernames[i - 1] for i in net.getUnconnectedOutLayers()]
    outputs = net.forward(outputNames)

    hT, wT, _ = img.shape
    bbox, classIds, confs = [], [], []
    count_dict = defaultdict(int)

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

    for i in indices:
        i = i[0] if isinstance(i, (list, tuple, np.ndarray)) else i
        label = classNames[classIds[i]]
        count_dict[label] += 1
        x, y, w, h = bbox[i]
        cv2.rectangle(img, (x, y), (x+w, y+h), (255, 0, 255), 2)
        cv2.putText(img, f"{label}", (x, y - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 255), 2)

    return img, count_dict

# --- Baseline Storage ---
baseline_counts = None  # Dictionary of object label → count

# --- Compare and Send Missing Objects to ESP32 ---
def send_result(current_counts):
    global baseline_counts

    if baseline_counts is None:
        baseline_counts = current_counts.copy()
        print("[Baseline] Stored as reference:", baseline_counts)
        return

    missing = []
    for obj in baseline_counts:
        base_count = baseline_counts[obj]
        curr_count = current_counts.get(obj, 0)
        if curr_count < base_count:
            missing.append(obj)  # Only send label (no quantity)


     # 2. Build detection summary string: "person:1,chair:2"
    detected_msg = ",".join([f"{k}:{v}" for k, v in current_counts.items()])



       # 3. Combine missing + detected into final message
    if missing:
        msg = f"Missing: {','.join(missing)}|{detected_msg}"
    else:
        msg = f"No missing objects|{detected_msg}"

    framed = f"<{msg}>"
    print(f"[Python] Sending to ESP: {framed}")
    ser.write(framed.encode())

# --- Main Loop ---
while True:
    print("\n[Main] Requesting new image...")
    request_image()
    img = receive_image()
    if img is None:
        continue

    result_img, counts = detect_and_count(img)
    send_result(counts)

    cv2.imshow("YOLO Detection", result_img)
    print("Press 'q' to quit, 'r' to reset baseline, any other key to continue...")
    key = cv2.waitKey(0) & 0xFF
    if key == ord('q'):
        break
    elif key == ord('r'):
        baseline_counts = None
        print("[Python] Baseline reset. Next image will be new reference.")

# Cleanup
cv2.destroyAllWindows()
ser.close()
