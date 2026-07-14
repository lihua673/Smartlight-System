import sensor, image, time, lcd
from maix import KPU
from modules import ybserial
import time
import binascii

serial = ybserial()

#字符串转10进制
def str_int(data_str):
    bb = binascii.hexlify(data_str)
    bb = str(bb)[2:-1]
    hex_1 = int(bb[0])*16
    hex_2 = int(bb[1],16)
    return hex_1+hex_2

# 发送所有人脸框（新协议：含人数 + 多张人脸坐标）
def send_faces(dect):
    start = 0x24
    end = 0x23
    class_num = 0x05    # 例程编号
    class_group = 0xBB  # 例程组
    fenge = 0x2C        # 逗号分隔符
    crc = 0             # 校验位

    face_count = len(dect)  # 实际检测到的人脸数量
    data = []

    # 第1个字节: 人数
    data.append(face_count)
    data.append(fenge)

    # 依次编码每张脸: x_lo,x_hi, y_lo,y_hi, w_lo,w_hi, h_lo,h_hi (全部小端模式)
    for face in dect:
        x, y, w, h = face[0], face[1], face[2], face[3]

        # x (小端模式)
        data.append(x & 0xFF)
        data.append(fenge)
        data.append((x >> 8) & 0xFF)
        data.append(fenge)

        # y (小端模式)
        data.append(y & 0xFF)
        data.append(fenge)
        data.append((y >> 8) & 0xFF)
        data.append(fenge)

        # w (小端模式)
        data.append(w & 0xFF)
        data.append(fenge)
        data.append((w >> 8) & 0xFF)
        data.append(fenge)

        # h (小端模式)
        data.append(h & 0xFF)
        data.append(fenge)
        data.append((h >> 8) & 0xFF)
        data.append(fenge)

    data_num = len(data)
    length = 5 + len(data)

    # 组帧: length, class_num, class_group, data_num, data...
    send_merr = [length, class_num, class_group, data_num]
    for i in range(data_num):
        send_merr.append(data[i])

    # CRC = 帧头后、CRC位前所有字节之和 % 256
    for i in range(len(send_merr)):
        crc += send_merr[i]
    crc = crc % 256

    send_merr.insert(0, start)  # 插入帧头
    send_merr.append(crc)       # 插入CRC
    send_merr.append(end)       # 插入帧尾

    global send_buf
    send_buf = send_merr


send_buf = []
lcd.init()
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.skip_frames(time = 100)
clock = time.clock()

od_img = image.Image(size=(320,256))

anchor = (0.893, 1.463, 0.245, 0.389, 1.55, 2.58, 0.375, 0.594, 3.099, 5.038, 0.057, 0.090, 0.567, 0.904, 0.101, 0.160, 0.159, 0.255)
kpu = KPU()
kpu.load_kmodel("yolo_face_detect.kmodel")
kpu.init_yolo2(anchor, anchor_num=9, img_w=320, img_h=240, net_w=320, net_h=256, layer_w=10, layer_h=8, threshold=0.7, nms_value=0.3, classes=1)

while True:
    clock.tick()
    img = sensor.snapshot()
    a = od_img.draw_image(img, 0,0)
    od_img.pix_to_ai()
    kpu.run_with_output(od_img)
    dect = kpu.regionlayer_yolo2()
    fps = clock.fps()

    # 绘制所有人脸框
    if len(dect) > 0:
        for l in dect:
            a = img.draw_rectangle(l[0], l[1], l[2], l[3], color=(0, 255, 0))

    # 显示FPS和人数在屏幕左上角
    a = img.draw_string(0, 0, "%2.1ffps %df" % (fps, len(dect)), color=(0, 60, 128), scale=2.0)
    lcd.display(img)

    # 发送数据帧（每帧都发，无人脸时face_count=0）
    send_faces(dect)
    serial.send_bytearray(send_buf)
    print(send_buf)

kpu.deinit()
