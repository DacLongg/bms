# BMS UART Communication Protocol

> **Lưu ý tương thích (28/10/2026):** Đây là bản thiết kế V1.0 lịch sử, không phải khung đang chạy. Các chức năng mang mã `F0/F1/F2/F3/F4/F5` nay lần lượt có ID `15/31/16/17/18/19` (hex) theo nhóm lệnh firmware; đọc cả 10 cell bằng `11`, đọc dòng bằng `14`. Lệnh `19` trả số chu kỳ **xả tương đương hoàn tất**, không phải số phiên sạc hoàn tất của F5 cũ. Khung hiện tại là `AA 55 CMD LEN DATA CRC` và phản hồi có byte `STATUS`. Để tích hợp, dùng [tài liệu gửi đối tác](BMS_UART_Protocol_For_Partner.md); để đối chiếu chi tiết firmware, dùng [đặc tả theo source](bms_uart_protocol_from_source.md).

**Mã tài liệu:** BMS-UART-001  
**Phiên bản:** 1.0  
**Ngày:** 24/10/2026  

---


<!-- Trang 1 -->

TÀI LIỆU KỸ THUẬT • GIAO THỨC TRUYỀN THÔNG

Đặc tả giao tiếp bộ quản lý pin 10 cell

Thuộc tính                      Nội dung

Đơn vị biên soạn              Phòng kỹ thuật - Tổ điện

Mã tài liệu                   BMS-UART-001

Phiên bản                         1.0

Ngày phát hành                  24/10/2026

Trạng thái                Dự thảo kỹ thuật cho triển khai và kiểm thử

Phạm vi                      Một bộ điều khiển chính (Host) và một BMS, 10 cell nối tiếp

Giao diện                   UART, 115200 baud, 8N1

## 1. Giới thiệu

Tài liệu quy định định dạng bản tin, mã lệnh, dữ liệu phản hồi và phương pháp kiểm tra lỗi giữa Host và BMS. BMS chỉ phát

phản hồi khi nhận được yêu cầu hợp lệ từ Host; không phát dữ liệu định kỳ trong phiên bản này.

Đây là giao thức riêng được thiết kế cho hệ thống BMS đang phát triển. Thuật toán CRC-16/MODBUS chỉ dùng để kiểm tra

lỗi; bản tin không sử dụng định dạng Modbus RTU.

### 1.1. Lịch sử sửa đổi

Phiên bản      Ngày                 Nội dung

1.0              24/10/2026         Chuẩn hóa các mã A1, F0, F1, F2, F3, F4, F5 và phản hồi lỗi FF; bổ sung nhiệt độ pin F4

và bộ đếm số lần sạc F5.

### 1.2. Cấu trúc tài liệu

Trang        Nội dung

2           Giao diện UART, định dạng bản tin, CRC và quy tắc giao dịch

3           Lệnh A1 đọc điện áp cell; F0 đọc trạng thái pin

4           Lệnh F1 cân bằng; F2 điện áp tổng; F3 số cell cấu hình

5           Lệnh F4 nhiệt độ pin; F5 số lần sạc

6            Định nghĩa bit trạng thái, lỗi bảo vệ và phản hồi lỗi FF

7          Các bản tin mẫu đã tính CRC và tiêu chí kiểm thử


<!-- Trang 2 -->

## 2. Giao diện và định dạng bản tin

Thông số                       Giá trị

Tốc độ truyền                  115200 bit/s

Định dạng ký tự                8 bit dữ liệu, không parity, 1 stop bit (8N1)

Mô hình giao dịch                Host gửi yêu cầu; BMS trả lời; một yêu cầu đang chờ

Kết nối                         Host TX → BMS RX; Host RX ← BMS TX; tham chiếu GND chung

Mức điện áp logic              Theo thiết kế phần cứng; hai phía phải tương thích. Không đấu trực tiếp mức RS232 vào UART

logic.

### 2.1. Cấu trúc bản tin

AA | CMD | LEN | DATA[0 ... LEN-1] | CRC_L | CRC_H

Offset    Trường             Byte     Giá trị / diễn giải

0       HEADER          1      Cố định 0xAA

