# Đếm chu kỳ xả tương đương

`BMS_UpdateUsageCounter()` đọc chênh lệch `DASTATUS6 (0x0076)` của BQ76952. Chỉ điện lượng **xả** được cộng vào phần dư Q32 nội bộ. Khi phần dư đạt `BMS_NOMINAL_CAPACITY_MAH`, firmware tăng `completedCycleCount` thêm 1 và giữ lại phần dư cho chu kỳ kế tiếp. Ví dụ pack 1000 mAh, xả 1200 mAh thành 1 chu kỳ hoàn tất và 200 mAh còn dư. Đây không phải số phiên sạc đầy/xả hết và không dùng để tính SOH.

Flash V3 lưu `completedCycleCount` (`uint32_t`) và phần dư Q32 (`uint64_t`), **không lưu tổng lượng sạc/xả suốt đời**. Phần dư là bắt buộc: chỉ lưu số nguyên chu kỳ sẽ làm mất tiến độ của chu kỳ chưa đủ khi tắt nguồn. Số chu kỳ cho biết điện lượng xả tương đương tối thiểu (`count × dung lượng danh định`), không thể suy ra chính xác lượng đã sạc hay phần xả lẻ chỉ từ `count`.

Phần dư được kiểm tra ghi flash khi tăng ít nhất 50 mAh và đã qua tối thiểu 1 phút MCU hoạt động; khi đủ thêm một chu kỳ, firmware ưu tiên ghi sớm để giữ bộ đếm. Trước SHUTDOWN do điện áp thấp, firmware cố lưu ngay. Flash có hai bank chứa bản ghi nối tiếp và checksum. Bản ghi V2 cùng kích thước vẫn được đọc và chuyển thành `count + phần dư` khi khởi động; nếu lịch sử cũ chỉ có tổng sạc, lượng xả trước đó chỉ được ước lượng.

Giới hạn: BQ báo điện lượng **thuần** giữa hai lần đọc. Nạp và xả đổi chiều nhiều lần trong cùng khoảng đọc có thể triệt tiêu nhau, kể cả khoảng SLEEP giữa hai lần thức RTC (mặc định 5 phút). RESET_PASSQ hoặc reset BQ làm mất mốc tích lũy; firmware đặt lại mốc nhưng không thể phục hồi điện lượng đã mất. Mất nguồn đột ngột có thể mất phần dư chưa lưu. Cần đặt `BMS_NOMINAL_CAPACITY_MAH` đúng dung lượng danh định của pack và kiểm tra dấu dòng trên phần cứng.

Việc hiệu chuẩn lại dòng cũng đặt lại mốc so sánh PASSQ sau khi đổi `Capacity Gain`, để không nhầm thay đổi hệ số hiệu chuẩn thành điện lượng đi qua pin.