1      CMD              1      Mã lệnh yêu cầu hoặc phản hồi

2       LEN              1       Số byte DATA, uint8, từ 0 đến 255

3 ...     DATA           LEN      Nội dung theo từng lệnh; không có trường này khi LEN = 0

3 +      CRC_L            1        Byte thấp CRC16

LEN

4 +     CRC_H            1        Byte cao CRC16

LEN

Tổng chiều dài = 5 + LEN byte. LEN được kiểm tra theo từng lệnh; giới hạn trường LEN không đồng nghĩa mọi độ dài đều

hợp lệ. Giá trị nhiều byte dùng little-endian. Mọi hằng số trong khung mẫu là hex; số thứ tự cell và đơn vị vật lý được ghi rõ.

### 2.2. Kiểm tra CRC

Thuộc tính                      Giá trị

Thuật toán                 CRC-16/MODBUS

Giá trị khởi tạo / XOR cuối       0xFFFF / 0x0000

Polynomial                      0x8005; dạng phản chiếu dùng khi tính: 0xA001

RefIn / RefOut                      true / true

Phạm vi tính                Từ CMD đến hết DATA; không gồm HEADER hoặc hai byte CRC

Thứ tự truyền               CRC_L trước, CRC_H sau

### 2.3. Quy tắc giao dịch

Host chỉ gửi yêu cầu tiếp theo sau khi hoàn tất giao dịch hoặc xử lý timeout. Timeout phản hồi đề xuất là 200 ms tính từ cuối

yêu cầu. BMS phải hoàn tất phản hồi trong khoảng này; phản hồi không còn kịp thời hạn phải được hủy. Host bỏ dữ liệu đến

muộn trước giao dịch tiếp theo.

Bên nhận tìm AA, đọc CMD và LEN, sau đó nhận đúng LEN + 2 byte. AA trong DATA là dữ liệu. Khi nhận thiếu quá 200 ms,

hủy khung và đồng bộ lại. CRC sai: bỏ khung, không phản hồi lỗi. CRC đúng nhưng yêu cầu sai: phản hồi FF theo mục 6.3.

Phản hồi thành công giữ nguyên CMD. A1 còn phải đối chiếu CELL. Không có SEQ nên không phân biệt được phản hồi cũ

và mới của cùng lệnh/cùng cell. Bảo vệ pin chạy độc lập với giao tiếp UART.


<!-- Trang 3 -->

## 3. Lệnh đọc điện áp cell và trạng thái

### 3.1. CMD 0xA1 - Đọc điện áp một cell

Host chọn đúng một cell bằng DATA = CELL. Trong phạm vi tài liệu, CELL từ 0x01 đến 0x0A tương ứng cell 1 đến cell 10

theo thứ tự dây đo BMS.

Yêu cầu (6 byte): AA | A1 | 01 | CELL | CRC_L | CRC_H

Offset    Trường             Byte     Giá trị / diễn giải

0       HEADER          1      AA

1      CMD              1       A1

2       LEN              1       01

3       CELL             1        01-0A: cell 1-10

4-5      CRC16            2        Tính trên A1 01 CELL; byte thấp trước

Phản hồi (8 byte): AA | A1 | 03 | CELL | V_L | V_H | CRC_L | CRC_H

Offset    Trường             Byte     Giá trị / diễn giải

0-2       Header chung       3      AA A1 03

3       CELL             1       Giữ nguyên số cell được yêu cầu

4        V_L               1        Byte thấp điện áp cell

5       V_H              1        Byte cao điện áp cell

6-7      CRC16            2        Tính từ CMD đến V_H; byte thấp trước

Điện áp cell là uint16, đơn vị mV: V = V_L + 256 × V_H. CELL ngoài phạm vi: lỗi 03. Dữ liệu chưa hợp lệ hoặc BMS chưa sẵn

sàng: lỗi 04.

### 3.2. CMD 0xF0 - Đọc trạng thái pin

Yêu cầu (5 byte): AA | F0 | 00 | CRC_L | CRC_H

Phản hồi (9 byte): AA | F0 | 04 | SOC | STATUS | FAULT_L | FAULT_H | CRC_L | CRC_H

Offset    Trường             Byte     Giá trị / diễn giải

0-2       Header chung       3      AA F0 04

3      SOC              1          uint8; dung lượng còn lại từ 0 đến 100 (%)

4       STATUS           1           Bit trạng thái theo mục 6.1

5        FAULT_L          1        Byte thấp cờ lỗi bảo vệ

6       FAULT_H          1        Byte cao cờ lỗi bảo vệ

7-8      CRC16            2        Tính từ CMD đến FAULT_H; byte thấp trước

Cờ lỗi = FAULT_L + 256 × FAULT_H. Cờ lỗi biểu thị điều kiện bảo vệ đang hiện diện, không phải nhật ký lỗi. Khi SOC hoặc

dữ liệu trạng thái chưa hợp lệ, BMS trả lỗi 04.


<!-- Trang 4 -->

## 4. Cân bằng và thông số bộ pin

### 4.1. CMD 0xF1 - Đặt chế độ cân bằng

Yêu cầu: AA | F1 | 01 | MODE | CRC_L | CRC_H

Phản hồi: AA | F1 | 01 | MODE | CRC_L | CRC_H

Offset    Trường             Byte     Giá trị / diễn giải

0-2       Header chung       3      AA F1 01 cho cả hai chiều

3      MODE             1         00: tắt; 01: cho phép cân bằng tự động

4-5      CRC16            2        Tính trên F1 01 MODE; byte thấp trước

Mỗi khung dài 6 byte. Phản hồi xác nhận MODE đã được chấp nhận. MODE khác 00/01 trả lỗi 03. Gửi lại cùng MODE không

đảo trạng thái. Khi khởi động, mặc định MODE = 00.

MODE = 01 cho phép BMS tự chọn cell dựa trên các ngưỡng điện áp, độ lệch và điều kiện bảo vệ đã cấu hình trong

firmware. Lệnh không ép bật cell khi điều kiện bảo vệ không cho phép. Dùng STATUS bit 4 để biết có đang cân bằng thực tế,

bit 5 để biết chế độ tự động đã bật. Các ngưỡng cân bằng nằm ngoài phạm vi giao thức này.

### 4.2. CMD 0xF2 - Đọc điện áp tổng

Yêu cầu (5 byte): AA | F2 | 00 | CRC_L | CRC_H

Phản hồi (9 byte): AA | F2 | 04 | V0 | V1 | V2 | V3 | CRC_L | CRC_H

Offset    Trường             Byte     Giá trị / diễn giải

0-2       Header chung       3      AA F2 04 trong phản hồi

3-6       V0, V1, V2, V3      4          uint32, little-endian, đơn vị mV

7-8      CRC16            2        Tính từ CMD đến V3; byte thấp trước

Vpack = V0 + 256 × V1 + 65536 × V2 + 16777216 × V3. Đây là giá trị điện áp pack do BMS cung cấp; không yêu cầu Host

cộng các cell để thay thế giá trị này. Dữ liệu chưa hợp lệ: lỗi 04.

### 4.3. CMD 0xF3 - Đọc số cell đã cấu hình

Yêu cầu (5 byte): AA | F3 | 00 | CRC_L | CRC_H

Phản hồi (6 byte): AA | F3 | 01 | N_CELL | CRC_L | CRC_H

Offset    Trường             Byte     Giá trị / diễn giải

0-2       Header chung       3      AA F3 01 trong phản hồi

3       N_CELL           1          uint8; số cell nối tiếp lưu trong cấu hình BMS

4-5      CRC16            2        Tính trên F3 01 N_CELL; byte thấp trước

Với cấu hình của tài liệu này, N_CELL = 0x0A (10 cell). Giá trị là số cell được cấu hình, không phải số cell đo được hoặc còn

hoạt động. Lệnh chỉ đọc; không thay đổi cấu hình. Nếu chưa có cấu hình hợp lệ, BMS trả lỗi 04.


<!-- Trang 5 -->

## 5. Nhiệt độ pin và số lần sạc

### 5.1. CMD 0xF4 - Đọc nhiệt độ pin

Yêu cầu (5 byte): AA | F4 | 00 | CRC_L | CRC_H

Phản hồi (7 byte): AA | F4 | 02 | T_L | T_H | CRC_L | CRC_H

Offset    Trường             Byte     Giá trị / diễn giải

0-2       Header chung       3      AA F4 02 trong phản hồi

3-4       T_L, T_H          2          int16, bù hai, little-endian; đơn vị 0,1°C

5-6      CRC16            2        Tính từ CMD đến T_H; byte thấp trước

Ghép U = T_L + 256 × T_H. Nếu U ≥ 32768 thì giá trị có dấu S = U - 65536; ngược lại S = U. Nhiệt độ = S / 10 (°C). Ví dụ FF

00 = 25,5°C; 9C FF = -10,0°C. BMS cung cấp một giá trị nhiệt độ pin do firmware xác định. Chưa có dữ liệu hợp lệ hoặc lỗi

cảm biến: trả lỗi 04, không dùng 0°C thay lỗi.

### 5.2. CMD 0xF5 - Đọc số lần sạc

Yêu cầu (5 byte): AA | F5 | 00 | CRC_L | CRC_H

Phản hồi (9 byte): AA | F5 | 04 | C0 | C1 | C2 | C3 | CRC_L | CRC_H

Offset    Trường             Byte     Giá trị / diễn giải

0-2       Header chung       3      AA F5 04 trong phản hồi

3       C0                1        Byte thấp nhất của bộ đếm

4       C1                1        Byte thứ hai của bộ đếm

5       C2                1        Byte thứ ba của bộ đếm

6       C3                1        Byte cao nhất của bộ đếm

7-8      CRC16            2        Tính từ CMD đến C3; byte thấp trước

CHARGE_COUNT là uint32, little-endian, đơn vị lần: C0 + 256 × C1 + 65536 × C2 + 16777216 × C3. Ví dụ 7B 00 00 00

tương ứng 123 lần sạc.

Bộ đếm tăng một lần khi BMS xác nhận hoàn tất một phiên sạc theo điều kiện kết thúc sạc trong firmware. Không tăng chỉ vì

kết nối bộ sạc; không tăng lặp khi duy trì trạng thái đầy. Đây là số phiên sạc hoàn tất, không phải bộ đếm chu kỳ sạc tương

đương theo dung lượng tích lũy.

Bộ đếm được lưu trong bộ nhớ không mất dữ liệu khi tắt nguồn. Lệnh F5 chỉ đọc, không xóa hoặc đặt lại bộ đếm. Khi đạt

0xFFFFFFFF, giữ nguyên giá trị tối đa, không quay về 0. Nếu chưa đọc được bộ đếm hợp lệ, BMS trả lỗi 04; giá trị 0 chỉ dùng

khi bộ đếm hợp lệ bằng 0.


<!-- Trang 6 -->

## 6. Trạng thái, bảo vệ và phản hồi lỗi

### 6.1. Byte STATUS của lệnh F0

Bit       Tên                              Giá trị 1

0       CHARGING                    Đang sạc

1       DISCHARGING                 Đang xả

2       CHG_MOS_ON             MOS sạc đang bật

3       DSG_MOS_ON             MOS xả đang bật

4       BALANCING                  Có cell đang cân bằng thực tế

5       BALANCE_AUTO_EN            Chế độ cân bằng tự động đã bật

6-7      RESERVED                 Dự phòng; BMS gửi bằng 0

### 6.2. Cờ lỗi bảo vệ 16 bit của lệnh F0

Bit        Điều kiện lỗi khi bit = 1

0        Quá áp cell

1        Thấp áp cell

2        Quá dòng sạc

3        Quá dòng xả

4        Quá nhiệt

5          Nhiệt độ thấp

6        Ngắn mạch

7           Lỗi cảm biến

8-15     Dự phòng; BMS gửi bằng 0

### 6.3. CMD 0xFF - Phản hồi lỗi yêu cầu

`AA | FF | 02 | CMD_REQ | ERROR | CRC_L | CRC_H`

Tổng 7 byte. CMD_REQ là mã lệnh Host đã gửi; ERROR là mã lỗi ở bảng dưới. FF chỉ dùng làm phản hồi, không phải mã

yêu cầu từ Host.

ERROR (hex)       Diễn giải                         Trường hợp điển hình

01               Không hỗ trợ lệnh             CMD chưa được định nghĩa

02                  Sai độ dài DATA                  A1 có LEN khác 01; F0/F2/F3/F4/F5 có LEN khác 00

03              Tham số ngoài phạm vi             CELL ngoài 01-0A; MODE khác 00/01

04               Bận hoặc chưa sẵn sàng           Chưa có dữ liệu/cấu hình hợp lệ hoặc chưa thể xử lý

Chỉ gửi FF sau khi nhận đủ khung có CRC hợp lệ. Khung thiếu hoặc CRC sai bị hủy. Khi xác định được nhiều lỗi trong yêu

cầu, kiểm tra theo thứ tự: mã lệnh, độ dài, tham số, trạng thái sẵn sàng.


<!-- Trang 7 -->

## 7. Bản tin mẫu và kiểm thử

Các bản tin dưới đây gồm đầy đủ HEADER và CRC đã tính. Dấu cách chỉ để trình bày; dữ liệu truyền là chuỗi byte nhị phân.

Tình huống                          Host → BMS                 BMS → Host

A1 - Cell 1 = 3,3 V               AA A1 01 01 E0 72              AA A1 03 01 E4 0C 83 58

F0 - SOC 80%, đang xả, hai MOS     AA F0 00 45 B0                AA F0 04 50 0E 00 00 95 E8

bật, không lỗi

F1 - Bật cân bằng tự động          AA F1 01 01 E0 63              AA F1 01 01 E0 63

F1 - Tắt cân bằng                AA F1 01 00 21 A3              AA F1 01 00 21 A3

F2 - Điện áp tổng 33 V            AA F2 00 44 D0                AA F2 04 E8 80 00 00 D0 81

F3 - Cấu hình 10 cell             AA F3 00 45 40                AA F3 01 0A 00 64

F4 - Nhiệt độ pin 25,5°C           AA F4 00 47 70                AA F4 02 FF 00 D2 24

F5 - Số lần sạc = 123             AA F5 00 46 E0                AA F5 04 7B 00 00 00 FD 9A

FF - A1 yêu cầu cell 11, ngoài phạm   AA A1 01 0B 60 75              AA FF 02 A1 03 A8 61

vi

### 7.1. Lưu ý xác nhận cân bằng

Khung yêu cầu và phản hồi F1 có thể giống hệt nhau. Host phải nhận phản hồi do BMS gửi; không dùng dữ liệu tự echo của

bộ chuyển đổi làm xác nhận thành công.

### 7.2. Tiêu chí kiểm thử giao thức

Kiểm thử                                     Kết quả yêu cầu

A1 lần lượt CELL = 01 đến 0A                  Trả đúng CELL, LEN = 03, điện áp mV và CRC hợp lệ

A1 có CELL = 00 hoặc 0B                   Phản hồi FF, CMD_REQ = A1, ERROR = 03

Sai LEN nhưng CRC đúng                  Phản hồi FF, ERROR = 02 với mã lệnh được hỗ trợ

Đổi một byte mà không sửa CRC           BMS bỏ khung; Host xử lý timeout

Byte AA xuất hiện trong DATA               Nhận theo LEN; không tách thành khung mới

F1 gửi lặp MODE = 01                    Chế độ vẫn bật; không đảo trạng thái

F3 với cấu hình hợp lệ 10 cell                  Trả AA F3 01 0A 00 64

F4 với 25,5°C và -10,0°C                 DATA lần lượt FF 00 và 9C FF; giải mã đúng int16

F5 với bộ đếm 123 lần                   LEN = 04, DATA = 7B 00 00 00; vẫn giữ sau khi tắt nguồn

### 7.3. Giới hạn phiên bản

Chưa có số thứ tự giao dịch, bản tin tự phát, lệnh ghi ngưỡng bảo vệ hoặc lệnh thay đổi số cell. Khi bổ sung hoặc thay đổi

định dạng, phải cập nhật phiên bản tài liệu và firmware của cả Host lẫn BMS.
